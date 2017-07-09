/*
 * Wiimote-Pad
 *
 * Copyright (C) 2013 Giuseppe Bilotta
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <libudev.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include <sys/select.h>
#include <fcntl.h>
#include <glob.h>
#include <errno.h>
#include <signal.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <xwiimote.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

/* These definitions might be missing from older linux/input.h headers */
#ifndef BTN_DPAD_LEFT
#define BTN_DPAD_LEFT 0x222
#endif
#ifndef BTN_DPAD_RIGHT
#define BTN_DPAD_RIGHT 0x223
#endif
#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#endif
#ifndef BTN_DPAD_DOWN
#define BTN_DPAD_DOWN 0x221
#endif

#define WIIMOTE_PADMODE_VENDOR_ID 0x6181 /* GIuseppe BIlotta */
#define WIIMOTE_PADMODE_DEVICE_ID 0x3169 /* WIimote GamePad */

/* Check if an error \code is negative during \str action */
void err_check(int code, char const *str) {
    if (code < 0) {
        int err = errno;
        fprintf(stderr, "could not %s (%d): %s\n",
                str, err, strerror(err));
        exit(-err);
    }
}

#define BUTTONS do { \
    _BUTTON(XWII_KEY_LEFT, BTN_DPAD_LEFT); \
    _BUTTON(XWII_KEY_RIGHT, BTN_DPAD_RIGHT); \
    _BUTTON(XWII_KEY_UP, BTN_DPAD_UP); \
    _BUTTON(XWII_KEY_DOWN, BTN_DPAD_DOWN); \
    _BUTTON(XWII_KEY_A, BTN_A); \
    _BUTTON(XWII_KEY_B, BTN_B); \
    _BUTTON(XWII_KEY_PLUS, BTN_TL); \
    _BUTTON(XWII_KEY_MINUS, BTN_TR); \
    _BUTTON(XWII_KEY_HOME, BTN_MODE); \
    _BUTTON(XWII_KEY_ONE, BTN_1); \
    _BUTTON(XWII_KEY_TWO, BTN_2); \
    _BUTTON(XWII_KEY_X, BTN_3); \
    _BUTTON(XWII_KEY_Y, BTN_4); \
    _BUTTON(XWII_KEY_TL, BTN_5); \
    _BUTTON(XWII_KEY_TR, BTN_6); \
    _BUTTON(XWII_KEY_ZL, BTN_7); \
    _BUTTON(XWII_KEY_ZR, BTN_8); \
} while (0)


struct uinput_user_dev padmode;

/* controller keys + pro/classic buttons + two axes (wiimote acc) + 6 axes (classic), plus SYN */
#define MAXINPUTS XWII_KEY_ZR + 2 + 6 + 1 + 1

/* Room for events */
struct input_event iev[MAXINPUTS];

/* macros to set evbits and keybits */
#define set_ev(key) do { \
    ret = ioctl(fd, UI_SET_EVBIT, key); \
    err_check(ret, "set " #key); \
} while(0)

#define set_key(key) do { \
    ret = ioctl(fd, UI_SET_KEYBIT, key); \
    err_check(ret, "set " #key); \
} while(0)

#define set_abs(key, min, max, fuzz, flat) do { \
    ret = ioctl(fd, UI_SET_ABSBIT, key); \
    err_check(ret, "set " #key); \
    padmode.absmin[key] = min; \
    padmode.absmax[key] = max; \
    padmode.absfuzz[key] = fuzz; \
    padmode.absflat[key] = flat; \
} while(0)

#define AXIS_MAX 100

#define AXIS_ANALOG_MAX 24
#define AXIS_ANALOG_MIN -25

struct wiimote_dev {
    const char *device;
    char *root;

    int dev_id;

    int uinput;

    struct xwii_iface *iface;
    unsigned int ifs;
    int fd;
};

#define MAX_WIIMOTES 1
struct wiimote_dev dev[MAX_WIIMOTES];

void dev_init(struct wiimote_dev const *dev) {
    int ret;
    int fd = dev->uinput;

#define _BUTTON(n, bt) do { \
    set_key(bt); \
    iev[n].type = EV_KEY; \
    iev[n].code = bt; \
} while (0)
    set_ev(EV_KEY);
    BUTTONS;
    set_ev(EV_SYN);
#undef _BUTTON

    set_ev(EV_ABS);
    
    set_abs(ABS_HAT0X, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 9].type = EV_ABS;
    iev[MAXINPUTS - 9].code = ABS_HAT0X;
    
    set_abs(ABS_HAT0Y, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 8].type = EV_ABS;
    iev[MAXINPUTS - 8].code = ABS_HAT0Y;
    
    set_abs(ABS_HAT1X, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 7].type = EV_ABS;
    iev[MAXINPUTS - 7].code = ABS_HAT1X;
    
    set_abs(ABS_HAT1Y, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 6].type = EV_ABS;
    iev[MAXINPUTS - 6].code = ABS_HAT1Y;
    
    set_abs(ABS_HAT2X, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 5].type = EV_ABS;
    iev[MAXINPUTS - 5].code = ABS_HAT2X;
    
    set_abs(ABS_HAT2Y, AXIS_ANALOG_MIN, AXIS_ANALOG_MAX, 2, 4);
    iev[MAXINPUTS - 4].type = EV_ABS;
    iev[MAXINPUTS - 4].code = ABS_HAT2Y;
    
    set_abs(ABS_X, -AXIS_MAX, AXIS_MAX, 2, 4);
    iev[MAXINPUTS - 3].type = EV_ABS;
    iev[MAXINPUTS - 3].code = ABS_X;
    
    set_abs(ABS_Y, -AXIS_MAX, AXIS_MAX, 2, 4);
    iev[MAXINPUTS - 2].type = EV_ABS;
    iev[MAXINPUTS - 2].code = ABS_Y;

    iev[MAXINPUTS - 1].type = EV_SYN;
    iev[MAXINPUTS - 1].code = iev[MAXINPUTS - 1].value = 0;

    snprintf(padmode.name, UINPUT_MAX_NAME_SIZE, XWII_NAME_CORE " in gamepad mode");
    padmode.id.bustype = BUS_VIRTUAL;
    /*
    padmode.id.vendor = 0;
    padmode.id.product = 0;
    padmode.id.version = 0;
     */

    ret = write(fd, &padmode, sizeof (padmode));
    err_check(ret, "set dev properties");
    ret = ioctl(fd, UI_DEV_CREATE);
    err_check(ret, "create device");
}

static int wiimote_refresh(struct wiimote_dev *dev) {
    puts("Refreshing\n");
    return xwii_iface_open(dev->iface, dev->ifs);
}

static void wiimote_key(struct wiimote_dev *dev, struct xwii_event const *ev) {
    unsigned int code;
    unsigned int state;

    code = ev->v.key.code;
    state = ev->v.key.state;

    if (code > XWII_KEY_ZR)
        return;
    if (state > 1)
        return;
    iev[code].value = state;

    if (dev->uinput > 0) {
        int ret = write(dev->uinput, iev + code, sizeof (*iev));
        err_check(ret, "report button");
        ret = write(dev->uinput, iev + 13, sizeof (*iev));
        err_check(ret, "report btn SYN");
    } else {
        fputs("nowhere to report butto presses to\n", stderr);
    }
}

#define CLIP_AXIS(val) do { \
    if (val < -AXIS_MAX) \
        val = -AXIS_MAX; \
    if (val > AXIS_MAX) \
        val = AXIS_MAX; \
} while (0)

static void wiimote_axis(struct wiimote_dev *dev, struct xwii_event const *ev) {
    iev[MAXINPUTS - 9].value = -(ev->v.abs[0].x);
    iev[MAXINPUTS - 8].value = -(ev->v.abs[0].y);
    iev[MAXINPUTS - 7].value = -(ev->v.abs[1].x);
    iev[MAXINPUTS - 6].value = -(ev->v.abs[1].y);
    iev[MAXINPUTS - 5].value = -(ev->v.abs[2].x);
    iev[MAXINPUTS - 4].value = -(ev->v.abs[2].y);

    CLIP_AXIS(iev[MAXINPUTS - 9].value);
    CLIP_AXIS(iev[MAXINPUTS - 8].value);
    CLIP_AXIS(iev[MAXINPUTS - 7].value);
    CLIP_AXIS(iev[MAXINPUTS - 6].value);
    CLIP_AXIS(iev[MAXINPUTS - 5].value);
    CLIP_AXIS(iev[MAXINPUTS - 4].value);

    if (dev->uinput > 0) {
        int ret = write(dev->uinput, iev + MAXINPUTS - 9, sizeof (*iev));
        err_check(ret, "report accel X");
        ret = write(dev->uinput, iev + MAXINPUTS - 8, sizeof (*iev));
        err_check(ret, "report accel Y");
        ret = write(dev->uinput, iev + MAXINPUTS - 7, sizeof (*iev));
        err_check(ret, "report accel X");
        ret = write(dev->uinput, iev + MAXINPUTS - 6, sizeof (*iev));
        err_check(ret, "report accel Y");
        ret = write(dev->uinput, iev + MAXINPUTS - 5, sizeof (*iev));
        err_check(ret, "report accel X");
        ret = write(dev->uinput, iev + MAXINPUTS - 4, sizeof (*iev));
        err_check(ret, "report accel Y");
        ret = write(dev->uinput, iev + MAXINPUTS - 1, sizeof (*iev));
        err_check(ret, "report accel SYN");
#if 0
        printf("reported J (%d, %d) from ev (%d, %d)\n",
                iev[11].value, iev[12].value,
                -(ev->v.abs[0].y), ev->v.abs[0].x);
#endif
    } else {
        fputs("nowhere to report accel to\n", stderr);
    }
}

static void wiimote_accel(struct wiimote_dev *dev, struct xwii_event const *ev) {
    iev[MAXINPUTS - 3].value = -(ev->v.abs[0].y);
    iev[MAXINPUTS - 2].value = -(ev->v.abs[0].x);

    CLIP_AXIS(iev[MAXINPUTS - 3].value);
    CLIP_AXIS(iev[MAXINPUTS - 2].value);

    if (dev->uinput > 0) {
        int ret = write(dev->uinput, iev + MAXINPUTS - 3, sizeof (*iev));
        err_check(ret, "report accel X");
        ret = write(dev->uinput, iev + MAXINPUTS - 2, sizeof (*iev));
        err_check(ret, "report accel Y");
        ret = write(dev->uinput, iev + MAXINPUTS - 1, sizeof (*iev));
        err_check(ret, "report accel SYN");
#if 0
        printf("reported J (%d, %d) from ev (%d, %d)\n",
                iev[11].value, iev[12].value,
                -(ev->v.abs[0].y), ev->v.abs[0].x);
#endif
    } else {
        fputs("nowhere to report accel to\n", stderr);
    }
}

static int wiimote_poll(struct wiimote_dev *dev) {
    struct xwii_event ev;
    int ret;

    do {
        memset(&ev, 0, sizeof (ev));
        ret = xwii_iface_dispatch(dev->iface, &ev, sizeof (ev));

        if (ret)
            break;

        switch (ev.type) {
            case XWII_EVENT_WATCH:
                ret = wiimote_refresh(dev);
                break;
            case XWII_EVENT_KEY:
                wiimote_key(dev, &ev);
                break;
            case XWII_EVENT_CLASSIC_CONTROLLER_KEY:
                wiimote_key(dev, &ev);
                break;
            case XWII_EVENT_CLASSIC_CONTROLLER_MOVE:
                wiimote_axis(dev, &ev);
                break;
            case XWII_EVENT_ACCEL:
                wiimote_accel(dev, &ev);
                break;
            default:
                printf("Unhandled Wiimote event type %u\n", ev.type);
        }
    } while (!ret);

    if (ret == -EAGAIN) {
        ret = 0;
    }

    return ret;
}

int dev_create(struct wiimote_dev *dev) {
    int ret = 0;
    struct udev *udev;
    struct udev_device *d, *p;
    struct stat st;
    const char *root, *snum, *driver, *subs;
    int num;
    unsigned int openedInterfaces;

    if (!dev->device) {
        ret = EINVAL;
        goto exit;
    }

    if (stat(dev->device, &st)) {
        ret = errno;
        goto exit;
    }

    udev = udev_new();
    if (!udev) {
        fputs("could not connect to udev\n", stderr);
        ret = errno;
        goto exit;
    }

    d = udev_device_new_from_devnum(udev, 'c', st.st_rdev);
    if (!d) {
        fputs("could not find udev device\n", stderr);
        ret = errno;
        goto exit_udev;
    }

    p = udev_device_get_parent_with_subsystem_devtype(d, "hid", NULL);
    if (!p) {
        fputs("could not find parent HID device\n", stderr);
        ret = errno;
        goto exit_dev;
    }

    driver = udev_device_get_driver(p);
    subs = udev_device_get_subsystem(p);
    if (!driver || strcmp(driver, "wiimote") || !subs || strcmp(subs, "hid")) {
        fputs("parent is not a HID Wiimote\n", stderr);
        ret = errno;
        goto exit_dev;
    }

    root = udev_device_get_syspath(p);
    snum = udev_device_get_sysname(p);
    snum = snum ? strchr(snum, '.') : NULL;
    if (!root || !snum) {
        fputs("Could not get root path\n", stderr);
        ret = errno;
        goto exit_dev;
    }

    num = strtol(&snum[1], NULL, 16);
    if (num < 0) {
        fputs("Negative device number!\n", stderr);
        ret = errno;
        goto exit_dev;
    }
    dev->dev_id = num;

    dev->root = strdup(root);
    
    if (!dev->root) {
        fputs("Could not set device root\n", stderr);
        ret = errno;
        goto exit_dev;
    }

    printf("using device %d from root %s for %s\n", dev->dev_id, dev->root, dev->device);

    ret = xwii_iface_new(&dev->iface, dev->root);
    if (ret) {
        fputs("Could not create xwiimote interface\n", stderr);
        ret = errno;
        goto exit_wii;
    }
    
    dev->ifs = xwii_iface_available(dev->iface);

    ret = xwii_iface_open(dev->iface, dev->ifs);
    
    if (ret) {
        fputs("Could not open xwiimote interface\n", stderr);
        ret = errno;
        goto exit_wii;
    }

    if ((openedInterfaces = xwii_iface_opened(dev->iface)) != dev->ifs) {
        fputs("Some interfaces failed to open\n", stderr);
        ret = errno;
        goto exit_wii;
    }
    
    if (openedInterfaces & XWII_IFACE_CLASSIC_CONTROLLER) {
        printf("Info: Enabled Classic Controller\n");
    }

    dev->fd = xwii_iface_get_fd(dev->iface);

    goto exit_dev;

exit_wii:
    free(dev->root);
    dev->root = NULL;

exit_dev:
    udev_device_unref(d);
exit_udev:
    udev_unref(udev);
exit:
    return ret;
}

/**
 * Deassociates a device to stop using it.
 * @param dev
 */
static void dev_destroy(struct wiimote_dev *dev) {
    if (dev->root) {
        xwii_iface_unref(dev->iface);
        free(dev->root);
        dev->root = NULL;
    }

    if (dev->uinput > 0) {
        ioctl(dev->uinput, UI_DEV_DESTROY);
        close(dev->uinput);
    }

    printf("deassociated from device %s\n", dev->device);
}

glob_t js_devs;

/**
 * Deassociates from all devices.
 */
static void destroy_all_devs(void) {
#if MAX_WIIMOTES > 1
#error "destroy_all_devs only works for a single device"
#endif
    // Destroy the only device
    dev_destroy(dev);

    // Free the memory
    globfree(&js_devs);
}

static int last_signal;

static void sig_exit(int _signal) {
    last_signal = _signal;
    printf("Interrupted by signal %d\n", last_signal);
}

struct timeval no_wait;

const char uinput_path[] = "/dev/uinput";
const char js_glob[] = "/dev/input/js*";

/**
 * The main method of the application.
 * @param argc
 * @param argv
 * @return 
 */
int main(int argc, char *argv[]) {
    int ret;
    size_t j;
    fd_set fds[1];

    // Deassociate from all devices at exit
    atexit(destroy_all_devs);

    // Add Ctrl+c handling
    signal(SIGINT, sig_exit);

    // Determine which device we will use
    if (argc > 1) {

        // A device given by argument
        dev->device = argv[1];
        ret = dev_create(dev);

        if (ret) {
            fprintf(stderr, "could not %s (%d): %s\n",
                    "associate", ret, strerror(ret));
            return ret;
        }
    } else {
        /* No device specified. Since the Linux kernel exposes the
         * controller also as a joystick (without axes), we peek at
         * all available joysticks looking for one which is a Wiimote
         */
        switch (glob(js_glob, GLOB_NOSORT, NULL, &js_devs)) {
            case GLOB_ABORTED:
            case GLOB_NOMATCH:
            case GLOB_NOSPACE:
                fputs("no joysticks found\n", stderr);
                exit(ENODEV);
        }

        for (j = 0; j < js_devs.gl_pathc; ++j) {
            dev->device = js_devs.gl_pathv[j];
            ret = dev_create(dev);
            if (!ret)
                break; /* found */
            printf("skipping %s (%d): %s\n",
                    dev->device, ret, strerror(ret));
            dev->device = NULL;
        }
        
        if (!dev->device) {
            fputs("no wiimote found\n", stderr);
            exit(ENODEV);
        }
    }

    // Open uinput device for new joystick
    dev->uinput = open(uinput_path, O_WRONLY | O_NONBLOCK);
    err_check(dev->uinput, "open uinput");

    dev_init(dev);

    do {
        memset(&no_wait, 0, sizeof (no_wait));
        FD_ZERO(fds);
        FD_SET(dev->fd, fds);

        if (last_signal) {
            break;
        }

        ret = select(dev->fd + 1, fds, NULL, NULL, NULL);

        err_check(ret, "poll wiimote fd");

        if (ret > 0) {
            ret = wiimote_poll(dev);
            err_check(ret, "process wiimote data");
        }
    } while (1);

    return ret;
}

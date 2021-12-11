/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file defines a wrapper around libudev so we can avoid
 * linking directly to it and use dlopen instead.
 */

#ifndef HAL_LINUX_UDEV_H_
#define HAL_LINUX_UDEV_H_

#include <dlfcn.h>

#include "mozilla/ArrayUtils.h"

namespace mozilla {

struct udev;
struct udev_device;
struct udev_enumerate;
struct udev_list_entry;
struct udev_monitor;

class udev_lib {
 public:
  udev_lib() : lib(nullptr), udev(nullptr) {
    // Be careful about ABI compat! 0 -> 1 didn't change any
    // symbols this code relies on, per:
    // https://lists.fedoraproject.org/pipermail/devel/2012-June/168227.html
    const char* lib_names[] = {"libudev.so.0", "libudev.so.1"};
    // Check whether a library is already loaded so we don't load two
    // conflicting libs.
    for (unsigned i = 0; i < ArrayLength(lib_names); i++) {
      lib = dlopen(lib_names[i], RTLD_NOLOAD | RTLD_LAZY | RTLD_GLOBAL);
      if (lib) {
        break;
      }
    }
    // If nothing loads the first time through, it means no version of libudev
    // was already loaded.
    if (!lib) {
      for (unsigned i = 0; i < ArrayLength(lib_names); i++) {
        lib = dlopen(lib_names[i], RTLD_LAZY | RTLD_GLOBAL);
        if (lib) {
          break;
        }
      }
    }
    if (lib && LoadSymbols()) {
      udev = udev_new();
    }
  }

  ~udev_lib() {
    if (udev) {
      udev_unref(udev);
    }

    if (lib) {
      dlclose(lib);
    }
  }

  explicit operator bool() { return udev; }

 private:
#define DLSYM(s)                       \
  do {                                 \
    (s) = (decltype(s))dlsym(lib, #s); \
    if (!(s)) return false;            \
  } while (0)

  bool LoadSymbols() {
    DLSYM(udev_new);
    DLSYM(udev_unref);
    DLSYM(udev_device_unref);
    DLSYM(udev_device_new_from_syspath);
    DLSYM(udev_device_get_devnode);
    DLSYM(udev_device_get_parent_with_subsystem_devtype);
    DLSYM(udev_device_get_property_value);
    DLSYM(udev_device_get_action);
    DLSYM(udev_device_get_sysattr_value);
    DLSYM(udev_enumerate_new);
    DLSYM(udev_enumerate_unref);
    DLSYM(udev_enumerate_add_match_subsystem);
    DLSYM(udev_enumerate_scan_devices);
    DLSYM(udev_enumerate_get_list_entry);
    DLSYM(udev_list_entry_get_next);
    DLSYM(udev_list_entry_get_name);
    DLSYM(udev_monitor_new_from_netlink);
    DLSYM(udev_monitor_filter_add_match_subsystem_devtype);
    DLSYM(udev_monitor_enable_receiving);
    DLSYM(udev_monitor_get_fd);
    DLSYM(udev_monitor_receive_device);
    DLSYM(udev_monitor_unref);

    return true;
  }

#undef DLSYM

  void* lib;

 public:
  struct udev* udev;

  // Function pointers returned from dlsym.
  struct udev* (*udev_new)(void);
  void (*udev_unref)(struct udev*);

  void (*udev_device_unref)(struct udev_device*);
  struct udev_device* (*udev_device_new_from_syspath)(struct udev*,
                                                      const char*);
  const char* (*udev_device_get_devnode)(struct udev_device*);
  struct udev_device* (*udev_device_get_parent_with_subsystem_devtype)(
      struct udev_device*, const char*, const char*);
  const char* (*udev_device_get_property_value)(struct udev_device*,
                                                const char*);
  const char* (*udev_device_get_action)(struct udev_device*);
  const char* (*udev_device_get_sysattr_value)(struct udev_device*,
                                               const char*);

  struct udev_enumerate* (*udev_enumerate_new)(struct udev*);
  void (*udev_enumerate_unref)(struct udev_enumerate*);
  int (*udev_enumerate_add_match_subsystem)(struct udev_enumerate*,
                                            const char*);
  int (*udev_enumerate_scan_devices)(struct udev_enumerate*);
  struct udev_list_entry* (*udev_enumerate_get_list_entry)(
      struct udev_enumerate*);

  struct udev_list_entry* (*udev_list_entry_get_next)(struct udev_list_entry*);
  const char* (*udev_list_entry_get_name)(struct udev_list_entry*);

  struct udev_monitor* (*udev_monitor_new_from_netlink)(struct udev*,
                                                        const char*);
  int (*udev_monitor_filter_add_match_subsystem_devtype)(struct udev_monitor*,
                                                         const char*,
                                                         const char*);
  int (*udev_monitor_enable_receiving)(struct udev_monitor*);
  int (*udev_monitor_get_fd)(struct udev_monitor*);
  struct udev_device* (*udev_monitor_receive_device)(struct udev_monitor*);
  void (*udev_monitor_unref)(struct udev_monitor*);
};

}  // namespace mozilla

#endif  // HAL_LINUX_UDEV_H_

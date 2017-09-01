/* -*- Mode: rust; rust-indent-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]

#[macro_use]
extern crate lazy_static;
extern crate libc;

use libc::{c_void,c_int,c_char,c_ulonglong,dev_t};
use libc::{RTLD_GLOBAL,RTLD_LAZY,RTLD_NOLOAD};
use libc::{dlopen,dlclose,dlsym};
use std::ffi::CString;
use std::{marker,mem,ops,ptr};

#[repr(C)]
pub struct udev {
  __private: c_void
}

#[repr(C)]
pub struct udev_list_entry {
  __private: c_void
}

#[repr(C)]
pub struct udev_device {
  __private: c_void
}

#[repr(C)]
pub struct udev_monitor {
  __private: c_void
}

#[repr(C)]
pub struct udev_enumerate {
  __private: c_void
}

macro_rules! ifnull {
  ($a:expr, $b:expr) => {
    if $a.is_null() { $b } else { $a }
  }
}

struct Library(*mut c_void);

impl Library {
  fn open(name: &'static str) -> Library {
    let flags = RTLD_LAZY | RTLD_GLOBAL;
    let flags_noload = flags | RTLD_NOLOAD;
    let name = CString::new(name).unwrap();
    let name = name.as_ptr();

    Library(unsafe {
      ifnull!(dlopen(name, flags_noload), dlopen(name, flags))
    })
  }

  fn get(&self, name: &'static str) -> *mut c_void {
    let name = CString::new(name).unwrap();
    unsafe { dlsym(self.0, name.as_ptr()) }
  }
}

impl Drop for Library {
  fn drop(&mut self) {
    unsafe { dlclose(self.0); }
  }
}

unsafe impl Sync for Library {}

lazy_static! {
  static ref LIBRARY: Library = {
    Library::open("libudev.so.1")
  };
}

pub struct Symbol<T> {
  ptr: *mut c_void,
  pd: marker::PhantomData<T>
}

impl<T> Symbol<T> {
  fn new(ptr: *mut c_void) -> Self {
    let default = Self::default as *mut c_void;
    Self { ptr: ifnull!(ptr, default), pd: marker::PhantomData }
  }

  // This is the default symbol, used whenever dlopen() fails.
  // Users of this library are expected to check whether udev_new() returns
  // a nullptr, and if so they MUST NOT call any other exported functions.
  extern "C" fn default() -> *mut c_void {
    ptr::null_mut()
  }
}

impl<T> ops::Deref for Symbol<T> {
  type Target = T;

  fn deref(&self) -> &T {
    unsafe { mem::transmute(&self.ptr) }
  }
}

unsafe impl<T: Sync> Sync for Symbol<T> {}

macro_rules! define {
  ($name:ident, $type:ty) => {
    lazy_static! {
      pub static ref $name : Symbol<$type> = {
        Symbol::new(LIBRARY.get(stringify!($name)))
      };
    }
  };
}

// udev
define!(udev_new, extern "C" fn () -> *mut udev);
define!(udev_unref, extern "C" fn (*mut udev) -> *mut udev);

// udev_list
define!(udev_list_entry_get_next, extern "C" fn (*mut udev_list_entry) -> *mut udev_list_entry);
define!(udev_list_entry_get_name, extern "C" fn (*mut udev_list_entry) -> *const c_char);
define!(udev_list_entry_get_value, extern "C" fn (*mut udev_list_entry) -> *const c_char);

// udev_device
define!(udev_device_ref, extern "C" fn (*mut udev_device) -> *mut udev_device);
define!(udev_device_unref, extern "C" fn (*mut udev_device) -> *mut udev_device);
define!(udev_device_new_from_syspath, extern "C" fn (*mut udev, *const c_char) -> *mut udev_device);
define!(udev_device_get_parent, extern "C" fn (*mut udev_device) -> *mut udev_device);
define!(udev_device_get_devpath, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_subsystem, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_devtype, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_syspath, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_sysname, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_sysnum, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_devnode, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_is_initialized, extern "C" fn (*mut udev_device) -> c_int);
define!(udev_device_get_properties_list_entry, extern "C" fn (*mut udev_device) -> *mut udev_list_entry);
define!(udev_device_get_property_value, extern "C" fn (*mut udev_device, *const c_char) -> *const c_char);
define!(udev_device_get_driver, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_devnum, extern "C" fn (*mut udev_device) -> dev_t);
define!(udev_device_get_action, extern "C" fn (*mut udev_device) -> *const c_char);
define!(udev_device_get_sysattr_value, extern "C" fn (*mut udev_device, *const c_char) -> *const c_char);
define!(udev_device_set_sysattr_value, extern "C" fn (*mut udev_device, *const c_char, *mut c_char) -> c_int);
define!(udev_device_get_sysattr_list_entry, extern "C" fn (*mut udev_device) -> *mut udev_list_entry);
define!(udev_device_get_seqnum, extern "C" fn (*mut udev_device) -> c_ulonglong);

// udev_monitor
define!(udev_monitor_ref, extern "C" fn (*mut udev_monitor) -> *mut udev_monitor);
define!(udev_monitor_unref, extern "C" fn (*mut udev_monitor) -> *mut udev_monitor);
define!(udev_monitor_new_from_netlink, extern "C" fn (*mut udev, *const c_char) -> *mut udev_monitor);
define!(udev_monitor_enable_receiving, extern "C" fn (*mut udev_monitor) -> c_int);
define!(udev_monitor_get_fd, extern "C" fn (*mut udev_monitor) -> c_int);
define!(udev_monitor_receive_device, extern "C" fn (*mut udev_monitor) -> *mut udev_device);
define!(udev_monitor_filter_add_match_subsystem_devtype, extern "C" fn (*mut udev_monitor, *const c_char, *const c_char) -> c_int);
define!(udev_monitor_filter_add_match_tag, extern "C" fn (*mut udev_monitor, *const c_char) -> c_int);
define!(udev_monitor_filter_remove, extern "C" fn (*mut udev_monitor) -> c_int);

// udev_enumerate
define!(udev_enumerate_unref, extern "C" fn (*mut udev_enumerate) -> *mut udev_enumerate);
define!(udev_enumerate_new, extern "C" fn (*mut udev) -> *mut udev_enumerate);
define!(udev_enumerate_add_match_subsystem, extern "C" fn (*mut udev_enumerate, *const c_char) -> c_int);
define!(udev_enumerate_add_nomatch_subsystem, extern "C" fn (*mut udev_enumerate, *const c_char) -> c_int);
define!(udev_enumerate_add_match_sysattr, extern "C" fn (*mut udev_enumerate, *const c_char, *const c_char) -> c_int);
define!(udev_enumerate_add_nomatch_sysattr, extern "C" fn (*mut udev_enumerate, *const c_char, *const c_char) -> c_int);
define!(udev_enumerate_add_match_property, extern "C" fn (*mut udev_enumerate, *const c_char, *const c_char) -> c_int);
define!(udev_enumerate_add_match_tag, extern "C" fn (*mut udev_enumerate, *const c_char) -> c_int);
define!(udev_enumerate_add_match_parent, extern "C" fn (*mut udev_enumerate, *mut udev_device) -> c_int);
define!(udev_enumerate_add_match_is_initialized, extern "C" fn (*mut udev_enumerate) -> c_int);
define!(udev_enumerate_add_match_sysname, extern "C" fn (*mut udev_enumerate, *const c_char) -> c_int);
define!(udev_enumerate_add_syspath, extern "C" fn (*mut udev_enumerate, *const c_char) -> c_int);
define!(udev_enumerate_scan_devices, extern "C" fn (*mut udev_enumerate) -> c_int);
define!(udev_enumerate_get_list_entry, extern "C" fn (*mut udev_enumerate) -> *mut udev_list_entry);

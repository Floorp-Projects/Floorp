/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use std::mem;
use std::ptr;
use std::slice;

use std::ffi::OsString;
use std::os::windows::ffi::OsStringExt;

use util::io_err;

extern crate libc;
extern crate winapi;
use self::winapi::*;

#[link(name = "setupapi")]
extern "stdcall" {
    fn SetupDiGetClassDevsW(
        ClassGuid: *const GUID,
        Enumerator: PCSTR,
        hwndParent: HWND,
        flags: DWORD,
    ) -> HDEVINFO;

    fn SetupDiDestroyDeviceInfoList(DeviceInfoSet: HDEVINFO) -> BOOL;

    fn SetupDiEnumDeviceInterfaces(
        DeviceInfoSet: HDEVINFO,
        DeviceInfoData: PSP_DEVINFO_DATA,
        InterfaceClassGuid: *const GUID,
        MemberIndex: DWORD,
        DeviceInterfaceData: PSP_DEVICE_INTERFACE_DATA,
    ) -> BOOL;

    fn SetupDiGetDeviceInterfaceDetailW(
        DeviceInfoSet: HDEVINFO,
        DeviceInterfaceData: PSP_DEVICE_INTERFACE_DATA,
        DeviceInterfaceDetailData: PSP_DEVICE_INTERFACE_DETAIL_DATA_W,
        DeviceInterfaceDetailDataSize: DWORD,
        RequiredSize: PDWORD,
        DeviceInfoData: PSP_DEVINFO_DATA,
    ) -> BOOL;
}

#[link(name = "hid")]
extern "stdcall" {
    fn HidD_GetPreparsedData(
        HidDeviceObject: HANDLE,
        PreparsedData: *mut PHIDP_PREPARSED_DATA,
    ) -> BOOLEAN;

    fn HidD_FreePreparsedData(PreparsedData: PHIDP_PREPARSED_DATA) -> BOOLEAN;

    fn HidP_GetCaps(PreparsedData: PHIDP_PREPARSED_DATA, Capabilities: PHIDP_CAPS) -> NTSTATUS;
}

macro_rules! offset_of {
    ($ty:ty, $field:ident) => {
        unsafe { &(*(0 as *const $ty)).$field as *const _ as usize }
    }
}

fn from_wide_ptr(ptr: *const u16, len: usize) -> String {
    assert!(!ptr.is_null() && len % 2 == 0);
    let slice = unsafe { slice::from_raw_parts(ptr, len / 2) };
    OsString::from_wide(slice).to_string_lossy().into_owned()
}

pub struct DeviceInfoSet {
    set: HDEVINFO,
}

impl DeviceInfoSet {
    pub fn new() -> io::Result<Self> {
        let flags = DIGCF_PRESENT | DIGCF_DEVICEINTERFACE;
        let set = unsafe {
            SetupDiGetClassDevsW(
                &GUID_DEVINTERFACE_HID,
                ptr::null_mut(),
                ptr::null_mut(),
                flags,
            )
        };
        if set == INVALID_HANDLE_VALUE {
            return Err(io_err("SetupDiGetClassDevsW failed!"));
        }

        Ok(Self { set })
    }

    pub fn get(&self) -> HDEVINFO {
        self.set
    }

    pub fn devices(&self) -> DeviceInfoSetIter {
        DeviceInfoSetIter::new(self)
    }
}

impl Drop for DeviceInfoSet {
    fn drop(&mut self) {
        let _ = unsafe { SetupDiDestroyDeviceInfoList(self.set) };
    }
}

pub struct DeviceInfoSetIter<'a> {
    set: &'a DeviceInfoSet,
    index: DWORD,
}

impl<'a> DeviceInfoSetIter<'a> {
    fn new(set: &'a DeviceInfoSet) -> Self {
        Self { set, index: 0 }
    }
}

impl<'a> Iterator for DeviceInfoSetIter<'a> {
    type Item = String;

    fn next(&mut self) -> Option<Self::Item> {
        let mut device_interface_data = unsafe { mem::uninitialized::<SP_DEVICE_INTERFACE_DATA>() };
        device_interface_data.cbSize = mem::size_of::<SP_DEVICE_INTERFACE_DATA>() as UINT;

        let rv = unsafe {
            SetupDiEnumDeviceInterfaces(
                self.set.get(),
                ptr::null_mut(),
                &GUID_DEVINTERFACE_HID,
                self.index,
                &mut device_interface_data,
            )
        };
        if rv == 0 {
            return None; // We're past the last device index.
        }

        // Determine the size required to hold a detail struct.
        let mut required_size = 0;
        unsafe {
            SetupDiGetDeviceInterfaceDetailW(
                self.set.get(),
                &mut device_interface_data,
                ptr::null_mut(),
                required_size,
                &mut required_size,
                ptr::null_mut(),
            )
        };
        if required_size == 0 {
            return None; // An error occurred.
        }

        let detail = DeviceInterfaceDetailData::new(required_size as usize);
        if detail.is_none() {
            return None; // malloc() failed.
        }

        let detail = detail.unwrap();
        let rv = unsafe {
            SetupDiGetDeviceInterfaceDetailW(
                self.set.get(),
                &mut device_interface_data,
                detail.get(),
                required_size,
                ptr::null_mut(),
                ptr::null_mut(),
            )
        };
        if rv == 0 {
            return None; // An error occurred.
        }

        self.index += 1;
        Some(detail.path())
    }
}

struct DeviceInterfaceDetailData {
    data: PSP_DEVICE_INTERFACE_DETAIL_DATA_W,
    path_len: usize,
}

impl DeviceInterfaceDetailData {
    fn new(size: usize) -> Option<Self> {
        let mut cb_size = mem::size_of::<SP_DEVICE_INTERFACE_DETAIL_DATA_W>();
        if cfg!(target_pointer_width = "32") {
            cb_size = 4 + 2; // 4-byte uint + default TCHAR size. size_of is inaccurate.
        }

        if size < cb_size {
            warn!("DeviceInterfaceDetailData is too small. {}", size);
            return None;
        }

        let mut data = unsafe { libc::malloc(size) as PSP_DEVICE_INTERFACE_DETAIL_DATA_W };
        if data.is_null() {
            return None;
        }

        // Set total size of the structure.
        unsafe { (*data).cbSize = cb_size as UINT };

        // Compute offset of `SP_DEVICE_INTERFACE_DETAIL_DATA_W.DevicePath`.
        let offset = offset_of!(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath);

        Some(Self {
            data,
            path_len: size - offset,
        })
    }

    fn get(&self) -> PSP_DEVICE_INTERFACE_DETAIL_DATA_W {
        self.data
    }

    fn path(&self) -> String {
        unsafe { from_wide_ptr((*self.data).DevicePath.as_ptr(), self.path_len - 2) }
    }
}

impl Drop for DeviceInterfaceDetailData {
    fn drop(&mut self) {
        unsafe { libc::free(self.data as *mut libc::c_void) };
    }
}

pub struct DeviceCapabilities {
    caps: HIDP_CAPS,
}

impl DeviceCapabilities {
    pub fn new(handle: HANDLE) -> io::Result<Self> {
        let mut preparsed_data = ptr::null_mut();
        let rv = unsafe { HidD_GetPreparsedData(handle, &mut preparsed_data) };
        if rv == 0 || preparsed_data.is_null() {
            return Err(io_err("HidD_GetPreparsedData failed!"));
        }

        let mut caps: HIDP_CAPS = unsafe { mem::uninitialized() };

        unsafe {
            let rv = HidP_GetCaps(preparsed_data, &mut caps);
            HidD_FreePreparsedData(preparsed_data);

            if rv != HIDP_STATUS_SUCCESS {
                return Err(io_err("HidP_GetCaps failed!"));
            }
        }

        Ok(Self { caps })
    }

    pub fn usage(&self) -> USAGE {
        self.caps.Usage
    }

    pub fn usage_page(&self) -> USAGE {
        self.caps.UsagePage
    }
}

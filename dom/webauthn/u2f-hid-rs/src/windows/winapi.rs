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

use platform::winapi::winapi::shared::{guiddef, minwindef, ntdef, windef};
use platform::winapi::winapi::shared::{hidclass, hidpi, hidusage};
use platform::winapi::winapi::um::{handleapi, setupapi};

#[link(name = "setupapi")]
extern "stdcall" {
    fn SetupDiGetClassDevsW(
        ClassGuid: *const guiddef::GUID,
        Enumerator: ntdef::PCSTR,
        hwndParent: windef::HWND,
        flags: minwindef::DWORD,
    ) -> setupapi::HDEVINFO;

    fn SetupDiDestroyDeviceInfoList(DeviceInfoSet: setupapi::HDEVINFO) -> minwindef::BOOL;

    fn SetupDiEnumDeviceInterfaces(
        DeviceInfoSet: setupapi::HDEVINFO,
        DeviceInfoData: setupapi::PSP_DEVINFO_DATA,
        InterfaceClassGuid: *const guiddef::GUID,
        MemberIndex: minwindef::DWORD,
        DeviceInterfaceData: setupapi::PSP_DEVICE_INTERFACE_DATA,
    ) -> minwindef::BOOL;

    fn SetupDiGetDeviceInterfaceDetailW(
        DeviceInfoSet: setupapi::HDEVINFO,
        DeviceInterfaceData: setupapi::PSP_DEVICE_INTERFACE_DATA,
        DeviceInterfaceDetailData: setupapi::PSP_DEVICE_INTERFACE_DETAIL_DATA_W,
        DeviceInterfaceDetailDataSize: minwindef::DWORD,
        RequiredSize: minwindef::PDWORD,
        DeviceInfoData: setupapi::PSP_DEVINFO_DATA,
    ) -> minwindef::BOOL;
}

#[link(name = "hid")]
extern "stdcall" {
    fn HidD_GetPreparsedData(
        HidDeviceObject: ntdef::HANDLE,
        PreparsedData: *mut hidpi::PHIDP_PREPARSED_DATA,
    ) -> ntdef::BOOLEAN;

    fn HidD_FreePreparsedData(PreparsedData: hidpi::PHIDP_PREPARSED_DATA) -> ntdef::BOOLEAN;

    fn HidP_GetCaps(
        PreparsedData: hidpi::PHIDP_PREPARSED_DATA,
        Capabilities: hidpi::PHIDP_CAPS,
    ) -> ntdef::NTSTATUS;
}

macro_rules! offset_of {
    ($ty:ty, $field:ident) => {
        unsafe { &(*(0 as *const $ty)).$field as *const _ as usize }
    };
}

fn from_wide_ptr(ptr: *const u16, len: usize) -> String {
    assert!(!ptr.is_null() && len % 2 == 0);
    let slice = unsafe { slice::from_raw_parts(ptr, len / 2) };
    OsString::from_wide(slice).to_string_lossy().into_owned()
}

pub struct DeviceInfoSet {
    set: setupapi::HDEVINFO,
}

impl DeviceInfoSet {
    pub fn new() -> io::Result<Self> {
        let flags = setupapi::DIGCF_PRESENT | setupapi::DIGCF_DEVICEINTERFACE;
        let set = unsafe {
            SetupDiGetClassDevsW(
                &hidclass::GUID_DEVINTERFACE_HID,
                ptr::null_mut(),
                ptr::null_mut(),
                flags,
            )
        };
        if set == handleapi::INVALID_HANDLE_VALUE {
            return Err(io_err("SetupDiGetClassDevsW failed!"));
        }

        Ok(Self { set })
    }

    pub fn get(&self) -> setupapi::HDEVINFO {
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
    index: minwindef::DWORD,
}

impl<'a> DeviceInfoSetIter<'a> {
    fn new(set: &'a DeviceInfoSet) -> Self {
        Self { set, index: 0 }
    }
}

impl<'a> Iterator for DeviceInfoSetIter<'a> {
    type Item = String;

    fn next(&mut self) -> Option<Self::Item> {
        let mut device_interface_data =
            unsafe { mem::uninitialized::<setupapi::SP_DEVICE_INTERFACE_DATA>() };
        device_interface_data.cbSize =
            mem::size_of::<setupapi::SP_DEVICE_INTERFACE_DATA>() as minwindef::UINT;

        let rv = unsafe {
            SetupDiEnumDeviceInterfaces(
                self.set.get(),
                ptr::null_mut(),
                &hidclass::GUID_DEVINTERFACE_HID,
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
    data: setupapi::PSP_DEVICE_INTERFACE_DETAIL_DATA_W,
    path_len: usize,
}

impl DeviceInterfaceDetailData {
    fn new(size: usize) -> Option<Self> {
        let mut cb_size = mem::size_of::<setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W>();
        if cfg!(target_pointer_width = "32") {
            cb_size = 4 + 2; // 4-byte uint + default TCHAR size. size_of is inaccurate.
        }

        if size < cb_size {
            warn!("DeviceInterfaceDetailData is too small. {}", size);
            return None;
        }

        let data = unsafe { libc::malloc(size) as setupapi::PSP_DEVICE_INTERFACE_DETAIL_DATA_W };
        if data.is_null() {
            return None;
        }

        // Set total size of the structure.
        unsafe { (*data).cbSize = cb_size as minwindef::UINT };

        // Compute offset of `SP_DEVICE_INTERFACE_DETAIL_DATA_W.DevicePath`.
        let offset = offset_of!(setupapi::SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath);

        Some(Self {
            data,
            path_len: size - offset,
        })
    }

    fn get(&self) -> setupapi::PSP_DEVICE_INTERFACE_DETAIL_DATA_W {
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
    caps: hidpi::HIDP_CAPS,
}

impl DeviceCapabilities {
    pub fn new(handle: ntdef::HANDLE) -> io::Result<Self> {
        let mut preparsed_data = ptr::null_mut();
        let rv = unsafe { HidD_GetPreparsedData(handle, &mut preparsed_data) };
        if rv == 0 || preparsed_data.is_null() {
            return Err(io_err("HidD_GetPreparsedData failed!"));
        }

        let mut caps: hidpi::HIDP_CAPS = unsafe { mem::uninitialized() };

        unsafe {
            let rv = HidP_GetCaps(preparsed_data, &mut caps);
            HidD_FreePreparsedData(preparsed_data);

            if rv != hidpi::HIDP_STATUS_SUCCESS {
                return Err(io_err("HidP_GetCaps failed!"));
            }
        }

        Ok(Self { caps })
    }

    pub fn usage(&self) -> hidusage::USAGE {
        self.caps.Usage
    }

    pub fn usage_page(&self) -> hidusage::USAGE {
        self.caps.UsagePage
    }
}

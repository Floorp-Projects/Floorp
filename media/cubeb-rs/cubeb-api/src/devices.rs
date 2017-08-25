use ffi;
use std::marker;
use std::str;
use util::opt_bytes;

/// Audio device description
pub struct Device<'a> {
    raw: *const ffi::cubeb_device,
    _marker: marker::PhantomData<&'a ffi::cubeb_device>
}

impl<'a> Device<'a> {
    /// Gets the output device name.
    ///
    /// May return `None` if there is no output device.
    pub fn output_name(&self) -> Option<&str> {
        self.output_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    fn output_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, (*self.raw).output_name) }
    }

    /// Gets the input device name.
    ///
    /// May return `None` if there is no input device.
    pub fn input_name(&self) -> Option<&str> {
        self.input_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    fn input_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, (*self.raw).input_name) }
    }
}

impl<'a> Binding for Device<'a> {
    type Raw = *const ffi::cubeb_device;

    unsafe fn from_raw(raw: *const ffi::cubeb_device) -> Device<'a> {
        Device {
            raw: raw,
            _marker: marker::PhantomData
        }
    }
    fn raw(&self) -> *const ffi::cubeb_device {
        self.raw
    }
}

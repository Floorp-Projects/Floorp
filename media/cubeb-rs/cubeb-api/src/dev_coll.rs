//! Bindings to libcubeb's raw `cubeb_device_collection` type

use {Binding, Context};
use cubeb_core::{DeviceInfo, DeviceType, Result};
use ffi;
use std::{ptr, slice};
use std::ops::Deref;
use sys;

/// A collection of `DeviceInfo` used by libcubeb
pub struct DeviceCollection<'coll, 'ctx> {
    coll: &'coll [DeviceInfo],
    ctx: &'ctx Context
}

impl<'coll, 'ctx> DeviceCollection<'coll, 'ctx> {
    fn new(ctx: &'ctx Context, devtype: DeviceType) -> Result<DeviceCollection> {
        let mut coll = ffi::cubeb_device_collection {
            device: ptr::null(),
            count: 0
        };
        let devices = unsafe {
            try_call!(sys::cubeb_enumerate_devices(
                ctx.raw(),
                devtype.bits(),
                &mut coll
            ));
            slice::from_raw_parts(coll.device as *const _, coll.count)
        };
        Ok(DeviceCollection {
            coll: devices,
            ctx: ctx
        })
    }
}

impl<'coll, 'ctx> Deref for DeviceCollection<'coll, 'ctx> {
    type Target = [DeviceInfo];
    fn deref(&self) -> &[DeviceInfo] {
        self.coll
    }
}

impl<'coll, 'ctx> Drop for DeviceCollection<'coll, 'ctx> {
    fn drop(&mut self) {
        let mut coll = ffi::cubeb_device_collection {
            device: self.coll.as_ptr() as *const _,
            count: self.coll.len()
        };
        unsafe {
            call!(sys::cubeb_device_collection_destroy(
                self.ctx.raw(),
                &mut coll
            ));
        }
    }
}

pub fn enumerate(ctx: &Context, devtype: DeviceType) -> Result<DeviceCollection> {
    DeviceCollection::new(ctx, devtype)
}

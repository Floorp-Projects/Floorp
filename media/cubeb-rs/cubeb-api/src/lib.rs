//! # libcubeb bindings for rust
//!
//! This library contains bindings to the [cubeb][1] C library which
//! is used to interact with system audio.  The library itself is a
//! work in progress and is likely lacking documentation and test.
//!
//! [1]: https://github.com/kinetiknz/cubeb/
//!
//! The cubeb-rs library exposes the user API of libcubeb.  It doesn't
//! expose the internal interfaces, so isn't suitable for extending
//! libcubeb. See [cubeb-pulse-rs][2] for an example of extending
//! libcubeb via implementing a cubeb backend in rust.

extern crate cubeb_core;
extern crate libcubeb_sys as sys;

#[macro_use]
mod call;
mod context;
mod dev_coll;
mod frame;
mod log;
mod stream;
mod util;

pub use context::Context;
// Re-export cubeb_core types
pub use cubeb_core::{ChannelLayout, Device, DeviceFormat, DeviceId, DeviceInfo,
                     DeviceState, DeviceType, Error, ErrorCode, LogLevel, Result,
                     SampleFormat, State, StreamParams};
pub use cubeb_core::{DEVICE_FMT_F32BE, DEVICE_FMT_F32LE, DEVICE_FMT_S16BE,
                     DEVICE_FMT_S16LE};
pub use cubeb_core::{DEVICE_PREF_ALL, DEVICE_PREF_MULTIMEDIA, DEVICE_PREF_NONE,
                     DEVICE_PREF_NOTIFICATION, DEVICE_PREF_VOICE};
pub use cubeb_core::{DEVICE_TYPE_INPUT, DEVICE_TYPE_OUTPUT, DEVICE_TYPE_UNKNOWN};

use cubeb_core::binding::Binding;
use cubeb_core::ffi;
pub use dev_coll::DeviceCollection;
pub use frame::{Frame, MonoFrame, StereoFrame};
pub use log::*;
pub use stream::{SampleType, Stream, StreamCallback, StreamInitOptions,
                 StreamInitOptionsBuilder, StreamParamsBuilder};

pub type DeviceChangedCb<'a> = FnMut() + 'a;
pub type DeviceCollectionChangedCb<'a> = FnMut(Context) + 'a;

// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#[macro_use]
extern crate bitflags;

pub mod ffi;
pub mod binding;
mod error;
mod util;

use binding::Binding;
pub use error::Error;
use std::{marker, ptr, str};
use util::opt_bytes;

#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum SampleFormat {
    S16LE,
    S16BE,
    S16NE,
    Float32LE,
    Float32BE,
    Float32NE
}

impl From<ffi::cubeb_sample_format> for SampleFormat {
    fn from(x: ffi::cubeb_sample_format) -> SampleFormat {
        match x {
            ffi::CUBEB_SAMPLE_S16LE => SampleFormat::S16LE,
            ffi::CUBEB_SAMPLE_S16BE => SampleFormat::S16BE,
            ffi::CUBEB_SAMPLE_FLOAT32LE => SampleFormat::Float32LE,
            ffi::CUBEB_SAMPLE_FLOAT32BE => SampleFormat::Float32BE,
            // TODO: Implement TryFrom
            _ => SampleFormat::S16NE,
        }
    }
}

/// An opaque handle used to refer to a particular input or output device
/// across calls.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub struct DeviceId {
    raw: ffi::cubeb_devid
}

impl Binding for DeviceId {
    type Raw = ffi::cubeb_devid;

    unsafe fn from_raw(raw: Self::Raw) -> DeviceId {
        DeviceId {
            raw: raw
        }
    }
    fn raw(&self) -> Self::Raw {
        self.raw
    }
}

impl Default for DeviceId {
    fn default() -> Self {
        DeviceId {
            raw: ptr::null()
        }
    }
}

/// Level (verbosity) of logging for a particular cubeb context.
#[derive(PartialEq, Eq, Clone, Debug, Copy, PartialOrd, Ord)]
pub enum LogLevel {
    /// Logging disabled
    Disabled,
    /// Logging lifetime operation (creation/destruction).
    Normal,
    /// Verbose logging of callbacks, can have performance implications.
    Verbose
}

/// SMPTE channel layout (also known as wave order)
///
/// ---------------------------------------------------
/// Name          | Channels
/// ---------------------------------------------------
/// DUAL-MONO     | L   R
/// DUAL-MONO-LFE | L   R   LFE
/// MONO          | M
/// MONO-LFE      | M   LFE
/// STEREO        | L   R
/// STEREO-LFE    | L   R   LFE
/// 3F            | L   R   C
/// 3F-LFE        | L   R   C    LFE
/// 2F1           | L   R   S
/// 2F1-LFE       | L   R   LFE  S
/// 3F1           | L   R   C    S
/// 3F1-LFE       | L   R   C    LFE S
/// 2F2           | L   R   LS   RS
/// 2F2-LFE       | L   R   LFE  LS   RS
/// 3F2           | L   R   C    LS   RS
/// 3F2-LFE       | L   R   C    LFE  LS   RS
/// 3F3R-LFE      | L   R   C    LFE  RC   LS   RS
/// 3F4-LFE       | L   R   C    LFE  RLS  RRS  LS   RS
/// ---------------------------------------------------
///
/// The abbreviation of channel name is defined in following table:
/// ---------------------------
/// Abbr | Channel name
/// ---------------------------
/// M    | Mono
/// L    | Left
/// R    | Right
/// C    | Center
/// LS   | Left Surround
/// RS   | Right Surround
/// RLS  | Rear Left Surround
/// RC   | Rear Center
/// RRS  | Rear Right Surround
/// LFE  | Low Frequency Effects
/// ---------------------------
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum ChannelLayout {
    /// Indicate the speaker's layout is undefined.
    Undefined,
    DualMono,
    DualMonoLfe,
    Mono,
    MonoLfe,
    Stereo,
    StereoLfe,
    Layout3F,
    Layout3FLfe,
    Layout2F1,
    Layout2F1Lfe,
    Layout3F1,
    Layout3F1Lfe,
    Layout2F2,
    Layout2F2Lfe,
    Layout3F2,
    Layout3F2Lfe,
    Layout3F3RLfe,
    Layout3F4Lfe
}

impl From<ffi::cubeb_channel_layout> for ChannelLayout {
    fn from(x: ffi::cubeb_channel_layout) -> ChannelLayout {
        match x {
            ffi::CUBEB_LAYOUT_UNDEFINED => ChannelLayout::Undefined,
            ffi::CUBEB_LAYOUT_DUAL_MONO => ChannelLayout::DualMono,
            ffi::CUBEB_LAYOUT_DUAL_MONO_LFE => ChannelLayout::DualMonoLfe,
            ffi::CUBEB_LAYOUT_MONO => ChannelLayout::Mono,
            ffi::CUBEB_LAYOUT_MONO_LFE => ChannelLayout::MonoLfe,
            ffi::CUBEB_LAYOUT_STEREO => ChannelLayout::Stereo,
            ffi::CUBEB_LAYOUT_STEREO_LFE => ChannelLayout::StereoLfe,
            ffi::CUBEB_LAYOUT_3F => ChannelLayout::Layout3F,
            ffi::CUBEB_LAYOUT_3F_LFE => ChannelLayout::Layout3FLfe,
            ffi::CUBEB_LAYOUT_2F1 => ChannelLayout::Layout2F1,
            ffi::CUBEB_LAYOUT_2F1_LFE => ChannelLayout::Layout2F1Lfe,
            ffi::CUBEB_LAYOUT_3F1 => ChannelLayout::Layout3F1,
            ffi::CUBEB_LAYOUT_3F1_LFE => ChannelLayout::Layout3F1Lfe,
            ffi::CUBEB_LAYOUT_2F2 => ChannelLayout::Layout2F2,
            ffi::CUBEB_LAYOUT_2F2_LFE => ChannelLayout::Layout2F2Lfe,
            ffi::CUBEB_LAYOUT_3F2 => ChannelLayout::Layout3F2,
            ffi::CUBEB_LAYOUT_3F2_LFE => ChannelLayout::Layout3F2Lfe,
            ffi::CUBEB_LAYOUT_3F3R_LFE => ChannelLayout::Layout3F3RLfe,
            ffi::CUBEB_LAYOUT_3F4_LFE => ChannelLayout::Layout3F4Lfe,
            // TODO: Implement TryFrom
            _ => ChannelLayout::Undefined,
        }
    }
}

/// Stream format initialization parameters.
#[derive(Clone, Copy)]
pub struct StreamParams {
    raw: ffi::cubeb_stream_params
}

impl StreamParams {
    pub fn format(&self) -> SampleFormat {
        macro_rules! check( ($($raw:ident => $real:ident),*) => (
            $(if self.raw.format == ffi::$raw {
                SampleFormat::$real
            }) else *
            else {
                panic!("unknown sample format: {}", self.raw.format)
            }
        ) );

        check!(
            CUBEB_SAMPLE_S16LE => S16LE,
            CUBEB_SAMPLE_S16BE => S16BE,
            CUBEB_SAMPLE_FLOAT32LE => Float32LE,
            CUBEB_SAMPLE_FLOAT32BE => Float32BE
        )
    }

    pub fn rate(&self) -> u32 {
        self.raw.rate as u32
    }

    pub fn channels(&self) -> u32 {
        self.raw.channels as u32
    }

    pub fn layout(&self) -> ChannelLayout {
        macro_rules! check( ($($raw:ident => $real:ident),*) => (
            $(if self.raw.layout == ffi::$raw {
                ChannelLayout::$real
            }) else *
            else {
                panic!("unknown channel layout: {}", self.raw.layout)
            }
        ) );

        check!(CUBEB_LAYOUT_UNDEFINED => Undefined,
               CUBEB_LAYOUT_DUAL_MONO => DualMono,
               CUBEB_LAYOUT_DUAL_MONO_LFE => DualMonoLfe,
               CUBEB_LAYOUT_MONO => Mono,
               CUBEB_LAYOUT_MONO_LFE => MonoLfe,
               CUBEB_LAYOUT_STEREO => Stereo,
               CUBEB_LAYOUT_STEREO_LFE => StereoLfe,
               CUBEB_LAYOUT_3F => Layout3F,
               CUBEB_LAYOUT_3F_LFE => Layout3FLfe,
               CUBEB_LAYOUT_2F1 => Layout2F1,
               CUBEB_LAYOUT_2F1_LFE => Layout2F1Lfe,
               CUBEB_LAYOUT_3F1 => Layout3F1,
               CUBEB_LAYOUT_3F1_LFE => Layout3F1Lfe,
               CUBEB_LAYOUT_2F2 => Layout2F2,
               CUBEB_LAYOUT_2F2_LFE => Layout2F2Lfe,
               CUBEB_LAYOUT_3F2 => Layout3F2,
               CUBEB_LAYOUT_3F2_LFE => Layout3F2Lfe,
               CUBEB_LAYOUT_3F3R_LFE => Layout3F3RLfe,
               CUBEB_LAYOUT_3F4_LFE => Layout3F4Lfe)
    }
}

impl Binding for StreamParams {
    type Raw = *const ffi::cubeb_stream_params;
    unsafe fn from_raw(raw: *const ffi::cubeb_stream_params) -> Self {
        Self {
            raw: *raw
        }
    }
    fn raw(&self) -> Self::Raw {
        &self.raw as Self::Raw
    }
}

/// Audio device description
#[derive(Copy, Clone, Debug)]
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

    pub fn output_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, (*self.raw).output_name) }
    }

    /// Gets the input device name.
    ///
    /// May return `None` if there is no input device.
    pub fn input_name(&self) -> Option<&str> {
        self.input_name_bytes().map(|b| str::from_utf8(b).unwrap())
    }

    pub fn input_name_bytes(&self) -> Option<&[u8]> {
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

/// Stream states signaled via state_callback.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum State {
    /// Stream started.
    Started,
    /// Stream stopped.
    Stopped,
    /// Stream drained.
    Drained,
    /// Stream disabled due to error.
    Error
}

/// An enumeration of possible errors that can happen when working with cubeb.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum ErrorCode {
    /// GenericError
    Error,
    /// Requested format is invalid
    InvalidFormat,
    /// Requested parameter is invalid
    InvalidParameter,
    /// Requested operation is not supported
    NotSupported,
    /// Requested device is unavailable
    DeviceUnavailable
}

/// Whether a particular device is an input device (e.g. a microphone), or an
/// output device (e.g. headphones).
bitflags! {
    pub struct DeviceType: ffi::cubeb_device_type {
        const DEVICE_TYPE_UNKNOWN = ffi::CUBEB_DEVICE_TYPE_UNKNOWN as _;
        const DEVICE_TYPE_INPUT = ffi::CUBEB_DEVICE_TYPE_INPUT as _;
        const DEVICE_TYPE_OUTPUT = ffi::CUBEB_DEVICE_TYPE_OUTPUT as _;
    }
}

/// The state of a device.
#[derive(PartialEq, Eq, Clone, Debug, Copy)]
pub enum DeviceState {
    /// The device has been disabled at the system level.
    Disabled,
    /// The device is enabled, but nothing is plugged into it.
    Unplugged,
    /// The device is enabled.
    Enabled
}

/// Architecture specific sample type.
bitflags! {
    pub struct DeviceFormat: ffi::cubeb_device_fmt {
        const DEVICE_FMT_S16LE = ffi::CUBEB_DEVICE_FMT_S16LE;
        const DEVICE_FMT_S16BE = ffi::CUBEB_DEVICE_FMT_S16BE;
        const DEVICE_FMT_F32LE = ffi::CUBEB_DEVICE_FMT_F32LE;
        const DEVICE_FMT_F32BE = ffi::CUBEB_DEVICE_FMT_F32BE;
    }
}

/// Channel type for a `cubeb_stream`. Depending on the backend and platform
/// used, this can control inter-stream interruption, ducking, and volume
/// control.
bitflags! {
    pub struct DevicePref: ffi::cubeb_device_pref {
        const DEVICE_PREF_NONE = ffi::CUBEB_DEVICE_PREF_NONE;
        const DEVICE_PREF_MULTIMEDIA = ffi::CUBEB_DEVICE_PREF_MULTIMEDIA;
        const DEVICE_PREF_VOICE = ffi::CUBEB_DEVICE_PREF_VOICE;
        const DEVICE_PREF_NOTIFICATION = ffi::CUBEB_DEVICE_PREF_NOTIFICATION;
        const DEVICE_PREF_ALL = ffi::CUBEB_DEVICE_PREF_ALL;
    }
}

/// This structure holds the characteristics of an input or output
/// audio device. It is obtained using `enumerate_devices`, which
/// returns these structures via `device_collection` and must be
/// destroyed via `device_collection_destroy`.
pub struct DeviceInfo {
    raw: ffi::cubeb_device_info
}

impl DeviceInfo {
    pub fn raw(&self) -> &ffi::cubeb_device_info {
        &self.raw
    }

    /// Device identifier handle.
    pub fn devid(&self) -> DeviceId {
        unsafe { Binding::from_raw(self.raw.devid) }
    }

    /// Device identifier which might be presented in a UI.
    pub fn device_id(&self) -> Option<&str> {
        self.device_id_bytes().and_then(|s| str::from_utf8(s).ok())
    }

    pub fn device_id_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, self.raw.device_id) }
    }

    /// Friendly device name which might be presented in a UI.
    pub fn friendly_name(&self) -> Option<&str> {
        self.friendly_name_bytes().and_then(
            |s| str::from_utf8(s).ok()
        )
    }

    pub fn friendly_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, self.raw.friendly_name) }
    }

    /// Two devices have the same group identifier if they belong to
    /// the same physical device; for example a headset and
    /// microphone.
    pub fn group_id(&self) -> Option<&str> {
        self.group_id_bytes().and_then(|s| str::from_utf8(s).ok())
    }

    pub fn group_id_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, self.raw.group_id) }
    }

    /// Optional vendor name, may be NULL.
    pub fn vendor_name(&self) -> Option<&str> {
        self.vendor_name_bytes().and_then(
            |s| str::from_utf8(s).ok()
        )
    }

    pub fn vendor_name_bytes(&self) -> Option<&[u8]> {
        unsafe { opt_bytes(self, self.raw.vendor_name) }
    }

    /// Type of device (Input/Output).
    pub fn device_type(&self) -> DeviceType {
        DeviceType::from_bits_truncate(self.raw.device_type)
    }

    /// State of device disabled/enabled/unplugged.
    pub fn state(&self) -> DeviceState {
        let state = self.raw.state;
        macro_rules! check( ($($raw:ident => $real:ident),*) => (
            $(if state == ffi::$raw {
                DeviceState::$real
            }) else *
            else {
                panic!("unknown device state: {}", state)
            }
        ));

        check!(CUBEB_DEVICE_STATE_DISABLED => Disabled,
               CUBEB_DEVICE_STATE_UNPLUGGED => Unplugged,
               CUBEB_DEVICE_STATE_ENABLED => Enabled)
    }

    /// Preferred device.
    pub fn preferred(&self) -> DevicePref {
        DevicePref::from_bits(self.raw.preferred).unwrap()
    }

    /// Sample format supported.
    pub fn format(&self) -> DeviceFormat {
        DeviceFormat::from_bits(self.raw.format).unwrap()
    }

    /// The default sample format for this device.
    pub fn default_format(&self) -> DeviceFormat {
        DeviceFormat::from_bits(self.raw.default_format).unwrap()
    }

    /// Channels.
    pub fn max_channels(&self) -> u32 {
        self.raw.max_channels
    }

    /// Default/Preferred sample rate.
    pub fn default_rate(&self) -> u32 {
        self.raw.default_rate
    }

    /// Maximum sample rate supported.
    pub fn max_rate(&self) -> u32 {
        self.raw.max_rate
    }

    /// Minimum sample rate supported.
    pub fn min_rate(&self) -> u32 {
        self.raw.min_rate
    }

    /// Lowest possible latency in frames.
    pub fn latency_lo(&self) -> u32 {
        self.raw.latency_lo
    }

    /// Higest possible latency in frames.
    pub fn latency_hi(&self) -> u32 {
        self.raw.latency_hi
    }
}

pub type Result<T> = ::std::result::Result<T, Error>;

#[cfg(test)]
mod tests {
    use binding::Binding;
    use std::mem;

    #[test]
    fn stream_params_raw_channels() {
        let mut raw: super::ffi::cubeb_stream_params = unsafe { mem::zeroed() };
        raw.channels = 2;
        let params = unsafe { super::StreamParams::from_raw(&raw as *const _) };
        assert_eq!(params.channels(), 2);
    }

    #[test]
    fn stream_params_raw_format() {
        let mut raw: super::ffi::cubeb_stream_params = unsafe { mem::zeroed() };
        macro_rules! check(
            ($($raw:ident => $real:ident),*) => (
                $(raw.format = super::ffi::$raw;
                  let params = unsafe {
                      super::StreamParams::from_raw(&raw as *const _)
                  };
                  assert_eq!(params.format(), super::SampleFormat::$real);
                )*
            ) );

        check!(CUBEB_SAMPLE_S16LE => S16LE,
               CUBEB_SAMPLE_S16BE => S16BE,
               CUBEB_SAMPLE_FLOAT32LE => Float32LE,
               CUBEB_SAMPLE_FLOAT32BE => Float32BE);
    }

    #[test]
    fn stream_params_raw_format_native_endian() {
        let mut raw: super::ffi::cubeb_stream_params = unsafe { mem::zeroed() };
        raw.format = super::ffi::CUBEB_SAMPLE_S16NE;
        let params = unsafe { super::StreamParams::from_raw(&raw as *const _) };
        assert_eq!(
            params.format(),
            if cfg!(target_endian = "little") {
                super::SampleFormat::S16LE
            } else {
                super::SampleFormat::S16BE
            }
        );

        raw.format = super::ffi::CUBEB_SAMPLE_FLOAT32NE;
        let params = unsafe { super::StreamParams::from_raw(&raw as *const _) };
        assert_eq!(
            params.format(),
            if cfg!(target_endian = "little") {
                super::SampleFormat::Float32LE
            } else {
                super::SampleFormat::Float32BE
            }
        );
    }

    #[test]
    fn stream_params_raw_layout() {
        let mut raw: super::ffi::cubeb_stream_params = unsafe { mem::zeroed() };
        macro_rules! check(
            ($($raw:ident => $real:ident),*) => (
                $(raw.layout = super::ffi::$raw;
                  let params = unsafe {
                      super::StreamParams::from_raw(&raw as *const _)
                  };
                  assert_eq!(params.layout(), super::ChannelLayout::$real);
                )*
            ) );

        check!(CUBEB_LAYOUT_UNDEFINED => Undefined,
               CUBEB_LAYOUT_DUAL_MONO => DualMono,
               CUBEB_LAYOUT_DUAL_MONO_LFE => DualMonoLfe,
               CUBEB_LAYOUT_MONO => Mono,
               CUBEB_LAYOUT_MONO_LFE => MonoLfe,
               CUBEB_LAYOUT_STEREO => Stereo,
               CUBEB_LAYOUT_STEREO_LFE => StereoLfe,
               CUBEB_LAYOUT_3F => Layout3F,
               CUBEB_LAYOUT_3F_LFE => Layout3FLfe,
               CUBEB_LAYOUT_2F1 => Layout2F1,
               CUBEB_LAYOUT_2F1_LFE => Layout2F1Lfe,
               CUBEB_LAYOUT_3F1 => Layout3F1,
               CUBEB_LAYOUT_3F1_LFE => Layout3F1Lfe,
               CUBEB_LAYOUT_2F2 => Layout2F2,
               CUBEB_LAYOUT_2F2_LFE => Layout2F2Lfe,
               CUBEB_LAYOUT_3F2 => Layout3F2,
               CUBEB_LAYOUT_3F2_LFE => Layout3F2Lfe,
               CUBEB_LAYOUT_3F3R_LFE => Layout3F3RLfe,
               CUBEB_LAYOUT_3F4_LFE => Layout3F4Lfe);
    }

    #[test]
    fn stream_params_raw_rate() {
        let mut raw: super::ffi::cubeb_stream_params = unsafe { mem::zeroed() };
        raw.rate = 44100;
        let params = unsafe { super::StreamParams::from_raw(&raw as *const _) };
        assert_eq!(params.rate(), 44100);
    }

}

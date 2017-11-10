// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use cubeb_core::{self, ffi};
use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;

#[derive(Debug, Serialize, Deserialize)]
pub struct Device {
    pub output_name: Option<Vec<u8>>,
    pub input_name: Option<Vec<u8>>
}

impl<'a> From<cubeb_core::Device<'a>> for Device {
    fn from(info: cubeb_core::Device) -> Self {
        Self {
            output_name: info.output_name_bytes().map(|s| s.to_vec()),
            input_name: info.input_name_bytes().map(|s| s.to_vec())
        }
    }
}

impl From<ffi::cubeb_device> for Device {
    fn from(info: ffi::cubeb_device) -> Self {
        Self {
            output_name: dup_str(info.output_name),
            input_name: dup_str(info.input_name)
        }
    }
}

impl From<Device> for ffi::cubeb_device {
    fn from(info: Device) -> Self {
        Self {
            output_name: opt_str(info.output_name),
            input_name: opt_str(info.input_name)
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct DeviceInfo {
    pub devid: usize,
    pub device_id: Option<Vec<u8>>,
    pub friendly_name: Option<Vec<u8>>,
    pub group_id: Option<Vec<u8>>,
    pub vendor_name: Option<Vec<u8>>,

    pub device_type: ffi::cubeb_device_type,
    pub state: ffi::cubeb_device_state,
    pub preferred: ffi::cubeb_device_pref,

    pub format: ffi::cubeb_device_fmt,
    pub default_format: ffi::cubeb_device_fmt,
    pub max_channels: u32,
    pub default_rate: u32,
    pub max_rate: u32,
    pub min_rate: u32,

    pub latency_lo: u32,
    pub latency_hi: u32
}

impl<'a> From<&'a ffi::cubeb_device_info> for DeviceInfo {
    fn from(info: &'a ffi::cubeb_device_info) -> Self {
        DeviceInfo {
            devid: info.devid as _,
            device_id: dup_str(info.device_id),
            friendly_name: dup_str(info.friendly_name),
            group_id: dup_str(info.group_id),
            vendor_name: dup_str(info.vendor_name),

            device_type: info.device_type,
            state: info.state,
            preferred: info.preferred,

            format: info.format,
            default_format: info.default_format,
            max_channels: info.max_channels,
            default_rate: info.default_rate,
            max_rate: info.max_rate,
            min_rate: info.min_rate,

            latency_lo: info.latency_lo,
            latency_hi: info.latency_hi
        }
    }
}

impl From<DeviceInfo> for ffi::cubeb_device_info {
    fn from(info: DeviceInfo) -> Self {
        ffi::cubeb_device_info {
            devid: info.devid as _,
            device_id: opt_str(info.device_id),
            friendly_name: opt_str(info.friendly_name),
            group_id: opt_str(info.group_id),
            vendor_name: opt_str(info.vendor_name),

            device_type: info.device_type,
            state: info.state,
            preferred: info.preferred,

            format: info.format,
            default_format: info.default_format,
            max_channels: info.max_channels,
            default_rate: info.default_rate,
            max_rate: info.max_rate,
            min_rate: info.min_rate,

            latency_lo: info.latency_lo,
            latency_hi: info.latency_hi
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamParams {
    pub format: u32,
    pub rate: u16,
    pub channels: u8,
    pub layout: i32
}

impl<'a> From<&'a ffi::cubeb_stream_params> for StreamParams {
    fn from(params: &'a ffi::cubeb_stream_params) -> Self {
        assert!(params.channels <= u32::from(u8::max_value()));

        StreamParams {
            format: params.format,
            rate: params.rate as u16,
            channels: params.channels as u8,
            layout: params.layout
        }
    }
}

impl<'a> From<&'a StreamParams> for ffi::cubeb_stream_params {
    fn from(params: &StreamParams) -> Self {
        ffi::cubeb_stream_params {
            format: params.format,
            rate: u32::from(params.rate),
            channels: u32::from(params.channels),
            layout: params.layout
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamInitParams {
    pub stream_name: Option<Vec<u8>>,
    pub input_device: usize,
    pub input_stream_params: Option<StreamParams>,
    pub output_device: usize,
    pub output_stream_params: Option<StreamParams>,
    pub latency_frames: u32
}

fn dup_str(s: *const c_char) -> Option<Vec<u8>> {
    if s.is_null() {
        None
    } else {
        let vec: Vec<u8> = unsafe { CStr::from_ptr(s) }.to_bytes().to_vec();
        Some(vec)
    }
}

fn opt_str(v: Option<Vec<u8>>) -> *const c_char {
    match v {
        Some(v) => {
            match CString::new(v) {
                Ok(s) => s.into_raw(),
                Err(_) => {
                    debug!("Failed to convert bytes to CString");
                    ptr::null()
                },
            }
        },
        None => ptr::null(),
    }
}

// Client -> Server messages.
// TODO: Callbacks should be different messages types so
// ServerConn::process_msg doesn't have a catch-all case.
#[derive(Debug, Serialize, Deserialize)]
pub enum ServerMessage {
    ClientConnect,
    ClientDisconnect,

    ContextGetBackendId,
    ContextGetMaxChannelCount,
    ContextGetMinLatency(StreamParams),
    ContextGetPreferredSampleRate,
    ContextGetPreferredChannelLayout,
    ContextGetDeviceEnumeration(ffi::cubeb_device_type),

    StreamInit(StreamInitParams),
    StreamDestroy(usize),

    StreamStart(usize),
    StreamStop(usize),
    StreamResetDefaultDevice(usize),
    StreamGetPosition(usize),
    StreamGetLatency(usize),
    StreamSetVolume(usize, f32),
    StreamSetPanning(usize, f32),
    StreamGetCurrentDevice(usize),

    StreamDataCallback(isize),
    StreamStateCallback
}

// Server -> Client messages.
// TODO: Streams need id.
#[derive(Debug, Serialize, Deserialize)]
pub enum ClientMessage {
    ClientConnected,
    ClientDisconnected,

    ContextBackendId(),
    ContextMaxChannelCount(u32),
    ContextMinLatency(u32),
    ContextPreferredSampleRate(u32),
    ContextPreferredChannelLayout(ffi::cubeb_channel_layout),
    ContextEnumeratedDevices(Vec<DeviceInfo>),

    StreamCreated(usize), /*(RawFd)*/
    StreamCreatedInputShm, /*(RawFd)*/
    StreamCreatedOutputShm, /*(RawFd)*/
    StreamDestroyed,

    StreamStarted,
    StreamStopped,
    StreamDefaultDeviceReset,
    StreamPosition(u64),
    StreamLatency(u32),
    StreamVolumeSet,
    StreamPanningSet,
    StreamCurrentDevice(Device),

    StreamDataCallback(isize, usize),
    StreamStateCallback(ffi::cubeb_state),

    Error(ffi::cubeb_error_code)
}

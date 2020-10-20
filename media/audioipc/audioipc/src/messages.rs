// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crate::PlatformHandle;
use crate::PlatformHandleType;
#[cfg(target_os = "linux")]
use audio_thread_priority::RtPriorityThreadInfo;
use cubeb::{self, ffi};
use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint};
use std::ptr;

#[derive(Debug, Serialize, Deserialize)]
pub struct Device {
    pub output_name: Option<Vec<u8>>,
    pub input_name: Option<Vec<u8>>,
}

impl<'a> From<&'a cubeb::DeviceRef> for Device {
    fn from(info: &'a cubeb::DeviceRef) -> Self {
        Self {
            output_name: info.output_name_bytes().map(|s| s.to_vec()),
            input_name: info.input_name_bytes().map(|s| s.to_vec()),
        }
    }
}

impl From<ffi::cubeb_device> for Device {
    fn from(info: ffi::cubeb_device) -> Self {
        Self {
            output_name: dup_str(info.output_name),
            input_name: dup_str(info.input_name),
        }
    }
}

impl From<Device> for ffi::cubeb_device {
    fn from(info: Device) -> Self {
        Self {
            output_name: opt_str(info.output_name),
            input_name: opt_str(info.input_name),
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
    pub latency_hi: u32,
}

impl<'a> From<&'a cubeb::DeviceInfoRef> for DeviceInfo {
    fn from(info: &'a cubeb::DeviceInfoRef) -> Self {
        let info = unsafe { &*info.as_ptr() };
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
            latency_hi: info.latency_hi,
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
            latency_hi: info.latency_hi,
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct StreamParams {
    pub format: ffi::cubeb_sample_format,
    pub rate: c_uint,
    pub channels: c_uint,
    pub layout: ffi::cubeb_channel_layout,
    pub prefs: ffi::cubeb_stream_prefs,
}

impl<'a> From<&'a cubeb::StreamParamsRef> for StreamParams {
    fn from(x: &cubeb::StreamParamsRef) -> StreamParams {
        unsafe { *(x.as_ptr() as *mut StreamParams) }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamInitParams {
    pub stream_name: Option<Vec<u8>>,
    pub input_device: usize,
    pub input_stream_params: Option<StreamParams>,
    pub output_device: usize,
    pub output_stream_params: Option<StreamParams>,
    pub latency_frames: u32,
}

fn dup_str(s: *const c_char) -> Option<Vec<u8>> {
    if s.is_null() {
        None
    } else {
        let vec: Vec<u8> = unsafe { CStr::from_ptr(s) }.to_bytes().to_vec();
        Some(vec)
    }
}

fn opt_str(v: Option<Vec<u8>>) -> *mut c_char {
    match v {
        Some(v) => match CString::new(v) {
            Ok(s) => s.into_raw(),
            Err(_) => {
                debug!("Failed to convert bytes to CString");
                ptr::null_mut()
            }
        },
        None => ptr::null_mut(),
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct StreamCreate {
    pub token: usize,
    pub platform_handles: [PlatformHandle; 3],
    pub target_pid: u32,
}

#[derive(Debug, Serialize, Deserialize)]
pub struct RegisterDeviceCollectionChanged {
    pub platform_handles: [PlatformHandle; 3],
    pub target_pid: u32,
}

// Client -> Server messages.
// TODO: Callbacks should be different messages types so
// ServerConn::process_msg doesn't have a catch-all case.
#[derive(Debug, Serialize, Deserialize)]
pub enum ServerMessage {
    ClientConnect(u32),
    ClientDisconnect,

    ContextGetBackendId,
    ContextGetMaxChannelCount,
    ContextGetMinLatency(StreamParams),
    ContextGetPreferredSampleRate,
    ContextGetDeviceEnumeration(ffi::cubeb_device_type),
    ContextSetupDeviceCollectionCallback,
    ContextRegisterDeviceCollectionChanged(ffi::cubeb_device_type, bool),

    StreamInit(StreamInitParams),
    StreamDestroy(usize),

    StreamStart(usize),
    StreamStop(usize),
    StreamResetDefaultDevice(usize),
    StreamGetPosition(usize),
    StreamGetLatency(usize),
    StreamGetInputLatency(usize),
    StreamSetVolume(usize, f32),
    StreamGetCurrentDevice(usize),
    StreamRegisterDeviceChangeCallback(usize, bool),

    #[cfg(target_os = "linux")]
    PromoteThreadToRealTime([u8; std::mem::size_of::<RtPriorityThreadInfo>()]),
}

// Server -> Client messages.
// TODO: Streams need id.
#[derive(Debug, Serialize, Deserialize)]
pub enum ClientMessage {
    ClientConnected,
    ClientDisconnected,

    ContextBackendId(String),
    ContextMaxChannelCount(u32),
    ContextMinLatency(u32),
    ContextPreferredSampleRate(u32),
    ContextEnumeratedDevices(Vec<DeviceInfo>),
    ContextSetupDeviceCollectionCallback(RegisterDeviceCollectionChanged),
    ContextRegisteredDeviceCollectionChanged,

    StreamCreated(StreamCreate),
    StreamDestroyed,

    StreamStarted,
    StreamStopped,
    StreamDefaultDeviceReset,
    StreamPosition(u64),
    StreamLatency(u32),
    StreamInputLatency(u32),
    StreamVolumeSet,
    StreamCurrentDevice(Device),
    StreamRegisterDeviceChangeCallback,

    #[cfg(target_os = "linux")]
    ThreadPromoted,

    Error(c_int),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum CallbackReq {
    Data {
        nframes: isize,
        input_frame_size: usize,
        output_frame_size: usize,
    },
    State(ffi::cubeb_state),
    DeviceChange,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum CallbackResp {
    Data(isize),
    State,
    DeviceChange,
}

#[derive(Debug, Deserialize, Serialize)]
pub enum DeviceCollectionReq {
    DeviceChange(ffi::cubeb_device_type),
}

#[derive(Debug, Deserialize, Serialize)]
pub enum DeviceCollectionResp {
    DeviceChange,
}

pub trait AssocRawPlatformHandle {
    fn platform_handles(&self) -> Option<([PlatformHandleType; 3], u32)> {
        None
    }

    fn take_platform_handles<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<[PlatformHandleType; 3]>,
    {
        assert!(f().is_none());
    }
}

impl AssocRawPlatformHandle for ServerMessage {}

impl AssocRawPlatformHandle for ClientMessage {
    fn platform_handles(&self) -> Option<([PlatformHandleType; 3], u32)> {
        unsafe {
            match *self {
                ClientMessage::StreamCreated(ref data) => Some((
                    [
                        data.platform_handles[0].into_raw(),
                        data.platform_handles[1].into_raw(),
                        data.platform_handles[2].into_raw(),
                    ],
                    data.target_pid,
                )),
                ClientMessage::ContextSetupDeviceCollectionCallback(ref data) => Some((
                    [
                        data.platform_handles[0].into_raw(),
                        data.platform_handles[1].into_raw(),
                        data.platform_handles[2].into_raw(),
                    ],
                    data.target_pid,
                )),
                _ => None,
            }
        }
    }

    fn take_platform_handles<F>(&mut self, f: F)
    where
        F: FnOnce() -> Option<[PlatformHandleType; 3]>,
    {
        let owned = cfg!(unix);
        match *self {
            ClientMessage::StreamCreated(ref mut data) => {
                let handles =
                    f().expect("platform_handles must be available when processing StreamCreated");
                data.platform_handles = [
                    PlatformHandle::new(handles[0], owned),
                    PlatformHandle::new(handles[1], owned),
                    PlatformHandle::new(handles[2], owned),
                ]
            }
            ClientMessage::ContextSetupDeviceCollectionCallback(ref mut data) => {
                let handles = f().expect("platform_handles must be available when processing ContextSetupDeviceCollectionCallback");
                data.platform_handles = [
                    PlatformHandle::new(handles[0], owned),
                    PlatformHandle::new(handles[1], owned),
                    PlatformHandle::new(handles[2], owned),
                ]
            }
            _ => {}
        }
    }
}

#[cfg(test)]
mod test {
    use super::StreamParams;
    use cubeb::ffi;
    use std::mem;

    #[test]
    fn stream_params_size_check() {
        assert_eq!(
            mem::size_of::<StreamParams>(),
            mem::size_of::<ffi::cubeb_stream_params>()
        )
    }

    #[test]
    fn stream_params_from() {
        let mut raw = ffi::cubeb_stream_params::default();
        raw.format = ffi::CUBEB_SAMPLE_FLOAT32BE;
        raw.rate = 96_000;
        raw.channels = 32;
        raw.layout = ffi::CUBEB_LAYOUT_3F1_LFE;
        raw.prefs = ffi::CUBEB_STREAM_PREF_LOOPBACK;
        let wrapped = ::cubeb::StreamParams::from(raw);
        let params = StreamParams::from(wrapped.as_ref());
        assert_eq!(params.format, raw.format);
        assert_eq!(params.rate, raw.rate);
        assert_eq!(params.channels, raw.channels);
        assert_eq!(params.layout, raw.layout);
        assert_eq!(params.prefs, raw.prefs);
    }
}

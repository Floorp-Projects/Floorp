// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

#![allow(non_camel_case_types)]

use std::os::raw::{c_char, c_int, c_long, c_uint, c_void};

macro_rules! cubeb_enum {
    (pub enum $name:ident { $($variants:tt)* }) => {
        #[cfg(target_env = "msvc")]
        pub type $name = i32;
        #[cfg(not(target_env = "msvc"))]
        pub type $name = u32;
        cubeb_enum!(gen, $name, 0, $($variants)*);
    };
    (pub enum $name:ident: $t:ty { $($variants:tt)* }) => {
        pub type $name = $t;
        cubeb_enum!(gen, $name, 0, $($variants)*);
    };
    (gen, $name:ident, $val:expr, $variant:ident, $($rest:tt)*) => {
        pub const $variant: $name = $val;
        cubeb_enum!(gen, $name, $val+1, $($rest)*);
    };
    (gen, $name:ident, $val:expr, $variant:ident = $e:expr, $($rest:tt)*) => {
        pub const $variant: $name = $e;
        cubeb_enum!(gen, $name, $e+1, $($rest)*);
    };
    (gen, $name:ident, $val:expr, ) => {}
}

pub enum cubeb {}
pub enum cubeb_stream {}

cubeb_enum! {
    pub enum cubeb_sample_format {
        CUBEB_SAMPLE_S16LE,
        CUBEB_SAMPLE_S16BE,
        CUBEB_SAMPLE_FLOAT32LE,
        CUBEB_SAMPLE_FLOAT32BE,
    }
}

#[cfg(target_endian = "big")]
pub const CUBEB_SAMPLE_S16NE: cubeb_sample_format = CUBEB_SAMPLE_S16BE;
#[cfg(target_endian = "big")]
pub const CUBEB_SAMPLE_FLOAT32NE: cubeb_sample_format = CUBEB_SAMPLE_FLOAT32BE;
#[cfg(target_endian = "little")]
pub const CUBEB_SAMPLE_S16NE: cubeb_sample_format = CUBEB_SAMPLE_S16LE;
#[cfg(target_endian = "little")]
pub const CUBEB_SAMPLE_FLOAT32NE: cubeb_sample_format = CUBEB_SAMPLE_FLOAT32LE;

pub type cubeb_devid = *const c_void;

cubeb_enum! {
    pub enum cubeb_log_level: c_int {
        CUBEB_LOG_DISABLED = 0,
        CUBEB_LOG_NORMAL = 1,
        CUBEB_LOG_VERBOSE = 2,
    }
}


cubeb_enum! {
    pub enum cubeb_channel_layout: c_int {
        CUBEB_LAYOUT_UNDEFINED,
        CUBEB_LAYOUT_DUAL_MONO,
        CUBEB_LAYOUT_DUAL_MONO_LFE,
        CUBEB_LAYOUT_MONO,
        CUBEB_LAYOUT_MONO_LFE,
        CUBEB_LAYOUT_STEREO,
        CUBEB_LAYOUT_STEREO_LFE,
        CUBEB_LAYOUT_3F,
        CUBEB_LAYOUT_3F_LFE,
        CUBEB_LAYOUT_2F1,
        CUBEB_LAYOUT_2F1_LFE,
        CUBEB_LAYOUT_3F1,
        CUBEB_LAYOUT_3F1_LFE,
        CUBEB_LAYOUT_2F2,
        CUBEB_LAYOUT_2F2_LFE,
        CUBEB_LAYOUT_3F2,
        CUBEB_LAYOUT_3F2_LFE,
        CUBEB_LAYOUT_3F3R_LFE,
        CUBEB_LAYOUT_3F4_LFE,
        CUBEB_LAYOUT_MAX,
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct cubeb_stream_params {
    pub format: cubeb_sample_format,
    pub rate: c_uint,
    pub channels: c_uint,
    pub layout: cubeb_channel_layout
}

#[repr(C)]
#[derive(Debug)]
pub struct cubeb_device {
    pub output_name: *const c_char,
    pub input_name: *const c_char
}

cubeb_enum! {
    pub enum cubeb_state: c_int {
        CUBEB_STATE_STARTED,
        CUBEB_STATE_STOPPED,
        CUBEB_STATE_DRAINED,
        CUBEB_STATE_ERROR,
    }
}

cubeb_enum! {
    pub enum cubeb_error_code: c_int {
        CUBEB_OK = 0,
        CUBEB_ERROR = -1,
        CUBEB_ERROR_INVALID_FORMAT = -2,
        CUBEB_ERROR_INVALID_PARAMETER = -3,
        CUBEB_ERROR_NOT_SUPPORTED = -4,
        CUBEB_ERROR_DEVICE_UNAVAILABLE = -5,
    }
}

cubeb_enum! {
    pub enum cubeb_device_type {
        CUBEB_DEVICE_TYPE_UNKNOWN,
        CUBEB_DEVICE_TYPE_INPUT,
        CUBEB_DEVICE_TYPE_OUTPUT,
    }
}

cubeb_enum! {
    pub enum cubeb_device_state {
        CUBEB_DEVICE_STATE_DISABLED,
        CUBEB_DEVICE_STATE_UNPLUGGED,
        CUBEB_DEVICE_STATE_ENABLED,
    }
}

cubeb_enum! {
    pub enum cubeb_device_fmt {
        CUBEB_DEVICE_FMT_S16LE          = 0x0010,
        CUBEB_DEVICE_FMT_S16BE          = 0x0020,
        CUBEB_DEVICE_FMT_F32LE          = 0x1000,
        CUBEB_DEVICE_FMT_F32BE          = 0x2000,
    }
}

#[cfg(target_endian = "big")]
pub const CUBEB_DEVICE_FMT_S16NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_S16BE;
#[cfg(target_endian = "big")]
pub const CUBEB_DEVICE_FMT_F32NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_F32BE;
#[cfg(target_endian = "little")]
pub const CUBEB_DEVICE_FMT_S16NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_S16LE;
#[cfg(target_endian = "little")]
pub const CUBEB_DEVICE_FMT_F32NE: cubeb_device_fmt = CUBEB_DEVICE_FMT_F32LE;

pub const CUBEB_DEVICE_FMT_S16_MASK: cubeb_device_fmt = (CUBEB_DEVICE_FMT_S16LE | CUBEB_DEVICE_FMT_S16BE);
pub const CUBEB_DEVICE_FMT_F32_MASK: cubeb_device_fmt = (CUBEB_DEVICE_FMT_F32LE | CUBEB_DEVICE_FMT_F32BE);
pub const CUBEB_DEVICE_FMT_ALL: cubeb_device_fmt = (CUBEB_DEVICE_FMT_S16_MASK | CUBEB_DEVICE_FMT_F32_MASK);

cubeb_enum! {
    pub enum cubeb_device_pref  {
        CUBEB_DEVICE_PREF_NONE          = 0x00,
        CUBEB_DEVICE_PREF_MULTIMEDIA    = 0x01,
        CUBEB_DEVICE_PREF_VOICE         = 0x02,
        CUBEB_DEVICE_PREF_NOTIFICATION  = 0x04,
        CUBEB_DEVICE_PREF_ALL           = 0x0F,
    }
}

#[repr(C)]
#[derive(Debug)]
pub struct cubeb_device_info {
    pub devid: cubeb_devid,
    pub device_id: *const c_char,
    pub friendly_name: *const c_char,
    pub group_id: *const c_char,
    pub vendor_name: *const c_char,

    pub device_type: cubeb_device_type,
    pub state: cubeb_device_state,
    pub preferred: cubeb_device_pref,

    pub format: cubeb_device_fmt,
    pub default_format: cubeb_device_fmt,
    pub max_channels: c_uint,
    pub default_rate: c_uint,
    pub max_rate: c_uint,
    pub min_rate: c_uint,

    pub latency_lo: c_uint,
    pub latency_hi: c_uint
}

#[repr(C)]
#[derive(Debug)]
pub struct cubeb_device_collection {
    pub device: *const cubeb_device_info,
    pub count: usize
}

pub type cubeb_data_callback = extern "C" fn(*mut cubeb_stream,
                                             *mut c_void,
                                             *const c_void,
                                             *mut c_void,
                                             c_long)
                                             -> c_long;

pub type cubeb_state_callback = extern "C" fn(*mut cubeb_stream, *mut c_void, cubeb_state);
pub type cubeb_device_changed_callback = extern "C" fn(*mut c_void);
pub type cubeb_device_collection_changed_callback = extern "C" fn(*mut cubeb, *mut c_void);
pub type cubeb_log_callback = extern "C" fn(*const c_char, ...);

#[cfg(test)]
mod test {
    use std::mem::{align_of, size_of};
    #[test]
    fn test_layout_cubeb_stream_params() {
        use super::cubeb_stream_params;
        assert_eq!(
            size_of::<cubeb_stream_params>(),
            16usize,
            concat!("Size of: ", stringify!(cubeb_stream_params))
        );
        assert_eq!(
            align_of::<cubeb_stream_params>(),
            4usize,
            concat!("Alignment of ", stringify!(cubeb_stream_params))
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_stream_params)).format as *const _ as usize },
            0usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_stream_params),
                "::",
                stringify!(format)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_stream_params)).rate as *const _ as usize },
            4usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_stream_params),
                "::",
                stringify!(rate)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_stream_params)).channels as *const _ as usize },
            8usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_stream_params),
                "::",
                stringify!(channels)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_stream_params)).layout as *const _ as usize },
            12usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_stream_params),
                "::",
                stringify!(layout)
            )
        );
    }

    #[test]
    fn test_layout_cubeb_device() {
        use super::cubeb_device;
        assert_eq!(
            size_of::<cubeb_device>(),
            2 * size_of::<usize>(),
            concat!("Size of: ", stringify!(cubeb_device))
        );
        assert_eq!(
            align_of::<cubeb_device>(),
            align_of::<usize>(),
            concat!("Alignment of ", stringify!(cubeb_device))
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device)).output_name as *const _ as usize },
            0usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device),
                "::",
                stringify!(output_name)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device)).input_name as *const _ as usize },
            size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device),
                "::",
                stringify!(input_name)
            )
        );
    }

    #[test]
    fn test_layout_cubeb_device_info() {
        use super::cubeb_device_info;
        let psize: usize = size_of::<usize>();
        assert_eq!(
            size_of::<cubeb_device_info>(),
            (5 * psize + 44 + psize - 1) / psize * psize,
            concat!("Size of: ", stringify!(cubeb_device_info))
        );
        assert_eq!(
            align_of::<cubeb_device_info>(),
            align_of::<usize>(),
            concat!("Alignment of ", stringify!(cubeb_device_info))
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).devid as *const _ as usize },
            0usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(devid)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).device_id as *const _ as usize },
            size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(device_id)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).friendly_name as *const _ as usize },
            2 * size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(friendly_name)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).group_id as *const _ as usize },
            3 * size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(group_id)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).vendor_name as *const _ as usize },
            4 * size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(vendor_name)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).device_type as *const _ as usize },
            5 * size_of::<usize>() + 0,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(type_)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).state as *const _ as usize },
            5 * size_of::<usize>() + 4,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(state)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).preferred as *const _ as usize },
            5 * size_of::<usize>() + 8,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(preferred)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).format as *const _ as usize },
            5 * size_of::<usize>() + 12,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(format)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).default_format as *const _ as usize },
            5 * size_of::<usize>() + 16,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(default_format)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).max_channels as *const _ as usize },
            5 * size_of::<usize>() + 20,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(max_channels)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).default_rate as *const _ as usize },
            5 * size_of::<usize>() + 24,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(default_rate)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).max_rate as *const _ as usize },
            5 * size_of::<usize>() + 28,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(max_rate)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).min_rate as *const _ as usize },
            5 * size_of::<usize>() + 32,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(min_rate)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).latency_lo as *const _ as usize },
            5 * size_of::<usize>() + 36,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(latency_lo)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_info)).latency_hi as *const _ as usize },
            5 * size_of::<usize>() + 40,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_info),
                "::",
                stringify!(latency_hi)
            )
        );
    }

    #[test]
    fn test_layout_cubeb_device_collection() {
        use super::cubeb_device_collection;
        assert_eq!(
            size_of::<cubeb_device_collection>(),
            2 * size_of::<usize>(),
            concat!("Size of: ", stringify!(cubeb_device_collection))
        );
        assert_eq!(
            align_of::<cubeb_device_collection>(),
            align_of::<usize>(),
            concat!("Alignment of ", stringify!(cubeb_device_collection))
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_collection)).device as *const _ as usize },
            0usize,
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_collection),
                "::",
                stringify!(device)
            )
        );
        assert_eq!(
            unsafe { &(*(0 as *const cubeb_device_collection)).count as *const _ as usize },
            size_of::<usize>(),
            concat!(
                "Alignment of field: ",
                stringify!(cubeb_device_collection),
                "::",
                stringify!(count)
            )
        );
    }
}

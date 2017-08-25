use Error;
use std::os::raw::c_int;

macro_rules! call {
    (sys::$p:ident ($($e:expr),*)) => (
        sys::$p($(::call::convert(&$e)),*)
    )
}

macro_rules! try_call {
    (sys::$p:ident ($($e:expr),*)) => ({
        match ::call::try(sys::$p($(::call::convert(&$e)),*)) {
            Ok(o) => o,
            Err(e) => { return Err(e) }
        }
    })
}

pub fn try(ret: c_int) -> Result<c_int, Error> {
    match ret {
        n if n < 0 => Err(unsafe { Error::from_raw(n) }),
        n => Ok(n),
    }
}

#[doc(hidden)]
pub trait Convert<T> {
    fn convert(&self) -> T;
}

pub fn convert<T, U>(u: &U) -> T
where
    U: Convert<T>,
{
    u.convert()
}

mod impls {
    use call::Convert;
    use cubeb_core::{ChannelLayout, LogLevel, SampleFormat, State};
    use ffi;
    use std::ffi::CString;
    use std::os::raw::c_char;
    use std::ptr;

    impl<T: Copy> Convert<T> for T {
        fn convert(&self) -> T {
            *self
        }
    }

    impl<'a, T> Convert<*const T> for &'a T {
        fn convert(&self) -> *const T {
            &**self as *const _
        }
    }

    impl<'a, T> Convert<*mut T> for &'a mut T {
        fn convert(&self) -> *mut T {
            &**self as *const _ as *mut _
        }
    }

    impl<T> Convert<*const T> for *mut T {
        fn convert(&self) -> *const T {
            *self as *const T
        }
    }

    impl Convert<*const c_char> for CString {
        fn convert(&self) -> *const c_char {
            self.as_ptr()
        }
    }

    impl<T, U: Convert<*const T>> Convert<*const T> for Option<U> {
        fn convert(&self) -> *const T {
            self.as_ref().map(|s| s.convert()).unwrap_or(ptr::null())
        }
    }

    impl Convert<ffi::cubeb_sample_format> for SampleFormat {
        #[cfg_attr(feature = "cargo-clippy", allow(match_same_arms))]
        fn convert(&self) -> ffi::cubeb_sample_format {
            match *self {
                SampleFormat::S16LE => ffi::CUBEB_SAMPLE_S16LE,
                SampleFormat::S16BE => ffi::CUBEB_SAMPLE_S16BE,
                SampleFormat::S16NE => ffi::CUBEB_SAMPLE_S16NE,
                SampleFormat::Float32LE => ffi::CUBEB_SAMPLE_FLOAT32LE,
                SampleFormat::Float32BE => ffi::CUBEB_SAMPLE_FLOAT32BE,
                SampleFormat::Float32NE => ffi::CUBEB_SAMPLE_FLOAT32NE,
            }
        }
    }

    impl Convert<ffi::cubeb_log_level> for LogLevel {
        fn convert(&self) -> ffi::cubeb_log_level {
            match *self {
                LogLevel::Disabled => ffi::CUBEB_LOG_DISABLED,
                LogLevel::Normal => ffi::CUBEB_LOG_NORMAL,
                LogLevel::Verbose => ffi::CUBEB_LOG_VERBOSE,
            }
        }
    }


    impl Convert<ffi::cubeb_channel_layout> for ChannelLayout {
        fn convert(&self) -> ffi::cubeb_channel_layout {
            match *self {
                ChannelLayout::Undefined => ffi::CUBEB_LAYOUT_UNDEFINED,
                ChannelLayout::DualMono => ffi::CUBEB_LAYOUT_DUAL_MONO,
                ChannelLayout::DualMonoLfe => ffi::CUBEB_LAYOUT_DUAL_MONO_LFE,
                ChannelLayout::Mono => ffi::CUBEB_LAYOUT_MONO,
                ChannelLayout::MonoLfe => ffi::CUBEB_LAYOUT_MONO_LFE,
                ChannelLayout::Stereo => ffi::CUBEB_LAYOUT_STEREO,
                ChannelLayout::StereoLfe => ffi::CUBEB_LAYOUT_STEREO_LFE,
                ChannelLayout::Layout3F => ffi::CUBEB_LAYOUT_3F,
                ChannelLayout::Layout3FLfe => ffi::CUBEB_LAYOUT_3F_LFE,
                ChannelLayout::Layout2F1 => ffi::CUBEB_LAYOUT_2F1,
                ChannelLayout::Layout2F1Lfe => ffi::CUBEB_LAYOUT_2F1_LFE,
                ChannelLayout::Layout3F1 => ffi::CUBEB_LAYOUT_3F1,
                ChannelLayout::Layout3F1Lfe => ffi::CUBEB_LAYOUT_3F1_LFE,
                ChannelLayout::Layout2F2 => ffi::CUBEB_LAYOUT_2F2,
                ChannelLayout::Layout2F2Lfe => ffi::CUBEB_LAYOUT_2F2_LFE,
                ChannelLayout::Layout3F2 => ffi::CUBEB_LAYOUT_3F2,
                ChannelLayout::Layout3F2Lfe => ffi::CUBEB_LAYOUT_3F2_LFE,
                ChannelLayout::Layout3F3RLfe => ffi::CUBEB_LAYOUT_3F3R_LFE,
                ChannelLayout::Layout3F4Lfe => ffi::CUBEB_LAYOUT_3F4_LFE,
            }
        }
    }

    impl Convert<ffi::cubeb_state> for State {
        fn convert(&self) -> ffi::cubeb_state {
            {
                match *self {
                    State::Started => ffi::CUBEB_STATE_STARTED,
                    State::Stopped => ffi::CUBEB_STATE_STOPPED,
                    State::Drained => ffi::CUBEB_STATE_DRAINED,
                    State::Error => ffi::CUBEB_STATE_ERROR,
                }
            }
        }
    }
}

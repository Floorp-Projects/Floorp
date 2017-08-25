use {ChannelLayout, DeviceCollection, DeviceType, Result, Stream, StreamInitOptions, StreamParams};
use {ffi, sys};
use Binding;

use dev_coll;
use std::{ptr, str};
use std::ffi::CString;
use stream::{StreamCallback, stream_init};
use util::{opt_bytes, opt_cstr};

pub struct Context {
    raw: *mut ffi::cubeb
}

impl Context {
    pub fn init(context_name: &str, backend_name: Option<&str>) -> Result<Context> {
        let mut context: *mut ffi::cubeb = ptr::null_mut();
        let context_name = try!(CString::new(context_name));
        let backend_name = try!(opt_cstr(backend_name));
        unsafe {
            try_call!(sys::cubeb_init(&mut context, context_name, backend_name));
            Ok(Binding::from_raw(context))
        }
    }

    pub fn backend_id(&self) -> &str {
        str::from_utf8(self.backend_id_bytes()).unwrap()
    }
    pub fn backend_id_bytes(&self) -> &[u8] {
        unsafe { opt_bytes(self, call!(sys::cubeb_get_backend_id(self.raw))).unwrap() }
    }

    pub fn max_channel_count(&self) -> Result<u32> {
        let mut channel_count = 0u32;
        unsafe {
            try_call!(sys::cubeb_get_max_channel_count(
                self.raw,
                &mut channel_count
            ));
        }
        Ok(channel_count)
    }

    pub fn min_latency(&self, params: &StreamParams) -> Result<u32> {
        let mut latency = 0u32;
        unsafe {
            try_call!(sys::cubeb_get_min_latency(
                self.raw,
                params.raw(),
                &mut latency
            ));
        }
        Ok(latency)
    }

    pub fn preferred_sample_rate(&self) -> Result<u32> {
        let mut rate = 0u32;
        unsafe {
            try_call!(sys::cubeb_get_preferred_sample_rate(self.raw, &mut rate));
        }
        Ok(rate)
    }

    pub fn preferred_channel_layout(&self) -> Result<ChannelLayout> {
        let mut layout: ffi::cubeb_channel_layout = ffi::CUBEB_LAYOUT_UNDEFINED;
        unsafe {
            try_call!(sys::cubeb_get_preferred_channel_layout(
                self.raw,
                &mut layout
            ));
        }
        macro_rules! check( ($($raw:ident => $real:ident),*) => (
            $(if layout == ffi::$raw {
                Ok(ChannelLayout::$real)
            }) else *
            else {
                panic!("unknown channel layout: {}", layout)
            }
        ));

        check!(
            CUBEB_LAYOUT_UNDEFINED => Undefined,
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
            CUBEB_LAYOUT_3F4_LFE => Layout3F4Lfe
        )
    }

    /// Initialize a stream associated with the supplied application context.
    pub fn stream_init<CB>(&self, opts: &StreamInitOptions, cb: CB) -> Result<Stream<CB>>
    where
        CB: StreamCallback,
    {
        stream_init(self, opts, cb)
    }

    pub fn enumerate_devices(&self, devtype: DeviceType) -> Result<DeviceCollection> {
        dev_coll::enumerate(self, devtype)
    }

    /*
    pub fn register_device_collection_changed(
        &self,
        devtype: DeviceType,
        callback: &mut DeviceCollectionChangedCb,
        user_ptr: *mut c_void,
    ) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_register_device_collection_changed(self.raw, devtype, cb));
        }

        Ok(())
    }
*/
}

impl Binding for Context {
    type Raw = *mut ffi::cubeb;
    unsafe fn from_raw(raw: *mut ffi::cubeb) -> Self {
        Self {
            raw: raw
        }
    }
    fn raw(&self) -> Self::Raw {
        self.raw
    }
}

impl Drop for Context {
    fn drop(&mut self) {
        unsafe { sys::cubeb_destroy(self.raw) }
    }
}

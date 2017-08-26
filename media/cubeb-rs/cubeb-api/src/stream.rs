//! Stream Functions
//!
//! # Example
//! ```no_run
//! extern crate cubeb;
//! use std::time::Duration;
//! use std::thread;
//!
//! struct SquareWave {
//!     phase_inc: f32,
//!     phase: f32,
//!     volume: f32
//! }
//!
//! impl cubeb::StreamCallback for SquareWave {
//!    type Frame = cubeb::MonoFrame<f32>;
//!
//!    fn data_callback(&mut self, _: &[cubeb::MonoFrame<f32>], output: &mut [cubeb::MonoFrame<f32>]) -> isize {
//!        // Generate a square wave
//!        for x in output.iter_mut() {
//!            x.m = if self.phase < 0.5 {
//!                self.volume
//!            } else {
//!                -self.volume
//!            };
//!            self.phase = (self.phase + self.phase_inc) % 1.0;
//!        }
//!
//!        output.len() as isize
//!    }
//!
//!    fn state_callback(&mut self, state: cubeb::State) { println!("stream {:?}", state); }
//! }
//!
//! fn main() {
//!     let ctx = cubeb::Context::init("Cubeb tone example", None).unwrap();
//!
//!     let params = cubeb::StreamParamsBuilder::new()
//!         .format(cubeb::SampleFormat::Float32LE)
//!         .rate(44100)
//!         .channels(1)
//!         .layout(cubeb::ChannelLayout::Mono)
//!         .take();
//!
//!     let stream_init_opts = cubeb::StreamInitOptionsBuilder::new()
//!         .stream_name("Cubeb Square Wave")
//!         .output_stream_param(&params)
//!         .latency(4096)
//!         .take();
//!
//!     let stream = ctx.stream_init(
//!         &stream_init_opts,
//!         SquareWave {
//!             phase_inc: 440.0 / 44100.0,
//!             phase: 0.0,
//!             volume: 0.25
//!         }).unwrap();
//!
//!     // Start playback
//!     stream.start().unwrap();
//!
//!     // Play for 1/2 second
//!     thread::sleep(Duration::from_millis(500));
//!
//!     // Shutdown
//!     stream.stop().unwrap();
//! }
//! ```

use {Binding, ChannelLayout, Context, Device, DeviceId, Error, Result,
     SampleFormat, State, StreamParams};
use ffi;
use std::{ptr, str};
use std::ffi::CString;
use std::os::raw::{c_long, c_void};
use sys;
use util::IntoCString;

/// An extension trait which allows the implementation of converting
/// void* buffers from libcubeb-sys into rust slices of the appropriate
/// type.
pub trait SampleType: Send + Copy {
    /// Type of the sample
    fn format() -> SampleFormat;
    /// Map f32 in range [-1,1] to sample type
    fn from_float(f32) -> Self;
}

impl SampleType for i16 {
    fn format() -> SampleFormat {
        SampleFormat::S16NE
    }
    fn from_float(x: f32) -> i16 {
        (x * i16::max_value() as f32) as i16
    }
}

impl SampleType for f32 {
    fn format() -> SampleFormat {
        SampleFormat::Float32NE
    }
    fn from_float(x: f32) -> f32 {
        x
    }
}

pub trait StreamCallback: Send + 'static
{
    type Frame;

    // This should return a Result<usize,Error>
    fn data_callback(&mut self, &[Self::Frame], &mut [Self::Frame]) -> isize;
    fn state_callback(&mut self, state: State);
}

///
pub struct StreamParamsBuilder {
    format: SampleFormat,
    rate: u32,
    channels: u32,
    layout: ChannelLayout
}

impl Default for StreamParamsBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl StreamParamsBuilder {
    pub fn new() -> Self {
        Self {
            format: SampleFormat::S16NE,
            rate: 0,
            channels: 0,
            layout: ChannelLayout::Undefined
        }
    }

    pub fn format(&mut self, format: SampleFormat) -> &mut Self {
        self.format = format;
        self
    }

    pub fn rate(&mut self, rate: u32) -> &mut Self {
        self.rate = rate;
        self
    }

    pub fn channels(&mut self, channels: u32) -> &mut Self {
        self.channels = channels;
        self
    }

    pub fn layout(&mut self, layout: ChannelLayout) -> &mut Self {
        self.layout = layout;
        self
    }

    pub fn take(&self) -> StreamParams {
        // Convert native endian types to matching format
        let raw_sample_format = match self.format {
            SampleFormat::S16LE => ffi::CUBEB_SAMPLE_S16LE,
            SampleFormat::S16BE => ffi::CUBEB_SAMPLE_S16BE,
            SampleFormat::S16NE => ffi::CUBEB_SAMPLE_S16NE,
            SampleFormat::Float32LE => ffi::CUBEB_SAMPLE_FLOAT32LE,
            SampleFormat::Float32BE => ffi::CUBEB_SAMPLE_FLOAT32BE,
            SampleFormat::Float32NE => ffi::CUBEB_SAMPLE_FLOAT32NE,
        };
        unsafe {
            Binding::from_raw(&ffi::cubeb_stream_params {
                format: raw_sample_format,
                rate: self.rate,
                channels: self.channels,
                layout: self.layout as ffi::cubeb_channel_layout
            } as *const _)
        }
    }
}

///
pub struct Stream<CB>
where
    CB: StreamCallback,
{
    raw: *mut ffi::cubeb_stream,
    cbs: Box<CB>
}

impl<CB> Stream<CB>
where
    CB: StreamCallback,
{
    fn init(context: &Context, opts: &StreamInitOptions, cb: CB) -> Result<Stream<CB>> {
        let mut stream: *mut ffi::cubeb_stream = ptr::null_mut();

        let cbs = Box::new(cb);

        unsafe {
            let input_stream_params = opts.input_stream_params
                .as_ref()
                .map(|s| s.raw())
                .unwrap_or(ptr::null());

            let output_stream_params = opts.output_stream_params
                .as_ref()
                .map(|s| s.raw())
                .unwrap_or(ptr::null());

            let user_ptr: *mut c_void = &*cbs as *const _ as *mut _;

            try_call!(sys::cubeb_stream_init(
                context.raw(),
                &mut stream,
                opts.stream_name,
                opts.input_device.raw(),
                input_stream_params,
                opts.output_device.raw(),
                output_stream_params,
                opts.latency_frames,
                Stream::<CB>::data_cb_c,
                Stream::<CB>::state_cb_c,
                user_ptr
            ));
        }

        Ok(Stream {
            raw: stream,
            cbs: cbs
        })
    }

    // start playback.
    pub fn start(&self) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_stream_start(self.raw));
        }
        Ok(())
    }

    // Stop playback.
    pub fn stop(&self) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_stream_stop(self.raw));
        }
        Ok(())
    }

    // Get the current stream playback position.
    pub fn position(&self) -> Result<u64> {
        let mut position: u64 = 0;
        unsafe {
            try_call!(sys::cubeb_stream_get_position(self.raw, &mut position));
        }
        Ok(position)
    }

    pub fn latency(&self) -> Result<u32> {
        let mut latency: u32 = 0;
        unsafe {
            try_call!(sys::cubeb_stream_get_latency(self.raw, &mut latency));
        }
        Ok(latency)
    }

    pub fn set_volume(&self, volume: f32) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_stream_set_volume(self.raw, volume));
        }
        Ok(())
    }

    pub fn set_panning(&self, panning: f32) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_stream_set_panning(self.raw, panning));
        }
        Ok(())
    }

    pub fn current_device(&self) -> Result<Device> {
        let mut device_ptr: *const ffi::cubeb_device = ptr::null();
        unsafe {
            try_call!(sys::cubeb_stream_get_current_device(
                self.raw,
                &mut device_ptr
            ));
            Binding::from_raw_opt(device_ptr).ok_or(Error::from_raw(ffi::CUBEB_ERROR))
        }
    }

    pub fn destroy_device(&self, device: Device) -> Result<()> {
        unsafe {
            try_call!(sys::cubeb_stream_device_destroy(self.raw, device.raw()));
        }
        Ok(())
    }

    /*
    pub fn register_device_changed_callback(&self, device_changed_cb: &mut DeviceChangedCb) -> Result<(), Error> {
        unsafe { try_call!(sys::cubeb_stream_register_device_changed_callback(self.raw, ...)); }
        Ok(())
    }
*/

    // C callable callbacks
    extern "C" fn data_cb_c(
        _: *mut ffi::cubeb_stream,
        user_ptr: *mut c_void,
        input_buffer: *const c_void,
        output_buffer: *mut c_void,
        nframes: c_long,
    ) -> c_long {
        use std::slice::{from_raw_parts, from_raw_parts_mut};

        unsafe {
            let cbs = &mut *(user_ptr as *mut CB);
            let input: &[CB::Frame] = if input_buffer.is_null() {
                &[]
            } else {
                from_raw_parts(input_buffer as *const _, nframes as usize)
            };
            let mut output: &mut [CB::Frame] = if output_buffer.is_null() {
                &mut []
            } else {
                from_raw_parts_mut(output_buffer as *mut _, nframes as usize)
            };
            cbs.data_callback(input, output) as c_long
        }
    }

    extern "C" fn state_cb_c(_: *mut ffi::cubeb_stream, user_ptr: *mut c_void, state: ffi::cubeb_state) {
        let state = match state {
            ffi::CUBEB_STATE_STARTED => State::Started,
            ffi::CUBEB_STATE_STOPPED => State::Stopped,
            ffi::CUBEB_STATE_DRAINED => State::Drained,
            ffi::CUBEB_STATE_ERROR => State::Error,
            n => panic!("unknown state: {}", n),
        };
        unsafe {
            let cbs = &mut *(user_ptr as *mut CB);
            cbs.state_callback(state);
        };
    }
}

impl<CB> Drop for Stream<CB>
where
    CB: StreamCallback,
{
    fn drop(&mut self) {
        unsafe {
            sys::cubeb_stream_destroy(self.raw);
        }
    }
}

#[doc(hidden)]
pub fn stream_init<CB>(context: &Context, opts: &StreamInitOptions, cb: CB) -> Result<Stream<CB>>
where
    CB: StreamCallback,
{
    Stream::init(context, opts, cb)
}

pub struct StreamInitOptions {
    pub stream_name: Option<CString>,
    pub input_device: DeviceId,
    pub input_stream_params: Option<StreamParams>,
    pub output_device: DeviceId,
    pub output_stream_params: Option<StreamParams>,
    pub latency_frames: u32
}

impl StreamInitOptions {
    pub fn new() -> Self {
        StreamInitOptions {
            stream_name: None,
            input_device: DeviceId::default(),
            input_stream_params: None,
            output_device: DeviceId::default(),
            output_stream_params: None,
            latency_frames: 0
        }
    }
}

impl Default for StreamInitOptions {
    fn default() -> Self {
        Self::new()
    }
}

/// Structure describing options about how stream should be initialized.
pub struct StreamInitOptionsBuilder {
    opts: StreamInitOptions
}

impl Default for StreamInitOptionsBuilder {
    fn default() -> Self {
        Self::new()
    }
}

impl StreamInitOptionsBuilder {
    pub fn new() -> Self {
        StreamInitOptionsBuilder {
            opts: Default::default()
        }
    }

    pub fn stream_name<S>(&mut self, name: S) -> &mut Self
    where
        S: IntoCString,
    {
        self.opts.stream_name = Some(name.into_c_string().unwrap());
        self
    }

    pub fn input_device(&mut self, id: DeviceId) -> &mut Self {
        self.opts.input_device = id;
        self
    }

    pub fn input_stream_param(&mut self, param: &StreamParams) -> &mut Self {
        self.opts.input_stream_params = Some(*param);
        self
    }

    pub fn output_device(&mut self, id: DeviceId) -> &mut Self {
        self.opts.output_device = id;
        self
    }

    pub fn output_stream_param(&mut self, param: &StreamParams) -> &mut Self {
        self.opts.output_stream_params = Some(*param);
        self
    }

    pub fn latency(&mut self, latency: u32) -> &mut Self {
        self.opts.latency_frames = latency;
        self
    }

    pub fn take(&mut self) -> StreamInitOptions {
        use std::mem::replace;
        replace(&mut self.opts, Default::default())
    }
}

#[cfg(test)]
mod tests {
    use {StreamParamsBuilder, ffi};
    use cubeb_core::binding::Binding;

    #[test]
    fn stream_params_builder_channels() {
        let params = StreamParamsBuilder::new().channels(2).take();
        assert_eq!(params.channels(), 2);
    }

    #[test]
    fn stream_params_builder_format() {
        macro_rules! check(
            ($($real:ident),*) => (
                $(let params = StreamParamsBuilder::new()
                  .format(super::SampleFormat::$real)
                  .take();
                assert_eq!(params.format(), super::SampleFormat::$real);
                )*
            ) );

        check!(S16LE, S16BE, Float32LE, Float32BE);
    }

    #[test]
    fn stream_params_builder_format_native_endian() {
        let params = StreamParamsBuilder::new()
            .format(super::SampleFormat::S16NE)
            .take();
        assert_eq!(
            params.format(),
            if cfg!(target_endian = "little") {
                super::SampleFormat::S16LE
            } else {
                super::SampleFormat::S16BE
            }
        );

        let params = StreamParamsBuilder::new()
            .format(super::SampleFormat::Float32NE)
            .take();
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
    fn stream_params_builder_layout() {
        macro_rules! check(
            ($($real:ident),*) => (
                $(let params = StreamParamsBuilder::new()
                  .layout(super::ChannelLayout::$real)
                  .take();
                assert_eq!(params.layout(), super::ChannelLayout::$real);
                )*
            ) );

        check!(
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
            Layout3F3RLfe,
            Layout3F4Lfe
        );
    }

    #[test]
    fn stream_params_builder_rate() {
        let params = StreamParamsBuilder::new().rate(44100).take();
        assert_eq!(params.rate(), 44100);
    }

    #[test]
    fn stream_params_builder_to_raw_channels() {
        let params = StreamParamsBuilder::new().channels(2).take();
        let raw = unsafe { &*params.raw() };
        assert_eq!(raw.channels, 2);
    }

    #[test]
    fn stream_params_builder_to_raw_format() {
        macro_rules! check(
            ($($real:ident => $raw:ident),*) => (
                $(let params = super::StreamParamsBuilder::new()
                  .format(super::SampleFormat::$real)
                  .take();
                  let raw = unsafe { &*params.raw() };
                  assert_eq!(raw.format, ffi::$raw);
                )*
            ) );

        check!(S16LE => CUBEB_SAMPLE_S16LE,
               S16BE => CUBEB_SAMPLE_S16BE,
               Float32LE => CUBEB_SAMPLE_FLOAT32LE,
               Float32BE => CUBEB_SAMPLE_FLOAT32BE);
    }

    #[test]
    fn stream_params_builder_format_to_raw_native_endian() {
        let params = StreamParamsBuilder::new()
            .format(super::SampleFormat::S16NE)
            .take();
        let raw = unsafe { &*params.raw() };
        assert_eq!(
            raw.format,
            if cfg!(target_endian = "little") {
                ffi::CUBEB_SAMPLE_S16LE
            } else {
                ffi::CUBEB_SAMPLE_S16BE
            }
        );

        let params = StreamParamsBuilder::new()
            .format(super::SampleFormat::Float32NE)
            .take();
        let raw = unsafe { &*params.raw() };
        assert_eq!(
            raw.format,
            if cfg!(target_endian = "little") {
                ffi::CUBEB_SAMPLE_FLOAT32LE
            } else {
                ffi::CUBEB_SAMPLE_FLOAT32BE
            }
        );
    }

    #[test]
    fn stream_params_builder_to_raw_layout() {
        macro_rules! check(
            ($($real:ident => $raw:ident),*) => (
                $(let params = super::StreamParamsBuilder::new()
                  .layout(super::ChannelLayout::$real)
                  .take();
                  let raw = unsafe { &*params.raw() };
                  assert_eq!(raw.layout, ffi::$raw);
                )*
            ) );

        check!(Undefined => CUBEB_LAYOUT_UNDEFINED,
               DualMono => CUBEB_LAYOUT_DUAL_MONO,
               DualMonoLfe => CUBEB_LAYOUT_DUAL_MONO_LFE,
               Mono => CUBEB_LAYOUT_MONO,
               MonoLfe => CUBEB_LAYOUT_MONO_LFE,
               Stereo => CUBEB_LAYOUT_STEREO,
               StereoLfe => CUBEB_LAYOUT_STEREO_LFE,
               Layout3F => CUBEB_LAYOUT_3F,
               Layout3FLfe => CUBEB_LAYOUT_3F_LFE,
               Layout2F1 => CUBEB_LAYOUT_2F1,
               Layout2F1Lfe => CUBEB_LAYOUT_2F1_LFE,
               Layout3F1 => CUBEB_LAYOUT_3F1,
               Layout3F1Lfe => CUBEB_LAYOUT_3F1_LFE,
               Layout2F2 => CUBEB_LAYOUT_2F2,
               Layout2F2Lfe => CUBEB_LAYOUT_2F2_LFE,
               Layout3F2 => CUBEB_LAYOUT_3F2,
               Layout3F2Lfe => CUBEB_LAYOUT_3F2_LFE,
               Layout3F3RLfe => CUBEB_LAYOUT_3F3R_LFE,
               Layout3F4Lfe => CUBEB_LAYOUT_3F4_LFE);
    }

    #[test]
    fn stream_params_builder_to_raw_rate() {
        let params = StreamParamsBuilder::new().rate(44100).take();
        let raw = unsafe { &*params.raw() };
        assert_eq!(raw.rate, 44100);
    }
}

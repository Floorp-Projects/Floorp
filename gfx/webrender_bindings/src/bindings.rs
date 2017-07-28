use std::collections::HashSet;
use std::ffi::CString;
use std::{mem, slice};
use std::path::PathBuf;
use std::sync::Arc;
use std::os::raw::{c_void, c_char, c_float};
use gleam::gl;

use webrender_api::*;
use webrender::renderer::{ReadPixelsFormat, Renderer, RendererOptions};
use webrender::renderer::{ExternalImage, ExternalImageHandler, ExternalImageSource};
use webrender::{ApiRecordingReceiver, BinaryRecorder};
use thread_profiler::register_thread_with_profiler;
use moz2d_renderer::Moz2dImageRenderer;
use app_units::Au;
use rayon;
use euclid::SideOffsets2D;

extern crate webrender_api;

/// cbindgen:field-names=[mNamespace, mHandle]
type WrExternalImageBufferType = ExternalImageType;

/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
type WrEpoch = Epoch;
/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
type WrIdNamespace = IdNamespace;

/// cbindgen:field-names=[mNamespace, mHandle]
type WrPipelineId = PipelineId;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrImageKey = ImageKey;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrFontKey = FontKey;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrYuvColorSpace = YuvColorSpace;

fn make_slice<'a, T>(ptr: *const T, len: usize) -> &'a [T] {
    if ptr.is_null() {
        &[]
    } else {
        unsafe { slice::from_raw_parts(ptr, len) }
    }
}

fn make_slice_mut<'a, T>(ptr: *mut T, len: usize) -> &'a mut [T] {
    if ptr.is_null() {
        &mut []
    } else {
        unsafe { slice::from_raw_parts_mut(ptr, len) }
    }
}

#[repr(C)]
pub struct WrVecU8 {
    data: *mut u8,
    length: usize,
    capacity: usize,
}

impl WrVecU8 {
    fn to_vec(self) -> Vec<u8> {
        unsafe { Vec::from_raw_parts(self.data, self.length, self.capacity) }
    }
    fn from_vec(mut v: Vec<u8>) -> WrVecU8 {
        let w = WrVecU8 {
            data: v.as_mut_ptr(),
            length: v.len(),
            capacity: v.capacity(),
        };
        mem::forget(v);
        w
    }
}

#[no_mangle]
pub extern "C" fn wr_vec_u8_free(v: WrVecU8) {
    v.to_vec();
}

#[repr(C)]
pub struct ByteSlice {
    buffer: *const u8,
    len: usize,
}

impl ByteSlice {
    pub fn new(slice: &[u8]) -> ByteSlice {
        ByteSlice {
            buffer: &slice[0],
            len: slice.len(),
        }
    }

    pub fn as_slice(&self) -> &[u8] {
        make_slice(self.buffer, self.len)
    }
}

#[repr(C)]
pub struct MutByteSlice {
    buffer: *mut u8,
    len: usize,
}

impl MutByteSlice {
    pub fn new(slice: &mut [u8]) -> MutByteSlice {
        let len = slice.len();
        MutByteSlice {
            buffer: &mut slice[0],
            len: len,
        }
    }

    pub fn as_mut_slice(&mut self) -> &mut [u8] {
        make_slice_mut(self.buffer, self.len)
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct WrImageMask {
    image: WrImageKey,
    rect: LayoutRect,
    repeat: bool,
}

impl Into<ImageMask> for WrImageMask {
    fn into(self) -> ImageMask {
        ImageMask {
            image: self.image,
            rect: self.rect.into(),
            repeat: self.repeat,
        }
    }
}
impl<'a> Into<ImageMask> for &'a WrImageMask {
    fn into(self) -> ImageMask {
        ImageMask {
            image: self.image,
            rect: self.rect.into(),
            repeat: self.repeat,
        }
    }
}
impl From<ImageMask> for WrImageMask {
    fn from(image_mask: ImageMask) -> Self {
        WrImageMask {
            image: image_mask.image,
            rect: image_mask.rect.into(),
            repeat: image_mask.repeat,
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct WrImageDescriptor {
    pub format: ImageFormat,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub is_opaque: bool,
}

impl<'a> Into<ImageDescriptor> for &'a WrImageDescriptor {
    fn into(self) -> ImageDescriptor {
        ImageDescriptor {
            width: self.width,
            height: self.height,
            stride: if self.stride != 0 {
                Some(self.stride)
            } else {
                None
            },
            format: self.format,
            is_opaque: self.is_opaque,
            offset: 0,
        }
    }
}

/// cbindgen:field-names=[mHandle]
#[repr(C)]
#[derive(Copy, Clone)]
pub struct WrExternalImageId(pub u64);

impl Into<ExternalImageId> for WrExternalImageId {
    fn into(self) -> ExternalImageId {
        ExternalImageId(self.0)
    }
}
impl Into<WrExternalImageId> for ExternalImageId {
    fn into(self) -> WrExternalImageId {
        WrExternalImageId(self.0)
    }
}

#[repr(u32)]
#[allow(dead_code)]
enum WrExternalImageType {
    NativeTexture,
    RawData,
}

#[repr(C)]
struct WrExternalImage {
    image_type: WrExternalImageType,

    // external texture handle
    handle: u32,
    // external texture coordinate
    u0: f32,
    v0: f32,
    u1: f32,
    v1: f32,

    // external image buffer
    buff: *const u8,
    size: usize,
}

type LockExternalImageCallback = fn(*mut c_void, WrExternalImageId, u8) -> WrExternalImage;
type UnlockExternalImageCallback = fn(*mut c_void, WrExternalImageId, u8);

#[repr(C)]
pub struct WrExternalImageHandler {
    external_image_obj: *mut c_void,
    lock_func: LockExternalImageCallback,
    unlock_func: UnlockExternalImageCallback,
}

impl ExternalImageHandler for WrExternalImageHandler {
    fn lock(&mut self,
            id: ExternalImageId,
            channel_index: u8)
            -> ExternalImage {
        let image = (self.lock_func)(self.external_image_obj, id.into(), channel_index);

        match image.image_type {
            WrExternalImageType::NativeTexture => {
                ExternalImage {
                    u0: image.u0,
                    v0: image.v0,
                    u1: image.u1,
                    v1: image.v1,
                    source: ExternalImageSource::NativeTexture(image.handle),
                }
            },
            WrExternalImageType::RawData => {
                ExternalImage {
                    u0: image.u0,
                    v0: image.v0,
                    u1: image.u1,
                    v1: image.v1,
                    source: ExternalImageSource::RawData(make_slice(image.buff, image.size)),
                }
            },
        }
    }

    fn unlock(&mut self,
              id: ExternalImageId,
              channel_index: u8) {
        (self.unlock_func)(self.external_image_obj, id.into(), channel_index);
    }
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct WrComplexClipRegion {
    rect: LayoutRect,
    radii: BorderRadius,
}

impl<'a> Into<ComplexClipRegion> for &'a WrComplexClipRegion {
    fn into(self) -> ComplexClipRegion {
        ComplexClipRegion {
            rect: self.rect.into(),
            radii: self.radii.into(),
        }
    }
}

#[repr(u32)]
#[derive(Copy, Clone)]
pub enum WrFilterOpType {
  Blur = 0,
  Brightness = 1,
  Contrast = 2,
  Grayscale = 3,
  HueRotate = 4,
  Invert = 5,
  Opacity = 6,
  Saturate = 7,
  Sepia = 8,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct WrFilterOp {
    filter_type: WrFilterOpType,
    argument: c_float,
}

/// cbindgen:derive-eq=false
#[repr(C)]
#[derive(Debug)]
pub struct WrTransformProperty {
    pub id: u64,
    pub transform: LayoutTransform,
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct WrOpacityProperty {
    pub id: u64,
    pub opacity: f32,
}

/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct WrWindowId(u64);

fn get_proc_address(glcontext_ptr: *mut c_void,
                    name: &str)
                    -> *const c_void {

    extern "C" {
        fn get_proc_address_from_glcontext(glcontext_ptr: *mut c_void,
                                           procname: *const c_char)
                                           -> *const c_void;
    }

    let symbol_name = CString::new(name).unwrap();
    let symbol = unsafe { get_proc_address_from_glcontext(glcontext_ptr, symbol_name.as_ptr()) };

    // For now panic, not sure we should be though or if we can recover
    if symbol.is_null() {
        // XXX Bug 1322949 Make whitelist for extensions
        println!("Could not find symbol {:?} by glcontext", symbol_name);
    }

    symbol as *const _
}

extern "C" {
    fn is_in_compositor_thread() -> bool;
    fn is_in_render_thread() -> bool;
    fn is_in_main_thread() -> bool;
    fn is_glcontext_egl(glcontext_ptr: *mut c_void) -> bool;
    // Enables binary recording that can be used with `wrench replay`
    // Outputs a wr-record-*.bin file for each window that is shown
    // Note: wrench will panic if external images are used, they can
    // be disabled in WebRenderBridgeParent::ProcessWebRenderCommands
    // by commenting out the path that adds an external image ID
    fn gfx_use_wrench() -> bool;
    fn gfx_critical_note(msg: *const c_char);
}

struct CppNotifier {
    window_id: WrWindowId,
}

unsafe impl Send for CppNotifier {}

extern "C" {
    fn wr_notifier_new_frame_ready(window_id: WrWindowId);
    fn wr_notifier_new_scroll_frame_ready(window_id: WrWindowId,
                                          composite_needed: bool);
    fn wr_notifier_external_event(window_id: WrWindowId,
                                  raw_event: usize);
}

impl webrender_api::RenderNotifier for CppNotifier {
    fn new_frame_ready(&mut self) {
        unsafe {
            wr_notifier_new_frame_ready(self.window_id);
        }
    }

    fn new_scroll_frame_ready(&mut self,
                              composite_needed: bool) {
        unsafe {
            wr_notifier_new_scroll_frame_ready(self.window_id, composite_needed);
        }
    }

    fn external_event(&mut self,
                      event: ExternalEvent) {
        unsafe {
            wr_notifier_external_event(self.window_id, event.unwrap());
        }
    }
}

#[no_mangle]
pub extern "C" fn wr_renderer_set_external_image_handler(renderer: &mut Renderer,
                                                         external_image_handler: *mut WrExternalImageHandler) {
    if !external_image_handler.is_null() {
        renderer.set_external_image_handler(Box::new(unsafe {
                                                         WrExternalImageHandler {
                                                             external_image_obj:
                                                                 (*external_image_handler).external_image_obj,
                                                             lock_func: (*external_image_handler).lock_func,
                                                             unlock_func: (*external_image_handler).unlock_func,
                                                         }
                                                     }));
    }
}

#[no_mangle]
pub extern "C" fn wr_renderer_update(renderer: &mut Renderer) {
    renderer.update();
}

#[no_mangle]
pub extern "C" fn wr_renderer_render(renderer: &mut Renderer,
                                     width: u32,
                                     height: u32) {
    renderer.render(DeviceUintSize::new(width, height));
}

// Call wr_renderer_render() before calling this function.
#[no_mangle]
pub unsafe extern "C" fn wr_renderer_readback(renderer: &mut Renderer,
                                              width: u32,
                                              height: u32,
                                              dst_buffer: *mut u8,
                                              buffer_size: usize) {
    assert!(is_in_render_thread());

    let mut slice = make_slice_mut(dst_buffer, buffer_size);
    renderer.read_pixels_into(DeviceUintRect::new(
                                DeviceUintPoint::new(0, 0),
                                DeviceUintSize::new(width, height)),
                              ReadPixelsFormat::Bgra8,
                              &mut slice);
}

#[no_mangle]
pub extern "C" fn wr_renderer_set_profiler_enabled(renderer: &mut Renderer,
                                                   enabled: bool) {
    renderer.set_profiler_enabled(enabled);
}

#[no_mangle]
pub extern "C" fn wr_renderer_current_epoch(renderer: &mut Renderer,
                                            pipeline_id: WrPipelineId,
                                            out_epoch: &mut WrEpoch)
                                            -> bool {
    if let Some(epoch) = renderer.current_epoch(pipeline_id) {
        *out_epoch = epoch;
        return true;
    }
    return false;
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_renderer_delete(renderer: *mut Renderer) {
    Box::from_raw(renderer);
}

pub struct WrRenderedEpochs {
    data: Vec<(WrPipelineId, WrEpoch)>,
}

#[no_mangle]
pub unsafe extern "C" fn wr_renderer_flush_rendered_epochs(renderer: &mut Renderer) -> *mut WrRenderedEpochs {
    let map = renderer.flush_rendered_epochs();
    let pipeline_epochs = Box::new(WrRenderedEpochs {
                                       data: map.into_iter().collect(),
                                   });
    return Box::into_raw(pipeline_epochs);
}

#[no_mangle]
pub unsafe extern "C" fn wr_rendered_epochs_next(pipeline_epochs: &mut WrRenderedEpochs,
                                                 out_pipeline: &mut WrPipelineId,
                                                 out_epoch: &mut WrEpoch)
                                                 -> bool {
    if let Some((pipeline, epoch)) = pipeline_epochs.data.pop() {
        *out_pipeline = pipeline;
        *out_epoch = epoch;
        return true;
    }
    return false;
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_rendered_epochs_delete(pipeline_epochs: *mut WrRenderedEpochs) {
    Box::from_raw(pipeline_epochs);
}

pub struct WrThreadPool(Arc<rayon::ThreadPool>);

#[no_mangle]
pub unsafe extern "C" fn wr_thread_pool_new() -> *mut WrThreadPool {
    let worker_config = rayon::Configuration::new()
        .thread_name(|idx|{ format!("WebRender:Worker#{}", idx) })
        .start_handler(|idx| {
            register_thread_with_profiler(format!("WebRender:Worker#{}", idx));
        });

    let workers = Arc::new(rayon::ThreadPool::new(worker_config).unwrap());        

    Box::into_raw(Box::new(WrThreadPool(workers)))
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_thread_pool_delete(thread_pool: *mut WrThreadPool) {
    Box::from_raw(thread_pool);
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern "C" fn wr_window_new(window_id: WrWindowId,
                                window_width: u32,
                                window_height: u32,
                                gl_context: *mut c_void,
                                thread_pool: *mut WrThreadPool,
                                enable_profiler: bool,
                                out_api: &mut *mut RenderApi,
                                out_renderer: &mut *mut Renderer)
                                -> bool {
    assert!(unsafe { is_in_render_thread() });

    let recorder: Option<Box<ApiRecordingReceiver>> = if unsafe { gfx_use_wrench() } {
        let name = format!("wr-record-{}.bin", window_id.0);
        Some(Box::new(BinaryRecorder::new(&PathBuf::from(name))))
    } else {
        None
    };

    let gl;
    if unsafe { is_glcontext_egl(gl_context) } {
        gl = unsafe { gl::GlesFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    } else {
        gl = unsafe { gl::GlFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    }
    gl.clear_color(0.3, 0.0, 0.0, 1.0);

    let version = gl.get_string(gl::VERSION);

    println!("WebRender - OpenGL version new {}", version);

    let workers = unsafe {
        Arc::clone(&(*thread_pool).0)
    };

    let opts = RendererOptions {
        enable_aa: true,
        enable_subpixel_aa: true,
        enable_profiler: enable_profiler,
        recorder: recorder,
        blob_image_renderer: Some(Box::new(Moz2dImageRenderer::new(workers.clone()))),
        workers: Some(workers.clone()),
        cache_expiry_frames: 60, // see https://github.com/servo/webrender/pull/1294#issuecomment-304318800
        ..Default::default()
    };

    let window_size = DeviceUintSize::new(window_width, window_height);
    let (renderer, sender) = match Renderer::new(gl, opts, window_size) {
        Ok((renderer, sender)) => (renderer, sender),
        Err(e) => {
            println!(" Failed to create a Renderer: {:?}", e);
            let msg = CString::new(format!("wr_window_new: {:?}", e)).unwrap();
            unsafe {
                gfx_critical_note(msg.as_ptr());
            }
            return false;
        },
    };

    renderer.set_render_notifier(Box::new(CppNotifier {
                                              window_id: window_id,
                                          }));

    *out_api = Box::into_raw(Box::new(sender.create_api()));
    *out_renderer = Box::into_raw(Box::new(renderer));

    return true;
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_api_delete(api: *mut RenderApi) {
    let api = Box::from_raw(api);
    api.shut_down();
}

#[no_mangle]
pub extern "C" fn wr_api_add_image(api: &mut RenderApi,
                                   image_key: WrImageKey,
                                   descriptor: &WrImageDescriptor,
                                   bytes: ByteSlice) {
    assert!(unsafe { is_in_compositor_thread() });
    let copied_bytes = bytes.as_slice().to_owned();
    api.add_image(image_key,
                  descriptor.into(),
                  ImageData::new(copied_bytes),
                  None);
}

#[no_mangle]
pub extern "C" fn wr_api_add_blob_image(api: &mut RenderApi,
                                        image_key: WrImageKey,
                                        descriptor: &WrImageDescriptor,
                                        bytes: ByteSlice) {
    assert!(unsafe { is_in_compositor_thread() });
    let copied_bytes = bytes.as_slice().to_owned();
    api.add_image(image_key,
                  descriptor.into(),
                  ImageData::new_blob_image(copied_bytes),
                  None);
}

#[no_mangle]
pub extern "C" fn wr_api_add_external_image(api: &mut RenderApi,
                                            image_key: WrImageKey,
                                            descriptor: &WrImageDescriptor,
                                            external_image_id: WrExternalImageId,
                                            buffer_type: WrExternalImageBufferType,
                                            channel_index: u8) {
    assert!(unsafe { is_in_compositor_thread() });
    api.add_image(image_key,
                  descriptor.into(),
                  ImageData::External(ExternalImageData {
                                          id: external_image_id.into(),
                                          channel_index: channel_index,
                                          image_type: buffer_type,
                                      }),
                  None);
}

#[no_mangle]
pub extern "C" fn wr_api_add_external_image_buffer(api: &mut RenderApi,
                                                   image_key: WrImageKey,
                                                   descriptor: &WrImageDescriptor,
                                                   external_image_id: WrExternalImageId) {
    assert!(unsafe { is_in_compositor_thread() });
    api.add_image(image_key,
                  descriptor.into(),
                  ImageData::External(ExternalImageData {
                                          id: external_image_id.into(),
                                          channel_index: 0,
                                          image_type: ExternalImageType::ExternalBuffer,
                                      }),
                  None);
}

#[no_mangle]
pub extern "C" fn wr_api_update_image(api: &mut RenderApi,
                                      key: WrImageKey,
                                      descriptor: &WrImageDescriptor,
                                      bytes: ByteSlice) {
    assert!(unsafe { is_in_compositor_thread() });
    let copied_bytes = bytes.as_slice().to_owned();

    api.update_image(key, descriptor.into(), ImageData::new(copied_bytes), None);
}

#[no_mangle]
pub extern "C" fn wr_api_delete_image(api: &mut RenderApi,
                                      key: WrImageKey) {
    assert!(unsafe { is_in_compositor_thread() });
    api.delete_image(key)
}

#[no_mangle]
pub extern "C" fn wr_api_set_root_pipeline(api: &mut RenderApi,
                                           pipeline_id: WrPipelineId) {
    api.set_root_pipeline(pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_api_set_window_parameters(api: &mut RenderApi,
                                               width: i32,
                                               height: i32) {
    let size = DeviceUintSize::new(width as u32, height as u32);
    api.set_window_parameters(size, DeviceUintRect::new(DeviceUintPoint::new(0, 0), size));
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_set_root_display_list(api: &mut RenderApi,
                                                      color: ColorF,
                                                      epoch: WrEpoch,
                                                      viewport_width: f32,
                                                      viewport_height: f32,
                                                      pipeline_id: WrPipelineId,
                                                      content_size: LayoutSize,
                                                      dl_descriptor: BuiltDisplayListDescriptor,
                                                      dl_data: *mut u8,
                                                      dl_size: usize) {
    let color = if color.a == 0.0 {
        None
    } else {
        Some(color.into())
    };
    // See the documentation of set_display_list in api.rs. I don't think
    // it makes a difference in gecko at the moment(until APZ is figured out)
    // but I suppose it is a good default.
    let preserve_frame_state = true;

    let dl_slice = make_slice(dl_data, dl_size);
    let mut dl_vec = Vec::new();
    // XXX: see if we can get rid of the copy here
    dl_vec.extend_from_slice(dl_slice);
    let dl = BuiltDisplayList::from_data(dl_vec, dl_descriptor);

    api.set_display_list(color,
                         epoch,
                         LayoutSize::new(viewport_width, viewport_height),
                         (pipeline_id, content_size.into(), dl),
                         preserve_frame_state);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_clear_root_display_list(api: &mut RenderApi,
                                                        epoch: WrEpoch,
                                                        pipeline_id: WrPipelineId) {
    let preserve_frame_state = true;
    let frame_builder = WebRenderFrameBuilder::new(pipeline_id, LayoutSize::zero());

    api.set_display_list(None,
                         epoch,
                         LayoutSize::new(0.0, 0.0),
                         frame_builder.dl_builder.finalize(),
                         preserve_frame_state);
}

#[no_mangle]
pub extern "C" fn wr_api_generate_frame(api: &mut RenderApi) {
    api.generate_frame(None);
}

#[no_mangle]
pub extern "C" fn wr_api_generate_frame_with_properties(api: &mut RenderApi,
                                                        opacity_array: *const WrOpacityProperty,
                                                        opacity_count: usize,
                                                        transform_array: *const WrTransformProperty,
                                                        transform_count: usize) {
    let mut properties = DynamicProperties {
        transforms: Vec::new(),
        floats: Vec::new(),
    };

    if transform_count > 0 {
        let transform_slice = make_slice(transform_array, transform_count);

        for element in transform_slice.iter() {
            let prop = PropertyValue {
                key: PropertyBindingKey::new(element.id),
                value: element.transform.into(),
            };

            properties.transforms.push(prop);
        }
    }

    if opacity_count > 0 {
        let opacity_slice = make_slice(opacity_array, opacity_count);

        for element in opacity_slice.iter() {
            let prop = PropertyValue {
                key: PropertyBindingKey::new(element.id),
                value: element.opacity,
            };
            properties.floats.push(prop);
        }
    }

    api.generate_frame(Some(properties));
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub extern "C" fn wr_api_send_external_event(api: &mut RenderApi,
                                             evt: usize) {
    assert!(unsafe { !is_in_render_thread() });

    api.send_external_event(ExternalEvent::from_raw(evt));
}

#[no_mangle]
pub extern "C" fn wr_api_add_raw_font(api: &mut RenderApi,
                                      key: WrFontKey,
                                      font_buffer: *mut u8,
                                      buffer_size: usize,
                                      index: u32) {
    assert!(unsafe { is_in_compositor_thread() });

    let font_slice = make_slice(font_buffer, buffer_size);
    let mut font_vector = Vec::new();
    font_vector.extend_from_slice(font_slice);

    api.add_raw_font(key, font_vector, index);
}

#[no_mangle]
pub extern "C" fn wr_api_delete_font(api: &mut RenderApi,
                                     key: WrFontKey) {
    assert!(unsafe { is_in_compositor_thread() });
    api.delete_font(key);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_get_namespace(api: &mut RenderApi) -> WrIdNamespace {
    api.id_namespace
}

// RenderThread WIP notes:
// In order to separate the compositor thread (or ipc receiver) and the render
// thread, some of the logic below needs to be rewritten. In particular
// the WrWindowState and Notifier implementations aren't designed to work with
// a separate render thread.
// As part of that I am moving the bindings closer to WebRender's API boundary,
// and moving more of the logic in C++ land.
// This work is tracked by bug 1328602.
//
// See RenderThread.h for some notes about how the pieces fit together.

pub struct WebRenderFrameBuilder {
    pub root_pipeline_id: WrPipelineId,
    pub dl_builder: webrender_api::DisplayListBuilder,
    pub scroll_clips_defined: HashSet<ClipId>,
}

impl WebRenderFrameBuilder {
    pub fn new(root_pipeline_id: WrPipelineId,
               content_size: LayoutSize) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            root_pipeline_id: root_pipeline_id,
            dl_builder: webrender_api::DisplayListBuilder::new(root_pipeline_id, content_size.into()),
            scroll_clips_defined: HashSet::new(),
        }
    }
}

pub struct WrState {
    pipeline_id: WrPipelineId,
    frame_builder: WebRenderFrameBuilder,
}

#[no_mangle]
pub extern "C" fn wr_state_new(pipeline_id: WrPipelineId,
                               content_size: LayoutSize) -> *mut WrState {
    assert!(unsafe { !is_in_render_thread() });

    let state = Box::new(WrState {
                             pipeline_id: pipeline_id,
                             frame_builder: WebRenderFrameBuilder::new(pipeline_id,
                                                                       content_size),
                         });

    Box::into_raw(state)
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub extern "C" fn wr_state_delete(state: *mut WrState) {
    assert!(unsafe { !is_in_render_thread() });

    unsafe {
        Box::from_raw(state);
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_begin(state: &mut WrState,
                              width: u32,
                              height: u32) {
    assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.data.clear();

    let bounds = LayoutRect::new(LayoutPoint::new(0.0, 0.0),
                                 LayoutSize::new(width as f32, height as f32));

    state.frame_builder
         .dl_builder
         .push_stacking_context(webrender_api::ScrollPolicy::Scrollable,
                                bounds,
                                None,
                                TransformStyle::Flat,
                                None,
                                MixBlendMode::Normal,
                                Vec::new());
}

#[no_mangle]
pub extern "C" fn wr_dp_end(state: &mut WrState) {
    assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_stacking_context();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_stacking_context(state: &mut WrState,
                                              bounds: LayoutRect,
                                              animation_id: u64,
                                              opacity: *const f32,
                                              transform: *const LayoutTransform,
                                              transform_style: TransformStyle,
                                              mix_blend_mode: MixBlendMode,
                                              filters: *const WrFilterOp,
                                              filter_count: usize) {
    assert!(unsafe { !is_in_render_thread() });

    let bounds = bounds.into();

    let c_filters = make_slice(filters, filter_count);
    let mut filters : Vec<FilterOp> = c_filters.iter().map(|c_filter| {
        match c_filter.filter_type {
            WrFilterOpType::Blur => FilterOp::Blur(c_filter.argument),
            WrFilterOpType::Brightness => FilterOp::Brightness(c_filter.argument),
            WrFilterOpType::Contrast => FilterOp::Contrast(c_filter.argument),
            WrFilterOpType::Grayscale => FilterOp::Grayscale(c_filter.argument),
            WrFilterOpType::HueRotate => FilterOp::HueRotate(c_filter.argument),
            WrFilterOpType::Invert => FilterOp::Invert(c_filter.argument),
            WrFilterOpType::Opacity => FilterOp::Opacity(PropertyBinding::Value(c_filter.argument)),
            WrFilterOpType::Saturate => FilterOp::Saturate(c_filter.argument),
            WrFilterOpType::Sepia => FilterOp::Sepia(c_filter.argument),
        }
    }).collect();

    let opacity = unsafe { opacity.as_ref() };
    if let Some(opacity) = opacity {
        if *opacity < 1.0 {
            filters.push(FilterOp::Opacity(PropertyBinding::Value(*opacity)));
        }
    } else {
        filters.push(FilterOp::Opacity(PropertyBinding::Binding(PropertyBindingKey::new(animation_id))));
    }

    let transform = unsafe { transform.as_ref() };
    let transform_binding = match animation_id {
        0 => match transform {
            Some(transform) => Some(PropertyBinding::Value(transform.clone())),
            None => None,
        },
        _ => Some(PropertyBinding::Binding(PropertyBindingKey::new(animation_id))),
    };

    state.frame_builder
         .dl_builder
         .push_stacking_context(webrender_api::ScrollPolicy::Scrollable,
                                bounds,
                                transform_binding,
                                transform_style,
                                None,
                                mix_blend_mode,
                                filters);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_stacking_context(state: &mut WrState) {
    assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_stacking_context();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clip(state: &mut WrState,
                                  rect: LayoutRect,
                                  complex: *const WrComplexClipRegion,
                                  complex_count: usize,
                                  mask: *const WrImageMask)
                                  -> u64 {
    assert!(unsafe { is_in_main_thread() });
    let clip_rect: LayoutRect = rect.into();
    let complex_slice = make_slice(complex, complex_count);
    let complex_iter = complex_slice.iter().map(|x| x.into());
    let mask : Option<ImageMask> = unsafe { mask.as_ref() }.map(|x| x.into());

    let clip_id = state.frame_builder.dl_builder.define_clip(None, clip_rect, complex_iter, mask);
    state.frame_builder.dl_builder.push_clip_id(clip_id);
    // return the u64 id value from inside the ClipId::Clip(..)
    match clip_id {
        ClipId::Clip(id, nesting_index, pipeline_id) => {
            assert!(pipeline_id == state.pipeline_id);
            assert!(nesting_index == 0);
            id
        },
        _ => panic!("Got unexpected clip id type"),
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_clip(state: &mut WrState) {
    assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_scroll_layer(state: &mut WrState,
                                          scroll_id: u64,
                                          content_rect: LayoutRect,
                                          clip_rect: LayoutRect) {
    assert!(unsafe { is_in_main_thread() });
    let clip_id = ClipId::new(scroll_id, state.pipeline_id);
    // Avoid defining multiple scroll clips with the same clip id, as that
    // results in undefined behaviour or assertion failures.
    if !state.frame_builder.scroll_clips_defined.contains(&clip_id) {
        let content_rect: LayoutRect = content_rect.into();
        let clip_rect: LayoutRect = clip_rect.into();

        state.frame_builder.dl_builder.define_scroll_frame(Some(clip_id), content_rect, clip_rect, vec![], None);
        state.frame_builder.scroll_clips_defined.insert(clip_id);
    }
    state.frame_builder.dl_builder.push_clip_id(clip_id);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_scroll_layer(state: &mut WrState) {
    assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_scroll_layer_with_id(api: &mut RenderApi,
                                          pipeline_id: WrPipelineId,
                                          scroll_id: u64,
                                          new_scroll_origin: LayoutPoint) {
    assert!(unsafe { is_in_compositor_thread() });
    let clip_id = ClipId::new(scroll_id, pipeline_id);
    api.scroll_node_with_id(new_scroll_origin.into(), clip_id, ScrollClamping::NoClamping);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clip_and_scroll_info(state: &mut WrState,
                                                  scroll_id: u64,
                                                  clip_id: *const u64) {
    assert!(unsafe { is_in_main_thread() });
    let scroll_id = ClipId::new(scroll_id, state.pipeline_id);
    let info = if let Some(&id) = unsafe { clip_id.as_ref() } {
        ClipAndScrollInfo::new(
            scroll_id,
            ClipId::Clip(id, 0, state.pipeline_id))
    } else {
        ClipAndScrollInfo::simple(scroll_id)
    };
    state.frame_builder.dl_builder.push_clip_and_scroll_info(info);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_clip_and_scroll_info(state: &mut WrState) {
    assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_iframe(state: &mut WrState,
                                    rect: LayoutRect,
                                    pipeline_id: WrPipelineId) {
    assert!(unsafe { is_in_main_thread() });

    state.frame_builder.dl_builder.push_iframe(rect.into(), None, pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_rect(state: &mut WrState,
                                  rect: LayoutRect,
                                  clip: LayoutRect,
                                  color: ColorF) {
    assert!(unsafe { !is_in_render_thread() });

    state.frame_builder.dl_builder.push_rect(rect.into(),
                                             Some(LocalClip::Rect(clip.into())),
                                             color.into());
}

#[no_mangle]
pub extern "C" fn wr_dp_push_image(state: &mut WrState,
                                   bounds: LayoutRect,
                                   clip: LayoutRect,
                                   stretch_size: LayoutSize,
                                   tile_spacing: LayoutSize,
                                   image_rendering: ImageRendering,
                                   key: WrImageKey) {
    assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    state.frame_builder
         .dl_builder
         .push_image(bounds.into(),
                     Some(LocalClip::Rect(clip.into())),
                     stretch_size.into(),
                     tile_spacing.into(),
                     image_rendering,
                     key);
}

/// Push a 3 planar yuv image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_planar_image(state: &mut WrState,
                                              bounds: LayoutRect,
                                              clip: LayoutRect,
                                              image_key_0: WrImageKey,
                                              image_key_1: WrImageKey,
                                              image_key_2: WrImageKey,
                                              color_space: WrYuvColorSpace,
                                              image_rendering: ImageRendering) {
    assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    state.frame_builder
         .dl_builder
         .push_yuv_image(bounds.into(),
                         Some(LocalClip::Rect(clip.into())),
                         YuvData::PlanarYCbCr(image_key_0, image_key_1, image_key_2),
                         color_space,
                         image_rendering);
}

/// Push a 2 planar NV12 image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_NV12_image(state: &mut WrState,
                                            bounds: LayoutRect,
                                            clip: LayoutRect,
                                            image_key_0: WrImageKey,
                                            image_key_1: WrImageKey,
                                            color_space: WrYuvColorSpace,
                                            image_rendering: ImageRendering) {
    assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    state.frame_builder
         .dl_builder
         .push_yuv_image(bounds.into(),
                         Some(LocalClip::Rect(clip.into())),
                         YuvData::NV12(image_key_0, image_key_1),
                         color_space,
                         image_rendering);
}

/// Push a yuv interleaved image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_interleaved_image(state: &mut WrState,
                                                   bounds: LayoutRect,
                                                   clip: LayoutRect,
                                                   image_key_0: WrImageKey,
                                                   color_space: WrYuvColorSpace,
                                                   image_rendering: ImageRendering) {
    assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    state.frame_builder
         .dl_builder
         .push_yuv_image(bounds.into(),
                         Some(LocalClip::Rect(clip.into())),
                         YuvData::InterleavedYCbCr(image_key_0),
                         color_space,
                         image_rendering);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_text(state: &mut WrState,
                                  bounds: LayoutRect,
                                  clip: LayoutRect,
                                  color: ColorF,
                                  font_key: WrFontKey,
                                  glyphs: *const GlyphInstance,
                                  glyph_count: u32,
                                  glyph_size: f32) {
    assert!(unsafe { is_in_main_thread() });

    let glyph_slice = make_slice(glyphs, glyph_count as usize);

    let colorf = ColorF::new(color.r, color.g, color.b, color.a);

    let glyph_options = None; // TODO
    state.frame_builder
         .dl_builder
         .push_text(bounds.into(),
                    Some(LocalClip::Rect(clip.into())),
                    &glyph_slice,
                    font_key,
                    colorf,
                    Au::from_f32_px(glyph_size),
                    glyph_options);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border(state: &mut WrState,
                                    rect: LayoutRect,
                                    clip: LayoutRect,
                                    widths: BorderWidths,
                                    top: BorderSide,
                                    right: BorderSide,
                                    bottom: BorderSide,
                                    left: BorderSide,
                                    radius: BorderRadius) {
    assert!(unsafe { is_in_main_thread() });

    let border_details = BorderDetails::Normal(NormalBorder {
                                                   left: left.into(),
                                                   right: right.into(),
                                                   top: top.into(),
                                                   bottom: bottom.into(),
                                                   radius: radius.into(),
                                               });
    state.frame_builder
         .dl_builder
         .push_border(rect.into(),
                      Some(LocalClip::Rect(clip.into())),
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_image(state: &mut WrState,
                                          rect: LayoutRect,
                                          clip: LayoutRect,
                                          widths: BorderWidths,
                                          image: WrImageKey,
                                          patch: NinePatchDescriptor,
                                          outset: SideOffsets2D<f32>,
                                          repeat_horizontal: RepeatMode,
                                          repeat_vertical: RepeatMode) {
    assert!(unsafe { is_in_main_thread() });
    let border_details =
        BorderDetails::Image(ImageBorder {
                                 image_key: image,
                                 patch: patch.into(),
                                 fill: false,
                                 outset: outset.into(),
                                 repeat_horizontal: repeat_horizontal.into(),
                                 repeat_vertical: repeat_vertical.into(),
                             });
    state.frame_builder
         .dl_builder
         .push_border(rect.into(),
                      Some(LocalClip::Rect(clip.into())),
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             widths: BorderWidths,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             outset: SideOffsets2D<f32>) {
    assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let border_details = BorderDetails::Gradient(GradientBorder {
                                                     gradient:
                                                         state.frame_builder
                                                              .dl_builder
                                                              .create_gradient(start_point.into(),
                                                                               end_point.into(),
                                                                               stops_vector,
                                                                               extend_mode.into()),
                                                     outset: outset.into(),
                                                 });
    state.frame_builder
         .dl_builder
         .push_border(rect.into(),
                      Some(LocalClip::Rect(clip.into())),
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_radial_gradient(state: &mut WrState,
                                                    rect: LayoutRect,
                                                    clip: LayoutRect,
                                                    widths: BorderWidths,
                                                    center: LayoutPoint,
                                                    radius: LayoutSize,
                                                    stops: *const GradientStop,
                                                    stops_count: usize,
                                                    extend_mode: ExtendMode,
                                                    outset: SideOffsets2D<f32>) {
    assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let border_details =
        BorderDetails::RadialGradient(RadialGradientBorder {
                                          gradient:
                                              state.frame_builder
                                                   .dl_builder
                                                   .create_radial_gradient(center.into(),
                                                                           radius.into(),
                                                                           stops_vector,
                                                                           extend_mode.into()),
                                          outset: outset.into(),
                                      });
    state.frame_builder
         .dl_builder
         .push_border(rect.into(),
                      Some(LocalClip::Rect(clip.into())),
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_linear_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_gradient(start_point.into(),
                                         end_point.into(),
                                         stops_vector,
                                         extend_mode.into());
    state.frame_builder
         .dl_builder
         .push_gradient(rect.into(),
                        Some(LocalClip::Rect(clip.into())),
                        gradient,
                        tile_size.into(),
                        tile_spacing.into());
}

#[no_mangle]
pub extern "C" fn wr_dp_push_radial_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             center: LayoutPoint,
                                             radius: LayoutSize,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_radial_gradient(center.into(),
                                                radius.into(),
                                                stops_vector,
                                                extend_mode.into());
    state.frame_builder
         .dl_builder
         .push_radial_gradient(rect.into(),
                               Some(LocalClip::Rect(clip.into())),
                               gradient,
                               tile_size.into(),
                               tile_spacing.into());
}

#[no_mangle]
pub extern "C" fn wr_dp_push_box_shadow(state: &mut WrState,
                                        rect: LayoutRect,
                                        clip: LayoutRect,
                                        box_bounds: LayoutRect,
                                        offset: LayoutVector2D,
                                        color: ColorF,
                                        blur_radius: f32,
                                        spread_radius: f32,
                                        border_radius: f32,
                                        clip_mode: BoxShadowClipMode) {
    assert!(unsafe { is_in_main_thread() });

    state.frame_builder
         .dl_builder
         .push_box_shadow(rect.into(),
                          Some(LocalClip::Rect(clip.into())),
                          box_bounds.into(),
                          offset,
                          color.into(),
                          blur_radius,
                          spread_radius,
                          border_radius,
                          clip_mode);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_finalize_builder(state: &mut WrState,
                                                 content_size: &mut LayoutSize,
                                                 dl_descriptor: &mut BuiltDisplayListDescriptor,
                                                 dl_data: &mut WrVecU8) {
    let frame_builder = mem::replace(&mut state.frame_builder,
                                     WebRenderFrameBuilder::new(state.pipeline_id,
                                                                LayoutSize::zero()));
    let (_, size, dl) = frame_builder.dl_builder.finalize();
    *content_size = LayoutSize::new(size.width, size.height);
    let (data, descriptor) = dl.into_data();
    *dl_data = WrVecU8::from_vec(data);
    *dl_descriptor = descriptor;
}

#[no_mangle]
pub extern "C" fn wr_dp_push_built_display_list(state: &mut WrState,
                                                dl_descriptor: BuiltDisplayListDescriptor,
                                                dl_data: &mut WrVecU8) {
    let dl_vec = mem::replace(dl_data, WrVecU8::from_vec(Vec::new())).to_vec();

    let dl = BuiltDisplayList::from_data(dl_vec, dl_descriptor);

    state.frame_builder.dl_builder.push_nested_display_list(&dl);
    let (data, _) = dl.into_data();
    mem::replace(dl_data, WrVecU8::from_vec(data));
}

pub type VecU8 = Vec<u8>;
pub type ArcVecU8 = Arc<VecU8>;

#[no_mangle]
pub extern "C" fn wr_add_ref_arc(arc: &ArcVecU8) -> *const VecU8 {
    Arc::into_raw(arc.clone())
}

#[no_mangle]
pub unsafe extern "C" fn wr_dec_ref_arc(arc: *const VecU8) {
    Arc::from_raw(arc);
}

// TODO: nical
// Update for the new blob image interface changes.
//
extern "C" {
     // TODO: figure out the API for tiled blob images.
     pub fn wr_moz2d_render_cb(blob: ByteSlice,
                               width: u32,
                               height: u32,
                               format: ImageFormat,
                               output: MutByteSlice)
                               -> bool;
}

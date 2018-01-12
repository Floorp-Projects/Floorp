use std::ffi::{CStr, CString};
use std::{mem, slice};
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;
use std::sync::Arc;
use std::os::raw::{c_void, c_char, c_float};
use gleam::gl;

use webrender::api::*;
use webrender::{ReadPixelsFormat, Renderer, RendererOptions, ThreadListener};
use webrender::{ExternalImage, ExternalImageHandler, ExternalImageSource};
use webrender::DebugFlags;
use webrender::{ApiRecordingReceiver, BinaryRecorder};
use webrender::{ProgramCache, UploadMethod, VertexUsageHint};
use thread_profiler::register_thread_with_profiler;
use moz2d_renderer::Moz2dImageRenderer;
use app_units::Au;
use rayon;
use euclid::SideOffsets2D;
use log::{set_logger, shutdown_logger, LogLevelFilter, Log, LogLevel, LogMetadata, LogRecord};

#[cfg(target_os = "windows")]
use dwrote::{FontDescriptor, FontWeight, FontStretch, FontStyle};

#[cfg(target_os = "macos")]
use core_foundation::string::CFString;
#[cfg(target_os = "macos")]
use core_graphics::font::CGFont;

/// cbindgen:field-names=[mNamespace, mHandle]
type WrExternalImageBufferType = ExternalImageType;

/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
type WrEpoch = Epoch;
/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
/// cbindgen:derive-neq=true
type WrIdNamespace = IdNamespace;

/// cbindgen:field-names=[mNamespace, mHandle]
type WrPipelineId = PipelineId;
/// cbindgen:field-names=[mNamespace, mHandle]
/// cbindgen:derive-neq=true
type WrImageKey = ImageKey;
/// cbindgen:field-names=[mNamespace, mHandle]
pub type WrFontKey = FontKey;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrFontInstanceKey = FontInstanceKey;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrYuvColorSpace = YuvColorSpace;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrLogLevelFilter = LogLevelFilter;

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

pub struct DocumentHandle {
    api: RenderApi,
    document_id: DocumentId,
}

impl DocumentHandle {
    pub fn new(api: RenderApi, size: DeviceUintSize) -> DocumentHandle {
        let layer = 0; //TODO
        let doc = api.add_document(size, layer);
        DocumentHandle {
            api: api,
            document_id: doc
        }
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

    // Equivalent to `to_vec` but clears self instead of consuming the value.
    fn flush_into_vec(&mut self) -> Vec<u8> {
        self.convert_into_vec::<u8>()
    }

    // Like flush_into_vec, but also does an unsafe conversion to the desired type.
    fn convert_into_vec<T>(&mut self) -> Vec<T> {
        let vec = unsafe {
            Vec::from_raw_parts(
                self.data as *mut T,
                self.length / mem::size_of::<T>(),
                self.capacity / mem::size_of::<T>(),
            )
        };
        self.data = ptr::null_mut();
        self.length = 0;
        self.capacity = 0;
        vec
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

    fn push_bytes(&mut self, bytes: &[u8]) {
        let mut vec = self.flush_into_vec();
        vec.extend_from_slice(bytes);
        *self = Self::from_vec(vec);
    }
}


#[no_mangle]
pub extern "C" fn wr_vec_u8_push_bytes(v: &mut WrVecU8, bytes: ByteSlice) {
    v.push_bytes(bytes.as_slice());
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
    RawData,
    NativeTexture,
    Invalid,
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
            WrExternalImageType::Invalid => {
                ExternalImage {
                    u0: image.u0,
                    v0: image.v0,
                    u1: image.u1,
                    v1: image.v1,
                    source: ExternalImageSource::Invalid,
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

#[repr(u32)]
pub enum WrAnimationType {
    Transform = 0,
    Opacity = 1,
}

#[repr(C)]
pub struct WrAnimationProperty {
    effect_type: WrAnimationType,
    id: u64,
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
  DropShadow = 9,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct WrFilterOp {
    filter_type: WrFilterOpType,
    argument: c_float, // holds radius for DropShadow; value for other filters
    offset: LayoutVector2D, // only used for DropShadow
    color: ColorF, // only used for DropShadow
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
    fn is_glcontext_angle(glcontext_ptr: *mut c_void) -> bool;
    // Enables binary recording that can be used with `wrench replay`
    // Outputs a wr-record-*.bin file for each window that is shown
    // Note: wrench will panic if external images are used, they can
    // be disabled in WebRenderBridgeParent::ProcessWebRenderCommands
    // by commenting out the path that adds an external image ID
    fn gfx_use_wrench() -> bool;
    fn gfx_wr_resource_path_override() -> *const c_char;
    // TODO: make gfx_critical_error() work.
    // We still have problem to pass the error message from render/render_backend
    // thread to main thread now.
    #[allow(dead_code)]
    fn gfx_critical_error(msg: *const c_char);
    fn gfx_critical_note(msg: *const c_char);
    fn gecko_printf_stderr_output(msg: *const c_char);
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

impl RenderNotifier for CppNotifier {
    fn clone(&self) -> Box<RenderNotifier> {
        Box::new(CppNotifier {
            window_id: self.window_id,
        })
    }

    fn wake_up(&self) {
        unsafe {
            wr_notifier_new_frame_ready(self.window_id);
        }
    }

    fn new_document_ready(&self,
                          _: DocumentId,
                          scrolled: bool,
                          composite_needed: bool) {
        unsafe {
            if scrolled {
                wr_notifier_new_scroll_frame_ready(self.window_id, composite_needed);
            } else {
                wr_notifier_new_frame_ready(self.window_id);
            }
        }
    }

    fn external_event(&self,
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
                                     height: u32) -> bool {
    match renderer.render(DeviceUintSize::new(width, height)) {
        Ok(_) => true,
        Err(errors) => {
            for e in errors {
                println!(" Failed to render: {:?}", e);
                let msg = CString::new(format!("wr_renderer_render: {:?}", e)).unwrap();
                unsafe {
                    gfx_critical_note(msg.as_ptr());
                }
            }
            false
        },
    }
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

/// cbindgen:field-names=[mBits]
#[repr(C)]
pub struct WrDebugFlags {
    bits: u32,
}

#[no_mangle]
pub extern "C" fn wr_renderer_get_debug_flags(renderer: &mut Renderer) -> WrDebugFlags {
    WrDebugFlags { bits: renderer.get_debug_flags().bits() }
}

#[no_mangle]
pub extern "C" fn wr_renderer_set_debug_flags(renderer: &mut Renderer, flags: WrDebugFlags) {
    if let Some(dbg_flags) = DebugFlags::from_bits(flags.bits) {
        renderer.set_debug_flags(dbg_flags);
    }
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
    let renderer = Box::from_raw(renderer);
    renderer.deinit();
    // let renderer go out of scope and get dropped
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

extern "C" {
    fn gecko_profiler_register_thread(name: *const ::std::os::raw::c_char);
    fn gecko_profiler_unregister_thread();
}

struct GeckoProfilerThreadListener {}

impl GeckoProfilerThreadListener {
    pub fn new() -> GeckoProfilerThreadListener {
        GeckoProfilerThreadListener{}
    }
}

impl ThreadListener for GeckoProfilerThreadListener {
    fn thread_started(&self, thread_name: &str) {
        let name = CString::new(thread_name).unwrap();
        unsafe {
            // gecko_profiler_register_thread copies the passed name here.
            gecko_profiler_register_thread(name.as_ptr());
        }
    }

    fn thread_stopped(&self, _: &str) {
        unsafe {
            gecko_profiler_unregister_thread();
        }
    }
}

pub struct WrThreadPool(Arc<rayon::ThreadPool>);

#[no_mangle]
pub unsafe extern "C" fn wr_thread_pool_new() -> *mut WrThreadPool {
    let worker_config = rayon::Configuration::new()
        .thread_name(|idx|{ format!("WRWorker#{}", idx) })
        .start_handler(|idx| {
            let name = format!("WRWorker#{}", idx);
            register_thread_with_profiler(name.clone());
            gecko_profiler_register_thread(CString::new(name).unwrap().as_ptr());
        })
        .exit_handler(|_idx| {
            gecko_profiler_unregister_thread();
        });

    let workers = Arc::new(rayon::ThreadPool::new(worker_config).unwrap());

    Box::into_raw(Box::new(WrThreadPool(workers)))
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_thread_pool_delete(thread_pool: *mut WrThreadPool) {
    Box::from_raw(thread_pool);
}

pub struct WrProgramCache(Rc<ProgramCache>);

#[no_mangle]
pub unsafe extern "C" fn wr_program_cache_new() -> *mut WrProgramCache {
    let program_cache = ProgramCache::new();
    Box::into_raw(Box::new(WrProgramCache(program_cache)))
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_program_cache_delete(program_cache: *mut WrProgramCache) {
    Rc::from_raw(program_cache);
}

#[no_mangle]
pub extern "C" fn wr_renderer_update_program_cache(renderer: &mut Renderer, program_cache: &mut WrProgramCache) {
    let program_cache = Rc::clone(&program_cache.0);
    renderer.update_program_cache(program_cache);
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern "C" fn wr_window_new(window_id: WrWindowId,
                                window_width: u32,
                                window_height: u32,
                                gl_context: *mut c_void,
                                thread_pool: *mut WrThreadPool,
                                out_handle: &mut *mut DocumentHandle,
                                out_renderer: &mut *mut Renderer,
                                out_max_texture_size: *mut u32)
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
    gl.clear_color(0.0, 0.0, 0.0, 1.0);

    let version = gl.get_string(gl::VERSION);

    println!("WebRender - OpenGL version new {}", version);

    let workers = unsafe {
        Arc::clone(&(*thread_pool).0)
    };

    let upload_method = if unsafe { is_glcontext_angle(gl_context) } {
        UploadMethod::Immediate
    } else {
        UploadMethod::PixelBuffer(VertexUsageHint::Dynamic)
    };

    let opts = RendererOptions {
        enable_aa: true,
        enable_subpixel_aa: true,
        recorder: recorder,
        blob_image_renderer: Some(Box::new(Moz2dImageRenderer::new(workers.clone()))),
        workers: Some(workers.clone()),
        thread_listener: Some(Box::new(GeckoProfilerThreadListener::new())),
        enable_render_on_scroll: false,
        resource_override_path: unsafe {
            let override_charptr = gfx_wr_resource_path_override();
            if override_charptr.is_null() {
                None
            } else {
                match CStr::from_ptr(override_charptr).to_str() {
                    Ok(override_str) => Some(PathBuf::from(override_str)),
                    _ => None
                }
            }
        },
        renderer_id: Some(window_id.0),
        upload_method,
        ..Default::default()
    };

    let notifier = Box::new(CppNotifier {
        window_id: window_id,
    });
    let (renderer, sender) = match Renderer::new(gl, notifier, opts) {
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

    unsafe {
        *out_max_texture_size = renderer.get_max_texture_size();
    }
    let window_size = DeviceUintSize::new(window_width, window_height);
    *out_handle = Box::into_raw(Box::new(
            DocumentHandle::new(sender.create_api(), window_size)));
    *out_renderer = Box::into_raw(Box::new(renderer));

    return true;
}

#[no_mangle]
pub extern "C" fn wr_api_clone(dh: &mut DocumentHandle,
                                      out_handle: &mut *mut DocumentHandle) {
    assert!(unsafe { is_in_compositor_thread() });

    let handle = DocumentHandle {
        api: dh.api.clone_sender().create_api(),
        document_id: dh.document_id,
    };
    *out_handle = Box::into_raw(Box::new(handle));
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_api_delete(dh: *mut DocumentHandle) {
    let handle = Box::from_raw(dh);
    if handle.document_id.0 == handle.api.get_namespace_id() {
        handle.api.delete_document(handle.document_id);
        handle.api.shut_down();
    }
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_image(
    resources: &mut ResourceUpdates,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
) {
    resources.add_image(
        image_key,
        descriptor.into(),
        ImageData::new(bytes.flush_into_vec()),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_blob_image(
    resources: &mut ResourceUpdates,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
) {
    resources.add_image(
        image_key,
        descriptor.into(),
        ImageData::new_blob_image(bytes.flush_into_vec()),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_external_image(
    resources: &mut ResourceUpdates,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    external_image_id: WrExternalImageId,
    buffer_type: WrExternalImageBufferType,
    channel_index: u8
) {
    resources.add_image(
        image_key,
        descriptor.into(),
        ImageData::External(
            ExternalImageData {
                id: external_image_id.into(),
                channel_index: channel_index,
                image_type: buffer_type,
            }
        ),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_image(
    resources: &mut ResourceUpdates,
    key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
) {
    resources.update_image(
        key,
        descriptor.into(),
        ImageData::new(bytes.flush_into_vec()),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_external_image(
    resources: &mut ResourceUpdates,
    key: WrImageKey,
    descriptor: &WrImageDescriptor,
    external_image_id: WrExternalImageId,
    image_type: WrExternalImageBufferType,
    channel_index: u8
) {
    resources.update_image(
        key,
        descriptor.into(),
        ImageData::External(
            ExternalImageData {
                id: external_image_id.into(),
                channel_index,
                image_type,
            }
        ),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_blob_image(
    resources: &mut ResourceUpdates,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
    dirty_rect: DeviceUintRect,
) {
    resources.update_image(
        image_key,
        descriptor.into(),
        ImageData::new_blob_image(bytes.flush_into_vec()),
        Some(dirty_rect)
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_image(
    resources: &mut ResourceUpdates,
    key: WrImageKey
) {
    resources.delete_image(key);
}

#[no_mangle]
pub extern "C" fn wr_api_update_resources(
    dh: &mut DocumentHandle,
    resources: &mut ResourceUpdates
) {
    let resource_updates = mem::replace(resources, ResourceUpdates::new());
    dh.api.update_resources(resource_updates);
}

#[no_mangle]
pub extern "C" fn wr_api_update_pipeline_resources(
    dh: &mut DocumentHandle,
    pipeline_id: WrPipelineId,
    epoch: WrEpoch,
    resources: &mut ResourceUpdates
) {
    let resource_updates = mem::replace(resources, ResourceUpdates::new());
    dh.api.update_pipeline_resources(resource_updates, dh.document_id, pipeline_id, epoch);
}


#[no_mangle]
pub extern "C" fn wr_api_set_root_pipeline(dh: &mut DocumentHandle,
                                           pipeline_id: WrPipelineId) {
    dh.api.set_root_pipeline(dh.document_id, pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_api_remove_pipeline(dh: &mut DocumentHandle,
                                         pipeline_id: WrPipelineId) {
    dh.api.remove_pipeline(dh.document_id, pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_api_set_window_parameters(dh: &mut DocumentHandle,
                                               width: i32,
                                               height: i32) {
    let size = DeviceUintSize::new(width as u32, height as u32);
    dh.api.set_window_parameters(dh.document_id,
                                 size,
                                 DeviceUintRect::new(DeviceUintPoint::new(0, 0), size),
                                 1.0);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_set_display_list(
    dh: &mut DocumentHandle,
    color: ColorF,
    epoch: WrEpoch,
    viewport_width: f32,
    viewport_height: f32,
    pipeline_id: WrPipelineId,
    content_size: LayoutSize,
    dl_descriptor: BuiltDisplayListDescriptor,
    dl_data: &mut WrVecU8,
    resources: &mut ResourceUpdates,
) {
    let resource_updates = mem::replace(resources, ResourceUpdates::new());

    let color = if color.a == 0.0 { None } else { Some(color) };

    // See the documentation of set_display_list in api.rs. I don't think
    // it makes a difference in gecko at the moment(until APZ is figured out)
    // but I suppose it is a good default.
    let preserve_frame_state = true;

    let dl_vec = dl_data.flush_into_vec();
    let dl = BuiltDisplayList::from_data(dl_vec, dl_descriptor);

    dh.api.set_display_list(
        dh.document_id,
        epoch,
        color,
        LayoutSize::new(viewport_width, viewport_height),
        (pipeline_id, content_size, dl),
        preserve_frame_state,
        resource_updates
    );
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_clear_display_list(
    dh: &mut DocumentHandle,
    epoch: WrEpoch,
    pipeline_id: WrPipelineId,
) {
    let preserve_frame_state = true;
    let frame_builder = WebRenderFrameBuilder::new(pipeline_id, LayoutSize::zero());
    let resource_updates = ResourceUpdates::new();

    dh.api.set_display_list(
        dh.document_id,
        epoch,
        None,
        LayoutSize::new(0.0, 0.0),
        frame_builder.dl_builder.finalize(),
        preserve_frame_state,
        resource_updates
    );
}

#[no_mangle]
pub extern "C" fn wr_api_generate_frame(dh: &mut DocumentHandle) {
    dh.api.generate_frame(dh.document_id, None);
}

#[no_mangle]
pub extern "C" fn wr_api_generate_frame_with_properties(dh: &mut DocumentHandle,
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

    dh.api.generate_frame(dh.document_id, Some(properties));
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub extern "C" fn wr_api_send_external_event(dh: &mut DocumentHandle,
                                             evt: usize) {
    assert!(unsafe { !is_in_render_thread() });

    dh.api.send_external_event(ExternalEvent::from_raw(evt));
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_raw_font(
    resources: &mut ResourceUpdates,
    key: WrFontKey,
    bytes: &mut WrVecU8,
    index: u32
) {
    resources.add_raw_font(key, bytes.flush_into_vec(), index);
}

#[cfg(target_os = "windows")]
fn read_font_descriptor(
    bytes: &mut WrVecU8,
    index: u32
) -> NativeFontHandle {
    let wchars = bytes.convert_into_vec::<u16>();
    FontDescriptor {
        family_name: String::from_utf16(&wchars).unwrap(),
        weight: FontWeight::from_u32(index & 0xffff),
        stretch: FontStretch::from_u32((index >> 16) & 0xff),
        style: FontStyle::from_u32((index >> 24) & 0xff),
    }
}

#[cfg(target_os = "macos")]
fn read_font_descriptor(
    bytes: &mut WrVecU8,
    _index: u32
) -> NativeFontHandle {
    let chars = bytes.flush_into_vec();
    let name = String::from_utf8(chars).unwrap();
    let font = CGFont::from_name(&CFString::new(&*name)).unwrap();
    NativeFontHandle(font)
}

#[cfg(not(any(target_os = "macos", target_os = "windows")))]
fn read_font_descriptor(
    bytes: &mut WrVecU8,
    index: u32
) -> NativeFontHandle {
    let cstr = CString::new(bytes.flush_into_vec()).unwrap();
    NativeFontHandle {
        pathname: String::from(cstr.to_str().unwrap()),
        index,
    }
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_font_descriptor(
    resources: &mut ResourceUpdates,
    key: WrFontKey,
    bytes: &mut WrVecU8,
    index: u32
) {
    let native_font_handle = read_font_descriptor(bytes, index);
    resources.add_native_font(key, native_font_handle);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_font(
    resources: &mut ResourceUpdates,
    key: WrFontKey
) {
    resources.delete_font(key);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_font_instance(
    resources: &mut ResourceUpdates,
    key: WrFontInstanceKey,
    font_key: WrFontKey,
    glyph_size: f32,
    options: *const FontInstanceOptions,
    platform_options: *const FontInstancePlatformOptions,
    variations: &mut WrVecU8,
) {
    resources.add_font_instance(
        key,
        font_key,
        Au::from_f32_px(glyph_size),
        unsafe { options.as_ref().cloned() },
        unsafe { platform_options.as_ref().cloned() },
        variations.convert_into_vec::<FontVariation>(),
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_font_instance(
    resources: &mut ResourceUpdates,
    key: WrFontInstanceKey
) {
    resources.delete_font_instance(key);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_new() -> *mut ResourceUpdates {
    let updates = Box::new(ResourceUpdates::new());
    Box::into_raw(updates)
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_clear(resources: &mut ResourceUpdates) {
    resources.updates.clear();
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub extern "C" fn wr_resource_updates_delete(updates: *mut ResourceUpdates) {
    unsafe {
        Box::from_raw(updates);
    }
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_get_namespace(dh: &mut DocumentHandle) -> WrIdNamespace {
    dh.api.get_namespace_id()
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
    pub dl_builder: DisplayListBuilder,
}

impl WebRenderFrameBuilder {
    pub fn new(root_pipeline_id: WrPipelineId,
               content_size: LayoutSize) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            root_pipeline_id: root_pipeline_id,
            dl_builder: DisplayListBuilder::new(root_pipeline_id, content_size),
        }
    }
    pub fn with_capacity(root_pipeline_id: WrPipelineId,
               content_size: LayoutSize,
               capacity: usize) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            root_pipeline_id: root_pipeline_id,
            dl_builder: DisplayListBuilder::with_capacity(root_pipeline_id, content_size, capacity),
        }
    }

}

pub struct WrState {
    pipeline_id: WrPipelineId,
    frame_builder: WebRenderFrameBuilder,
    current_tag: Option<ItemTag>,
}

#[no_mangle]
pub extern "C" fn wr_state_new(pipeline_id: WrPipelineId,
                               content_size: LayoutSize,
                               capacity: usize) -> *mut WrState {
    assert!(unsafe { !is_in_render_thread() });

    let state = Box::new(WrState {
                             pipeline_id: pipeline_id,
                             frame_builder: WebRenderFrameBuilder::with_capacity(pipeline_id,
                                                                                 content_size,
                                                                                 capacity),
                             current_tag: None,
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
pub extern "C" fn wr_dp_save(state: &mut WrState) {
    state.frame_builder.dl_builder.save();
}

#[no_mangle]
pub extern "C" fn wr_dp_restore(state: &mut WrState) {
    state.frame_builder.dl_builder.restore();
}

#[no_mangle]
pub extern "C" fn wr_dp_clear_save(state: &mut WrState) {
    state.frame_builder.dl_builder.clear_save();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_stacking_context(state: &mut WrState,
                                              bounds: LayoutRect,
                                              animation: *const WrAnimationProperty,
                                              opacity: *const f32,
                                              transform: *const LayoutTransform,
                                              transform_style: TransformStyle,
                                              perspective: *const LayoutTransform,
                                              mix_blend_mode: MixBlendMode,
                                              filters: *const WrFilterOp,
                                              filter_count: usize,
                                              is_backface_visible: bool) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let c_filters = make_slice(filters, filter_count);
    let mut filters : Vec<FilterOp> = c_filters.iter().map(|c_filter| {
        match c_filter.filter_type {
            WrFilterOpType::Blur => FilterOp::Blur(c_filter.argument),
            WrFilterOpType::Brightness => FilterOp::Brightness(c_filter.argument),
            WrFilterOpType::Contrast => FilterOp::Contrast(c_filter.argument),
            WrFilterOpType::Grayscale => FilterOp::Grayscale(c_filter.argument),
            WrFilterOpType::HueRotate => FilterOp::HueRotate(c_filter.argument),
            WrFilterOpType::Invert => FilterOp::Invert(c_filter.argument),
            WrFilterOpType::Opacity => FilterOp::Opacity(PropertyBinding::Value(c_filter.argument), c_filter.argument),
            WrFilterOpType::Saturate => FilterOp::Saturate(c_filter.argument),
            WrFilterOpType::Sepia => FilterOp::Sepia(c_filter.argument),
            WrFilterOpType::DropShadow => FilterOp::DropShadow(c_filter.offset,
                                                               c_filter.argument,
                                                               c_filter.color),
        }
    }).collect();

    let opacity_ref = unsafe { opacity.as_ref() };
    if let Some(opacity) = opacity_ref {
        if *opacity < 1.0 {
            filters.push(FilterOp::Opacity(PropertyBinding::Value(*opacity), *opacity));
        }
    }

    let transform_ref = unsafe { transform.as_ref() };
    let mut transform_binding = match transform_ref {
        Some(transform) => Some(PropertyBinding::Value(transform.clone())),
        None => None,
    };

    let anim = unsafe { animation.as_ref() };
    if let Some(anim) = anim {
        debug_assert!(anim.id > 0);
        match anim.effect_type {
            WrAnimationType::Opacity => filters.push(FilterOp::Opacity(PropertyBinding::Binding(PropertyBindingKey::new(anim.id)), 1.0)),
            WrAnimationType::Transform => transform_binding = Some(PropertyBinding::Binding(PropertyBindingKey::new(anim.id))),
        }
    }

    let perspective_ref = unsafe { perspective.as_ref() };
    let perspective = match perspective_ref {
        Some(perspective) => Some(perspective.clone()),
        None => None,
    };

    let mut prim_info = LayoutPrimitiveInfo::new(bounds);
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;

    state.frame_builder
         .dl_builder
         .push_stacking_context(&prim_info,
                                ScrollPolicy::Scrollable,
                                transform_binding,
                                transform_style,
                                perspective,
                                mix_blend_mode,
                                filters);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_stacking_context(state: &mut WrState) {
    debug_assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_stacking_context();
}

fn make_scroll_info(state: &mut WrState,
                    scroll_id: Option<&u64>,
                    clip_id: Option<&u64>)
                    -> Option<ClipAndScrollInfo> {
    if let Some(&sid) = scroll_id {
        if let Some(&cid) = clip_id {
            Some(ClipAndScrollInfo::new(
                ClipId::new(sid, state.pipeline_id),
                ClipId::Clip(cid, state.pipeline_id)))
        } else {
            Some(ClipAndScrollInfo::simple(
                ClipId::new(sid, state.pipeline_id)))
        }
    } else if let Some(&cid) = clip_id {
        Some(ClipAndScrollInfo::simple(
            ClipId::Clip(cid, state.pipeline_id)))
    } else {
        None
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clip(state: &mut WrState,
                                    ancestor_scroll_id: *const u64,
                                    ancestor_clip_id: *const u64,
                                    clip_rect: LayoutRect,
                                    complex: *const ComplexClipRegion,
                                    complex_count: usize,
                                    mask: *const WrImageMask)
                                    -> u64 {
    debug_assert!(unsafe { is_in_main_thread() });

    let info = make_scroll_info(state,
                                unsafe { ancestor_scroll_id.as_ref() },
                                unsafe { ancestor_clip_id.as_ref() });

    let complex_slice = make_slice(complex, complex_count);
    let complex_iter = complex_slice.iter().cloned();
    let mask : Option<ImageMask> = unsafe { mask.as_ref() }.map(|x| x.into());

    let clip_id = if info.is_some() {
        state.frame_builder.dl_builder.define_clip_with_parent(None,
            info.unwrap().scroll_node_id, clip_rect, complex_iter, mask)
    } else {
        state.frame_builder.dl_builder.define_clip(None, clip_rect, complex_iter, mask)
    };
    // return the u64 id value from inside the ClipId::Clip(..)
    match clip_id {
        ClipId::Clip(id, pipeline_id) => {
            assert!(pipeline_id == state.pipeline_id);
            id
        },
        _ => panic!("Got unexpected clip id type"),
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clip(state: &mut WrState,
                                  clip_id: u64) {
    debug_assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.push_clip_id(ClipId::Clip(clip_id, state.pipeline_id));
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_clip(state: &mut WrState) {
    debug_assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_dp_define_sticky_frame(state: &mut WrState,
                                            content_rect: LayoutRect,
                                            top_margin: *const f32,
                                            right_margin: *const f32,
                                            bottom_margin: *const f32,
                                            left_margin: *const f32,
                                            vertical_bounds: StickyOffsetBounds,
                                            horizontal_bounds: StickyOffsetBounds,
                                            applied_offset: LayoutVector2D)
                                            -> u64 {
    assert!(unsafe { is_in_main_thread() });
    let clip_id = state.frame_builder.dl_builder.define_sticky_frame(
        None, content_rect, SideOffsets2D::new(
            unsafe { top_margin.as_ref() }.cloned(),
            unsafe { right_margin.as_ref() }.cloned(),
            unsafe { bottom_margin.as_ref() }.cloned(),
            unsafe { left_margin.as_ref() }.cloned()
        ),
        vertical_bounds, horizontal_bounds, applied_offset);
    match clip_id {
        ClipId::Clip(id, pipeline_id) => {
            assert!(pipeline_id == state.pipeline_id);
            id
        },
        _ => panic!("Got unexpected clip id type"),
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_define_scroll_layer(state: &mut WrState,
                                            scroll_id: u64,
                                            ancestor_scroll_id: *const u64,
                                            ancestor_clip_id: *const u64,
                                            content_rect: LayoutRect,
                                            clip_rect: LayoutRect) {
    assert!(unsafe { is_in_main_thread() });

    let info = make_scroll_info(state,
                                unsafe { ancestor_scroll_id.as_ref() },
                                unsafe { ancestor_clip_id.as_ref() });

    let clip_id = ClipId::new(scroll_id, state.pipeline_id);
    if info.is_some() {
        state.frame_builder.dl_builder.define_scroll_frame_with_parent(
            Some(clip_id), info.unwrap().scroll_node_id, content_rect,
            clip_rect, vec![], None, ScrollSensitivity::Script);
    } else {
        state.frame_builder.dl_builder.define_scroll_frame(
            Some(clip_id), content_rect, clip_rect, vec![], None,
            ScrollSensitivity::Script);
    };
}

#[no_mangle]
pub extern "C" fn wr_dp_push_scroll_layer(state: &mut WrState,
                                          scroll_id: u64) {
    debug_assert!(unsafe { is_in_main_thread() });
    let clip_id = ClipId::new(scroll_id, state.pipeline_id);
    state.frame_builder.dl_builder.push_clip_id(clip_id);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_scroll_layer(state: &mut WrState) {
    debug_assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_scroll_layer_with_id(dh: &mut DocumentHandle,
                                          pipeline_id: WrPipelineId,
                                          scroll_id: u64,
                                          new_scroll_origin: LayoutPoint) {
    assert!(unsafe { is_in_compositor_thread() });
    let clip_id = ClipId::new(scroll_id, pipeline_id);
    dh.api.scroll_node_with_id(dh.document_id, new_scroll_origin, clip_id, ScrollClamping::NoClamping);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clip_and_scroll_info(state: &mut WrState,
                                                  scroll_id: u64,
                                                  clip_id: *const u64) {
    debug_assert!(unsafe { is_in_main_thread() });
    let info = make_scroll_info(state, Some(&scroll_id), unsafe { clip_id.as_ref() });
    debug_assert!(info.is_some());
    state.frame_builder.dl_builder.push_clip_and_scroll_info(info.unwrap());
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_clip_and_scroll_info(state: &mut WrState) {
    debug_assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_iframe(state: &mut WrState,
                                    rect: LayoutRect,
                                    is_backface_visible: bool,
                                    pipeline_id: WrPipelineId) {
    debug_assert!(unsafe { is_in_main_thread() });

    let mut prim_info = LayoutPrimitiveInfo::new(rect);
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder.dl_builder.push_iframe(&prim_info, pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_rect(state: &mut WrState,
                                  rect: LayoutRect,
                                  clip: LayoutRect,
                                  is_backface_visible: bool,
                                  color: ColorF) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder.dl_builder.push_rect(&prim_info,
                                             color);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clear_rect(state: &mut WrState,
                                        rect: LayoutRect) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let prim_info = LayoutPrimitiveInfo::new(rect);
    state.frame_builder.dl_builder.push_clear_rect(&prim_info);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_image(state: &mut WrState,
                                   bounds: LayoutRect,
                                   clip: LayoutRect,
                                   is_backface_visible: bool,
                                   stretch_size: LayoutSize,
                                   tile_spacing: LayoutSize,
                                   image_rendering: ImageRendering,
                                   key: WrImageKey) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_image(&prim_info,
                     stretch_size,
                     tile_spacing,
                     image_rendering,
                     key);
}

/// Push a 3 planar yuv image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_planar_image(state: &mut WrState,
                                              bounds: LayoutRect,
                                              clip: LayoutRect,
                                              is_backface_visible: bool,
                                              image_key_0: WrImageKey,
                                              image_key_1: WrImageKey,
                                              image_key_2: WrImageKey,
                                              color_space: WrYuvColorSpace,
                                              image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         YuvData::PlanarYCbCr(image_key_0, image_key_1, image_key_2),
                         color_space,
                         image_rendering);
}

/// Push a 2 planar NV12 image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_NV12_image(state: &mut WrState,
                                            bounds: LayoutRect,
                                            clip: LayoutRect,
                                            is_backface_visible: bool,
                                            image_key_0: WrImageKey,
                                            image_key_1: WrImageKey,
                                            color_space: WrYuvColorSpace,
                                            image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         YuvData::NV12(image_key_0, image_key_1),
                         color_space,
                         image_rendering);
}

/// Push a yuv interleaved image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_interleaved_image(state: &mut WrState,
                                                   bounds: LayoutRect,
                                                   clip: LayoutRect,
                                                   is_backface_visible: bool,
                                                   image_key_0: WrImageKey,
                                                   color_space: WrYuvColorSpace,
                                                   image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         YuvData::InterleavedYCbCr(image_key_0),
                         color_space,
                         image_rendering);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_text(state: &mut WrState,
                                  bounds: LayoutRect,
                                  clip: LayoutRect,
                                  is_backface_visible: bool,
                                  color: ColorF,
                                  font_key: WrFontInstanceKey,
                                  glyphs: *const GlyphInstance,
                                  glyph_count: u32,
                                  glyph_options: *const GlyphOptions) {
    debug_assert!(unsafe { is_in_main_thread() });

    let glyph_slice = make_slice(glyphs, glyph_count as usize);

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_text(&prim_info,
                    &glyph_slice,
                    font_key,
                    color,
                    unsafe { glyph_options.as_ref().cloned() });
}

#[no_mangle]
pub extern "C" fn wr_dp_push_shadow(state: &mut WrState,
                                    bounds: LayoutRect,
                                    clip: LayoutRect,
                                    is_backface_visible: bool,
                                    shadow: Shadow) {
    debug_assert!(unsafe { is_in_main_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder.dl_builder.push_shadow(&prim_info, shadow.into());
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_all_shadows(state: &mut WrState) {
    debug_assert!(unsafe { is_in_main_thread() });

    state.frame_builder.dl_builder.pop_all_shadows();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_line(state: &mut WrState,
                                  clip: &LayoutRect,
                                  is_backface_visible: bool,
                                  bounds: &LayoutRect,
                                  wavy_line_thickness: f32,
                                  orientation: LineOrientation,
                                  color: &ColorF,
                                  style: LineStyle) {
    debug_assert!(unsafe { is_in_main_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(*bounds, (*clip).into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_line(&prim_info,
                    wavy_line_thickness,
                    orientation,
                    color,
                    style);

}

#[no_mangle]
pub extern "C" fn wr_dp_push_border(state: &mut WrState,
                                    rect: LayoutRect,
                                    clip: LayoutRect,
                                    is_backface_visible: bool,
                                    widths: BorderWidths,
                                    top: BorderSide,
                                    right: BorderSide,
                                    bottom: BorderSide,
                                    left: BorderSide,
                                    radius: BorderRadius) {
    debug_assert!(unsafe { is_in_main_thread() });

    let border_details = BorderDetails::Normal(NormalBorder {
                                                   left: left.into(),
                                                   right: right.into(),
                                                   top: top.into(),
                                                   bottom: bottom.into(),
                                                   radius: radius.into(),
                                               });
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_border(&prim_info,
                      widths,
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_image(state: &mut WrState,
                                          rect: LayoutRect,
                                          clip: LayoutRect,
                                          is_backface_visible: bool,
                                          widths: BorderWidths,
                                          image: WrImageKey,
                                          patch: NinePatchDescriptor,
                                          outset: SideOffsets2D<f32>,
                                          repeat_horizontal: RepeatMode,
                                          repeat_vertical: RepeatMode) {
    debug_assert!(unsafe { is_in_main_thread() });
    let border_details =
        BorderDetails::Image(ImageBorder {
                                 image_key: image,
                                 patch: patch.into(),
                                 fill: false,
                                 outset: outset.into(),
                                 repeat_horizontal: repeat_horizontal.into(),
                                 repeat_vertical: repeat_vertical.into(),
                             });
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_border(&prim_info,
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             widths: BorderWidths,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             outset: SideOffsets2D<f32>) {
    debug_assert!(unsafe { is_in_main_thread() });

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
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_border(&prim_info,
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_radial_gradient(state: &mut WrState,
                                                    rect: LayoutRect,
                                                    clip: LayoutRect,
                                                    is_backface_visible: bool,
                                                    widths: BorderWidths,
                                                    center: LayoutPoint,
                                                    radius: LayoutSize,
                                                    stops: *const GradientStop,
                                                    stops_count: usize,
                                                    extend_mode: ExtendMode,
                                                    outset: SideOffsets2D<f32>) {
    debug_assert!(unsafe { is_in_main_thread() });

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
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_border(&prim_info,
                      widths.into(),
                      border_details);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_linear_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_gradient(start_point.into(),
                                         end_point.into(),
                                         stops_vector,
                                         extend_mode.into());
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_gradient(&prim_info,
                        gradient,
                        tile_size.into(),
                        tile_spacing.into());
}

#[no_mangle]
pub extern "C" fn wr_dp_push_radial_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             center: LayoutPoint,
                                             radius: LayoutSize,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = make_slice(stops, stops_count);
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_radial_gradient(center.into(),
                                                radius.into(),
                                                stops_vector,
                                                extend_mode.into());
    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_radial_gradient(&prim_info,
                               gradient,
                               tile_size,
                               tile_spacing);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_box_shadow(state: &mut WrState,
                                        rect: LayoutRect,
                                        clip: LayoutRect,
                                        is_backface_visible: bool,
                                        box_bounds: LayoutRect,
                                        offset: LayoutVector2D,
                                        color: ColorF,
                                        blur_radius: f32,
                                        spread_radius: f32,
                                        border_radius: BorderRadius,
                                        clip_mode: BoxShadowClipMode) {
    debug_assert!(unsafe { is_in_main_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(rect, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder
         .dl_builder
         .push_box_shadow(&prim_info,
                          box_bounds,
                          offset,
                          color,
                          blur_radius,
                          spread_radius,
                          border_radius,
                          clip_mode);
}

#[no_mangle]
pub extern "C" fn wr_dump_display_list(state: &mut WrState) {
    state.frame_builder
         .dl_builder
         .print_display_list();
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
pub extern "C" fn wr_set_item_tag(state: &mut WrState,
                                  scroll_id: u64,
                                  hit_info: u16) {
    state.current_tag = Some((scroll_id, hit_info));
}

#[no_mangle]
pub extern "C" fn wr_clear_item_tag(state: &mut WrState) {
    state.current_tag = None;
}

#[no_mangle]
pub extern "C" fn wr_api_hit_test(dh: &mut DocumentHandle,
                                  point: WorldPoint,
                                  out_pipeline_id: &mut WrPipelineId,
                                  out_scroll_id: &mut u64,
                                  out_hit_info: &mut u16) -> bool {
    let result = dh.api.hit_test(dh.document_id, None, point, HitTestFlags::empty());
    for item in &result.items {
        // For now we should never be getting results back for which the tag is
        // 0 (== CompositorHitTestInfo::eInvisibleToHitTest). In the future if
        // we allow this, we'll want to |continue| on the loop in this scenario.
        debug_assert!(item.tag.1 != 0);
        *out_pipeline_id = item.pipeline;
        *out_scroll_id = item.tag.0;
        *out_hit_info = item.tag.1;
        return true;
    }
    return false;
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
                               tile_size: *const u16,
                               tile_offset: *const TileOffset,
                               output: MutByteSlice)
                               -> bool;
}

type ExternalMessageHandler = unsafe extern "C" fn(msg: *const c_char);

struct WrExternalLogHandler {
    error_msg: ExternalMessageHandler,
    warn_msg: ExternalMessageHandler,
    info_msg: ExternalMessageHandler,
    debug_msg: ExternalMessageHandler,
    trace_msg: ExternalMessageHandler,
    log_level: LogLevel,
}

impl WrExternalLogHandler {
    fn new(log_level: LogLevel) -> WrExternalLogHandler {
        WrExternalLogHandler {
            error_msg: gfx_critical_note,
            warn_msg: gfx_critical_note,
            info_msg: gecko_printf_stderr_output,
            debug_msg: gecko_printf_stderr_output,
            trace_msg: gecko_printf_stderr_output,
            log_level: log_level,
        }
    }
}

impl Log for WrExternalLogHandler {
    fn enabled(&self, metadata : &LogMetadata) -> bool {
        metadata.level() <= self.log_level
    }

    fn log(&self, record: &LogRecord) {
        if self.enabled(record.metadata()) {
            // For file path and line, please check the record.location().
            let msg = CString::new(format!("WR: {}",
                                           record.args())).unwrap();
            unsafe {
                match record.level() {
                    LogLevel::Error => (self.error_msg)(msg.as_ptr()),
                    LogLevel::Warn => (self.warn_msg)(msg.as_ptr()),
                    LogLevel::Info => (self.info_msg)(msg.as_ptr()),
                    LogLevel::Debug => (self.debug_msg)(msg.as_ptr()),
                    LogLevel::Trace => (self.trace_msg)(msg.as_ptr()),
                }
            }
        }
    }
}

#[no_mangle]
pub extern "C" fn wr_init_external_log_handler(log_filter: WrLogLevelFilter) {
    let _ = set_logger(|max_log_level| {
        max_log_level.set(log_filter);
        Box::new(WrExternalLogHandler::new(log_filter.to_log_level()
                                                     .unwrap_or(LogLevel::Error)))
    });
}

#[no_mangle]
pub extern "C" fn wr_shutdown_external_log_handler() {
    let _ = shutdown_logger();
}

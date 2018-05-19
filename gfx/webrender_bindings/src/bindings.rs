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
use webrender::{AsyncPropertySampler, PipelineInfo, SceneBuilderHooks};
use webrender::{ProgramCache, UploadMethod, VertexUsageHint};
use thread_profiler::register_thread_with_profiler;
use moz2d_renderer::Moz2dImageRenderer;
use app_units::Au;
use rayon;
use euclid::SideOffsets2D;

#[cfg(target_os = "windows")]
use dwrote::{FontDescriptor, FontWeight, FontStretch, FontStyle};

#[cfg(target_os = "macos")]
use core_foundation::string::CFString;
#[cfg(target_os = "macos")]
use core_graphics::font::CGFont;

#[repr(C)]
pub enum WrExternalImageBufferType {
    TextureHandle = 0,
    TextureRectHandle = 1,
    TextureArrayHandle = 2,
    TextureExternalHandle = 3,
    ExternalBuffer = 4,
}

impl WrExternalImageBufferType {
    fn to_wr(self) -> ExternalImageType {
        match self {
            WrExternalImageBufferType::TextureHandle =>
                ExternalImageType::TextureHandle(TextureTarget::Default),
            WrExternalImageBufferType::TextureRectHandle =>
                ExternalImageType::TextureHandle(TextureTarget::Rect),
            WrExternalImageBufferType::TextureArrayHandle =>
                ExternalImageType::TextureHandle(TextureTarget::Array),
            WrExternalImageBufferType::TextureExternalHandle =>
                ExternalImageType::TextureHandle(TextureTarget::External),
            WrExternalImageBufferType::ExternalBuffer =>
                ExternalImageType::Buffer,
        }
    }
}

/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
type WrEpoch = Epoch;
/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
/// cbindgen:derive-neq=true
pub type WrIdNamespace = IdNamespace;

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
    pub fn new(api: RenderApi, size: DeviceUintSize, layer: i8) -> DocumentHandle {
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

#[repr(C)]
pub struct FfiVec<T> {
    // We use a *const instead of a *mut because we don't want the C++ side
    // to be mutating this. It is strictly read-only from C++
    data: *const T,
    length: usize,
    capacity: usize,
}

impl<T> FfiVec<T> {
    fn from_vec(v: Vec<T>) -> FfiVec<T> {
        let ret = FfiVec {
            data: v.as_ptr(),
            length: v.len(),
            capacity: v.capacity(),
        };
        mem::forget(v);
        ret
    }
}

impl<T> Drop for FfiVec<T> {
    fn drop(&mut self) {
        // turn the stuff back into a Vec and let it be freed normally
        let _ = unsafe {
            Vec::from_raw_parts(
                self.data as *mut T,
                self.length,
                self.capacity
            )
        };
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
            buffer: slice.as_ptr(),
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
            buffer: slice.as_mut_ptr(),
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
            size: DeviceUintSize::new(self.width, self.height),
            stride: if self.stride != 0 {
                Some(self.stride)
            } else {
                None
            },
            format: self.format,
            is_opaque: self.is_opaque,
            offset: 0,
            allow_mipmaps: false,
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

type LockExternalImageCallback = unsafe extern "C" fn(*mut c_void, WrExternalImageId, u8) -> WrExternalImage;
type UnlockExternalImageCallback = unsafe extern "C" fn(*mut c_void, WrExternalImageId, u8);

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

        let image = unsafe { (self.lock_func)(self.external_image_obj, id.into(), channel_index) };
        ExternalImage {
            uv: TexelRect::new(image.u0, image.v0, image.u1, image.v1),
            source: match image.image_type {
                WrExternalImageType::NativeTexture => ExternalImageSource::NativeTexture(image.handle),
                WrExternalImageType::RawData => ExternalImageSource::RawData(make_slice(image.buff, image.size)),
                WrExternalImageType::Invalid => ExternalImageSource::Invalid,
            },
        }
    }

    fn unlock(&mut self,
              id: ExternalImageId,
              channel_index: u8) {
        unsafe {
            (self.unlock_func)(self.external_image_obj, id.into(), channel_index);
        }
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
  ColorMatrix = 10,
}

#[repr(C)]
#[derive(Copy, Clone)]
pub struct WrFilterOp {
    filter_type: WrFilterOpType,
    argument: c_float, // holds radius for DropShadow; value for other filters
    offset: LayoutVector2D, // only used for DropShadow
    color: ColorF, // only used for DropShadow
    matrix: [f32;20], // only used in ColorMatrix
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

    if symbol.is_null() {
        // XXX Bug 1322949 Make whitelist for extensions
        warn!("Could not find symbol {:?} by glcontext", symbol_name);
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
}

struct CppNotifier {
    window_id: WrWindowId,
}

unsafe impl Send for CppNotifier {}

extern "C" {
    fn wr_notifier_wake_up(window_id: WrWindowId);
    fn wr_notifier_new_frame_ready(window_id: WrWindowId);
    fn wr_notifier_nop_frame_done(window_id: WrWindowId);
    fn wr_notifier_external_event(window_id: WrWindowId,
                                  raw_event: usize);
    fn wr_schedule_render(window_id: WrWindowId);
}

impl RenderNotifier for CppNotifier {
    fn clone(&self) -> Box<RenderNotifier> {
        Box::new(CppNotifier {
            window_id: self.window_id,
        })
    }

    fn wake_up(&self) {
        unsafe {
            wr_notifier_wake_up(self.window_id);
        }
    }

    fn new_frame_ready(&self,
                       _: DocumentId,
                       _scrolled: bool,
                       composite_needed: bool) {
        unsafe {
            if composite_needed {
                wr_notifier_new_frame_ready(self.window_id);
            } else {
                wr_notifier_nop_frame_done(self.window_id);
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
                warn!(" Failed to render: {:?}", e);
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
                              ReadPixelsFormat::Standard(ImageFormat::BGRA8),
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

// cbindgen doesn't support tuples, so we have a little struct instead, with
// an Into implementation to convert from the tuple to the struct.
#[repr(C)]
pub struct WrPipelineEpoch {
    pipeline_id: WrPipelineId,
    epoch: WrEpoch,
}

impl From<(WrPipelineId, WrEpoch)> for WrPipelineEpoch {
    fn from(tuple: (WrPipelineId, WrEpoch)) -> WrPipelineEpoch {
        WrPipelineEpoch {
            pipeline_id: tuple.0,
            epoch: tuple.1
        }
    }
}

#[repr(C)]
pub struct WrPipelineInfo {
    // This contains an entry for each pipeline that was rendered, along with
    // the epoch at which it was rendered. Rendered pipelines include the root
    // pipeline and any other pipelines that were reachable via IFrame display
    // items from the root pipeline.
    epochs: FfiVec<WrPipelineEpoch>,
    // This contains an entry for each pipeline that was removed during the
    // last transaction. These pipelines would have been explicitly removed by
    // calling remove_pipeline on the transaction object; the pipeline showing
    // up in this array means that the data structures have been torn down on
    // the webrender side, and so any remaining data structures on the caller
    // side can now be torn down also.
    removed_pipelines: FfiVec<PipelineId>,
}

impl WrPipelineInfo {
    fn new(info: PipelineInfo) -> Self {
        WrPipelineInfo {
            epochs: FfiVec::from_vec(info.epochs.into_iter().map(WrPipelineEpoch::from).collect()),
            removed_pipelines: FfiVec::from_vec(info.removed_pipelines),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wr_renderer_flush_pipeline_info(renderer: &mut Renderer) -> WrPipelineInfo {
    let info = renderer.flush_pipeline_info();
    WrPipelineInfo::new(info)
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_pipeline_info_delete(_info: WrPipelineInfo) {
    // _info will be dropped here, and the drop impl on FfiVec will free
    // the underlying vec memory
}

#[allow(improper_ctypes)] // this is needed so that rustc doesn't complain about passing the &mut Transaction to an extern function
extern "C" {
    // These callbacks are invoked from the scene builder thread (aka the APZ
    // updater thread)
    fn apz_register_updater(window_id: WrWindowId);
    fn apz_pre_scene_swap(window_id: WrWindowId);
    // This function takes ownership of the pipeline_info and is responsible for
    // freeing it via wr_pipeline_info_delete.
    fn apz_post_scene_swap(window_id: WrWindowId, pipeline_info: WrPipelineInfo);
    fn apz_run_updater(window_id: WrWindowId);
    fn apz_deregister_updater(window_id: WrWindowId);

    // These callbacks are invoked from the render backend thread (aka the APZ
    // sampler thread)
    fn apz_register_sampler(window_id: WrWindowId);
    fn apz_sample_transforms(window_id: WrWindowId, transaction: &mut Transaction);
    fn apz_deregister_sampler(window_id: WrWindowId);
}

struct APZCallbacks {
    window_id: WrWindowId,
}

impl APZCallbacks {
    pub fn new(window_id: WrWindowId) -> Self {
        APZCallbacks {
            window_id,
        }
    }
}

impl SceneBuilderHooks for APZCallbacks {
    fn register(&self) {
        unsafe { apz_register_updater(self.window_id) }
    }

    fn pre_scene_swap(&self) {
        unsafe { apz_pre_scene_swap(self.window_id) }
    }

    fn post_scene_swap(&self, info: PipelineInfo) {
        let info = WrPipelineInfo::new(info);
        unsafe { apz_post_scene_swap(self.window_id, info) }

        // After a scene swap we should schedule a render for the next vsync,
        // otherwise there's no guarantee that the new scene will get rendered
        // anytime soon
        unsafe { wr_schedule_render(self.window_id) }
    }

    fn poke(&self) {
        unsafe { apz_run_updater(self.window_id) }
    }

    fn deregister(&self) {
        unsafe { apz_deregister_updater(self.window_id) }
    }
}

struct SamplerCallback {
    window_id: WrWindowId,
}

impl SamplerCallback {
    pub fn new(window_id: WrWindowId) -> Self {
        SamplerCallback {
            window_id,
        }
    }
}

impl AsyncPropertySampler for SamplerCallback {
    fn register(&self) {
        unsafe { apz_register_sampler(self.window_id) }
    }

    fn sample(&self) -> Vec<FrameMsg> {
        let mut transaction = Transaction::new();
        unsafe { apz_sample_transforms(self.window_id, &mut transaction) };
        // TODO: also omta_sample_transforms(...)
        transaction.get_frame_ops()
    }

    fn deregister(&self) {
        unsafe { apz_deregister_sampler(self.window_id) }
    }
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
    let worker = rayon::ThreadPoolBuilder::new()
        .thread_name(|idx|{ format!("WRWorker#{}", idx) })
        .start_handler(|idx| {
            let name = format!("WRWorker#{}", idx);
            register_thread_with_profiler(name.clone());
            gecko_profiler_register_thread(CString::new(name).unwrap().as_ptr());
        })
        .exit_handler(|_idx| {
            gecko_profiler_unregister_thread();
        })
        .build();

    let workers = Arc::new(worker.unwrap());

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

    info!("WebRender - OpenGL version new {}", version);

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
        scene_builder_hooks: Some(Box::new(APZCallbacks::new(window_id))),
        sampler: Some(Box::new(SamplerCallback::new(window_id))),
        ..Default::default()
    };

    let notifier = Box::new(CppNotifier {
        window_id: window_id,
    });
    let (renderer, sender) = match Renderer::new(gl, notifier, opts) {
        Ok((renderer, sender)) => (renderer, sender),
        Err(e) => {
            warn!(" Failed to create a Renderer: {:?}", e);
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
    let layer = 0;
    *out_handle = Box::into_raw(Box::new(
            DocumentHandle::new(sender.create_api(), window_size, layer)));
    *out_renderer = Box::into_raw(Box::new(renderer));

    return true;
}

#[no_mangle]
pub extern "C" fn wr_api_create_document(
    root_dh: &mut DocumentHandle,
    out_handle: &mut *mut DocumentHandle,
    doc_size: DeviceUintSize,
    layer: i8,
) {
    assert!(unsafe { is_in_compositor_thread() });

    *out_handle = Box::into_raw(Box::new(DocumentHandle::new(
        root_dh.api.clone_sender().create_api(),
        doc_size,
        layer
    )));
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_api_delete_document(dh: &mut DocumentHandle) {
    dh.api.delete_document(dh.document_id);
}

#[no_mangle]
pub extern "C" fn wr_api_clone(
    dh: &mut DocumentHandle,
    out_handle: &mut *mut DocumentHandle
) {
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
    let _ = Box::from_raw(dh);
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub unsafe extern "C" fn wr_api_shut_down(dh: &mut DocumentHandle) {
    dh.api.shut_down();
}

fn make_transaction(do_async: bool) -> Transaction {
    let mut transaction = Transaction::new();
    // Ensure that we either use async scene building or not based on the
    // gecko pref, regardless of what the default is. We can remove this once
    // the scene builder thread is enabled everywhere and working well.
    if do_async {
        transaction.use_scene_builder_thread();
    } else {
        transaction.skip_scene_builder();
    }
    transaction
}

#[no_mangle]
pub extern "C" fn wr_transaction_new(do_async: bool) -> *mut Transaction {
    Box::into_raw(Box::new(make_transaction(do_async)))
}

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
#[no_mangle]
pub extern "C" fn wr_transaction_delete(txn: *mut Transaction) {
    unsafe { let _ = Box::from_raw(txn); }
}

#[no_mangle]
pub extern "C" fn wr_transaction_is_empty(txn: &Transaction) -> bool {
    txn.is_empty()
}

#[no_mangle]
pub extern "C" fn wr_transaction_update_epoch(
    txn: &mut Transaction,
    pipeline_id: WrPipelineId,
    epoch: WrEpoch,
) {
    txn.update_epoch(pipeline_id, epoch);
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_root_pipeline(
    txn: &mut Transaction,
    pipeline_id: WrPipelineId,
) {
    txn.set_root_pipeline(pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_transaction_remove_pipeline(
    txn: &mut Transaction,
    pipeline_id: WrPipelineId,
) {
    txn.remove_pipeline(pipeline_id);
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_display_list(
    txn: &mut Transaction,
    epoch: WrEpoch,
    background: ColorF,
    viewport_width: f32,
    viewport_height: f32,
    pipeline_id: WrPipelineId,
    content_size: LayoutSize,
    dl_descriptor: BuiltDisplayListDescriptor,
    dl_data: &mut WrVecU8,
) {
    let color = if background.a == 0.0 { None } else { Some(background) };

    // See the documentation of set_display_list in api.rs. I don't think
    // it makes a difference in gecko at the moment(until APZ is figured out)
    // but I suppose it is a good default.
    let preserve_frame_state = true;

    let dl_vec = dl_data.flush_into_vec();
    let dl = BuiltDisplayList::from_data(dl_vec, dl_descriptor);

    txn.set_display_list(
        epoch,
        color,
        LayoutSize::new(viewport_width, viewport_height),
        (pipeline_id, content_size, dl),
        preserve_frame_state,
    );
}

#[no_mangle]
pub extern "C" fn wr_transaction_update_resources(
    txn: &mut Transaction,
    resource_updates: &mut ResourceUpdates
) {
    if resource_updates.updates.is_empty() {
        return;
    }
    txn.update_resources(mem::replace(resource_updates, ResourceUpdates::new()));
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_window_parameters(
    txn: &mut Transaction,
    window_size: &DeviceUintSize,
    doc_rect: &DeviceUintRect,
) {
    txn.set_window_parameters(
        *window_size,
        *doc_rect,
        1.0,
    );
}

#[no_mangle]
pub extern "C" fn wr_transaction_generate_frame(txn: &mut Transaction) {
    txn.generate_frame();
}

#[no_mangle]
pub extern "C" fn wr_transaction_update_dynamic_properties(
    txn: &mut Transaction,
    opacity_array: *const WrOpacityProperty,
    opacity_count: usize,
    transform_array: *const WrTransformProperty,
    transform_count: usize,
) {
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

    txn.update_dynamic_properties(properties);
}

#[no_mangle]
pub extern "C" fn wr_transaction_append_transform_properties(
    txn: &mut Transaction,
    transform_array: *const WrTransformProperty,
    transform_count: usize,
) {
    if transform_count == 0 {
        return;
    }

    let mut properties = DynamicProperties {
        transforms: Vec::new(),
        floats: Vec::new(),
    };

    let transform_slice = make_slice(transform_array, transform_count);

    for element in transform_slice.iter() {
        let prop = PropertyValue {
            key: PropertyBindingKey::new(element.id),
            value: element.transform.into(),
        };

        properties.transforms.push(prop);
    }

    txn.append_dynamic_properties(properties);
}

#[no_mangle]
pub extern "C" fn wr_transaction_scroll_layer(
    txn: &mut Transaction,
    pipeline_id: WrPipelineId,
    scroll_id: u64,
    new_scroll_origin: LayoutPoint
) {
    let scroll_id = ExternalScrollId(scroll_id, pipeline_id);
    txn.scroll_node_with_id(new_scroll_origin, scroll_id, ScrollClamping::NoClamping);
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
                image_type: buffer_type.to_wr(),
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
                image_type: image_type.to_wr(),
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
pub extern "C" fn wr_api_send_transaction(
    dh: &mut DocumentHandle,
    transaction: &mut Transaction,
    is_async: bool
) {
    if transaction.is_empty() {
        return;
    }
    let new_txn = make_transaction(is_async);
    let txn = mem::replace(transaction, new_txn);
    dh.api.send_transaction(dh.document_id, txn);
}

#[no_mangle]
pub unsafe extern "C" fn wr_transaction_clear_display_list(
    txn: &mut Transaction,
    epoch: WrEpoch,
    pipeline_id: WrPipelineId,
) {
    let preserve_frame_state = true;
    let frame_builder = WebRenderFrameBuilder::new(pipeline_id, LayoutSize::zero());

    txn.set_display_list(
        epoch,
        None,
        LayoutSize::new(0.0, 0.0),
        frame_builder.dl_builder.finalize(),
        preserve_frame_state,
    );
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

#[no_mangle]
pub extern "C" fn wr_api_capture(
    dh: &mut DocumentHandle,
    path: *const c_char,
    bits_raw: u32,
) {
    use std::fs::{File, create_dir_all};
    use std::io::Write;

    let cstr = unsafe { CStr::from_ptr(path) };
    let path = PathBuf::from(&*cstr.to_string_lossy());

    let _ = create_dir_all(&path);
    match File::create(path.join("wr.txt")) {
        Ok(mut file) => {
            let revision = include_bytes!("../revision.txt");
            file.write(revision).unwrap();
        }
        Err(e) => {
            warn!("Unable to create path '{:?}' for capture: {:?}", path, e);
            return
        }
    }

    let bits = CaptureBits::from_bits(bits_raw as _).unwrap();
    dh.api.save_capture(path, bits);
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

#[no_mangle]
pub unsafe extern "C" fn wr_api_wake_scene_builder(dh: &mut DocumentHandle) {
    dh.api.wake_scene_builder();
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_flush_scene_builder(dh: &mut DocumentHandle) {
    dh.api.flush_scene_builder();
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
                                              clip_node_id: *const usize,
                                              animation: *const WrAnimationProperty,
                                              opacity: *const f32,
                                              transform: *const LayoutTransform,
                                              transform_style: TransformStyle,
                                              perspective: *const LayoutTransform,
                                              mix_blend_mode: MixBlendMode,
                                              filters: *const WrFilterOp,
                                              filter_count: usize,
                                              is_backface_visible: bool,
                                              glyph_raster_space: GlyphRasterSpace,
                                              out_is_reference_frame: &mut bool,
                                              out_reference_frame_id: &mut usize) {
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
            WrFilterOpType::ColorMatrix => FilterOp::ColorMatrix(c_filter.matrix),
        }
    }).collect();

    let clip_node_id_ref = unsafe { clip_node_id.as_ref() };
    let clip_node_id = match clip_node_id_ref {
        Some(clip_node_id) => Some(ClipId::Clip(*clip_node_id, state.pipeline_id)),
        None => None,
    };

    let transform_ref = unsafe { transform.as_ref() };
    let mut transform_binding = match transform_ref {
        Some(transform) => Some(PropertyBinding::Value(transform.clone())),
        None => None,
    };

    let opacity_ref = unsafe { opacity.as_ref() };
    let mut has_opacity_animation = false;
    let anim = unsafe { animation.as_ref() };
    if let Some(anim) = anim {
        debug_assert!(anim.id > 0);
        match anim.effect_type {
            WrAnimationType::Opacity => {
                filters.push(FilterOp::Opacity(PropertyBinding::Binding(PropertyBindingKey::new(anim.id),
                                                                        // We have to set the static opacity value as
                                                                        // the value for the case where the animation is
                                                                        // in not in-effect (e.g. in the delay phase
                                                                        // with no corresponding fill mode).
                                                                        opacity_ref.cloned().unwrap_or(1.0)),
                                                                        1.0));
                has_opacity_animation = true;
            },
            WrAnimationType::Transform => {
                transform_binding =
                    Some(PropertyBinding::Binding(PropertyBindingKey::new(anim.id),
                                                  // Same as above opacity case.
                                                  transform_ref.cloned().unwrap_or(LayoutTransform::identity())));
            },
        }
    }

    if let Some(opacity) = opacity_ref {
        if !has_opacity_animation && *opacity < 1.0 {
            filters.push(FilterOp::Opacity(PropertyBinding::Value(*opacity), *opacity));
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

    let ref_frame_id = state.frame_builder
         .dl_builder
         .push_stacking_context(&prim_info,
                                clip_node_id,
                                transform_binding,
                                transform_style,
                                perspective,
                                mix_blend_mode,
                                filters,
                                glyph_raster_space);
    if let Some(ClipId::Clip(id, pipeline_id)) = ref_frame_id {
        assert!(pipeline_id == state.pipeline_id);
        *out_is_reference_frame = true;
        *out_reference_frame_id = id;
    } else {
        *out_is_reference_frame = false;
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_stacking_context(state: &mut WrState) {
    debug_assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_stacking_context();
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clipchain(state: &mut WrState,
                                         parent_clipchain_id: *const u64,
                                         clips: *const usize,
                                         clips_count: usize)
                                         -> u64 {
    debug_assert!(unsafe { is_in_main_thread() });
    let parent = unsafe { parent_clipchain_id.as_ref() }.map(|id| ClipChainId(*id, state.pipeline_id));
    let clips_slice : Vec<ClipId> = make_slice(clips, clips_count).iter().map(|id| ClipId::Clip(*id, state.pipeline_id)).collect();
    let clipchain_id = state.frame_builder.dl_builder.define_clip_chain(parent, clips_slice);
    assert!(clipchain_id.1 == state.pipeline_id);
    clipchain_id.0
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clip(state: &mut WrState,
                                    parent_id: *const usize,
                                    clip_rect: LayoutRect,
                                    complex: *const ComplexClipRegion,
                                    complex_count: usize,
                                    mask: *const WrImageMask)
                                    -> usize {
    debug_assert!(unsafe { is_in_main_thread() });

    let parent_id = unsafe { parent_id.as_ref() };
    let complex_slice = make_slice(complex, complex_count);
    let complex_iter = complex_slice.iter().cloned();
    let mask : Option<ImageMask> = unsafe { mask.as_ref() }.map(|x| x.into());

    let clip_id = if let Some(&pid) = parent_id {
        state.frame_builder.dl_builder.define_clip_with_parent(
            ClipId::Clip(pid, state.pipeline_id),
            clip_rect, complex_iter, mask)
    } else {
        state.frame_builder.dl_builder.define_clip(clip_rect, complex_iter, mask)
    };
    // return the usize id value from inside the ClipId::Clip(..)
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
                                  clip_id: usize) {
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
                                            -> usize {
    assert!(unsafe { is_in_main_thread() });
    let clip_id = state.frame_builder.dl_builder.define_sticky_frame(
        content_rect, SideOffsets2D::new(
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
                                            parent_id: *const usize,
                                            content_rect: LayoutRect,
                                            clip_rect: LayoutRect)
                                            -> usize {
    assert!(unsafe { is_in_main_thread() });

    let parent_id = unsafe { parent_id.as_ref() };

    let new_id = if let Some(&pid) = parent_id {
        state.frame_builder.dl_builder.define_scroll_frame_with_parent(
            ClipId::Clip(pid, state.pipeline_id),
            Some(ExternalScrollId(scroll_id, state.pipeline_id)),
            content_rect,
            clip_rect,
            vec![],
            None,
            ScrollSensitivity::Script
        )
    } else {
        state.frame_builder.dl_builder.define_scroll_frame(
            Some(ExternalScrollId(scroll_id, state.pipeline_id)),
            content_rect,
            clip_rect,
            vec![],
            None,
            ScrollSensitivity::Script
        )
    };

    match new_id {
        ClipId::Clip(id, pipeline_id) => {
            assert!(pipeline_id == state.pipeline_id);
            id
        },
        _ => panic!("Got unexpected clip id type"),
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_push_scroll_layer(state: &mut WrState,
                                          scroll_id: usize) {
    debug_assert!(unsafe { is_in_main_thread() });
    let clip_id = ClipId::Clip(scroll_id, state.pipeline_id);
    state.frame_builder.dl_builder.push_clip_id(clip_id);
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_scroll_layer(state: &mut WrState) {
    debug_assert!(unsafe { is_in_main_thread() });
    state.frame_builder.dl_builder.pop_clip_id();
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clip_and_scroll_info(state: &mut WrState,
                                                  scroll_id: usize,
                                                  clip_chain_id: *const u64) {
    debug_assert!(unsafe { is_in_main_thread() });

    let clip_chain_id = unsafe { clip_chain_id.as_ref() };
    let info = if let Some(&ccid) = clip_chain_id {
        ClipAndScrollInfo::new(
            ClipId::Clip(scroll_id, state.pipeline_id),
            ClipId::ClipChain(ClipChainId(ccid, state.pipeline_id)))
    } else {
        ClipAndScrollInfo::simple(
            ClipId::Clip(scroll_id, state.pipeline_id))
    };
    state.frame_builder.dl_builder.push_clip_and_scroll_info(info);
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
                                    pipeline_id: WrPipelineId,
                                    ignore_missing_pipeline: bool) {
    debug_assert!(unsafe { is_in_main_thread() });

    let mut prim_info = LayoutPrimitiveInfo::new(rect);
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    state.frame_builder.dl_builder.push_iframe(&prim_info, pipeline_id, ignore_missing_pipeline);
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
                                   key: WrImageKey,
                                   premultiplied_alpha: bool) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let mut prim_info = LayoutPrimitiveInfo::with_clip_rect(bounds, clip.into());
    prim_info.is_backface_visible = is_backface_visible;
    prim_info.tag = state.current_tag;
    let alpha_type = if premultiplied_alpha {
        AlphaType::PremultipliedAlpha
    } else {
        AlphaType::Alpha
    };
    state.frame_builder
         .dl_builder
         .push_image(&prim_info,
                     stretch_size,
                     tile_spacing,
                     image_rendering,
                     alpha_type,
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
                                          width: u32,
                                          height: u32,
                                          slice: SideOffsets2D<u32>,
                                          outset: SideOffsets2D<f32>,
                                          repeat_horizontal: RepeatMode,
                                          repeat_vertical: RepeatMode) {
    debug_assert!(unsafe { is_in_main_thread() });
    let border_details = BorderDetails::NinePatch(NinePatchBorder {
        source: NinePatchBorderSource::Image(image),
        width,
        height,
        slice,
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

/// cbindgen:postfix=WR_DESTRUCTOR_SAFE_FUNC
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
                               dirty_rect: *const DeviceUintRect,
                               output: MutByteSlice)
                               -> bool;
}

#[no_mangle]
pub extern "C" fn wr_root_scroll_node_id() -> usize {
    // The PipelineId doesn't matter here, since we just want the numeric part of the id
    // produced for any given root reference frame.
    match ClipId::root_scroll_node(PipelineId(0, 0)) {
        ClipId::Clip(id, _) => id,
        _ => unreachable!("Got a non Clip ClipId for root reference frame."),
    }
}

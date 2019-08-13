use std::ffi::{CStr, CString};
#[cfg(not(target_os = "macos"))]
use std::ffi::OsString;
#[cfg(target_os = "windows")]
use std::os::windows::ffi::OsStringExt;
#[cfg(not(any(target_os = "macos", target_os = "windows")))]
use std::os::unix::ffi::OsStringExt;
use std::io::Cursor;
use std::{mem, slice, ptr, env};
use std::marker::PhantomData;
use std::path::PathBuf;
use std::rc::Rc;
use std::cell::RefCell;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::ops::Range;
use std::os::raw::{c_void, c_char, c_float};
#[cfg(target_os = "android")]
use std::os::raw::{c_int};
use std::time::Duration;
use gleam::gl;

use webrender::{
    api::*, api::units::*, ApiRecordingReceiver, AsyncPropertySampler, AsyncScreenshotHandle,
    BinaryRecorder, DebugFlags, Device, ExternalImage, ExternalImageHandler, ExternalImageSource,
    PipelineInfo, ProfilerHooks, RecordedFrameHandle, Renderer, RendererOptions, RendererStats,
    SceneBuilderHooks, ShaderPrecacheFlags, Shaders, ThreadListener, UploadMethod, VertexUsageHint,
    WrShaders, set_profiler_hooks,
};
use thread_profiler::register_thread_with_profiler;
use moz2d_renderer::Moz2dBlobImageHandler;
use program_cache::{WrProgramCache, remove_disk_cache};
use rayon;
use num_cpus;
use euclid::SideOffsets2D;
use nsstring::nsAString;

#[cfg(target_os = "macos")]
use core_foundation::string::CFString;
#[cfg(target_os = "macos")]
use core_graphics::font::CGFont;

extern "C" {
    #[cfg(target_os = "android")]
    fn __android_log_write(prio: c_int, tag: *const c_char, text: *const c_char) -> c_int;
}

/// The unique id for WR resource identification.
static NEXT_NAMESPACE_ID: AtomicUsize = AtomicUsize::new(1);

/// Special value handled in this wrapper layer to signify a redundant clip chain.
pub const ROOT_CLIP_CHAIN: u64 = !0;

fn next_namespace_id() -> IdNamespace {
    IdNamespace(NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed) as u32)
}

/// Whether a border should be antialiased.
#[repr(C)]
#[derive(Eq, PartialEq, Copy, Clone)]
pub enum AntialiasBorder {
    No = 0,
    Yes,
}

/// Used to indicate if an image is opaque, or has an alpha channel.
#[repr(u8)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum OpacityType {
    Opaque = 0,
    HasAlphaChannel = 1,
}

/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
/// cbindgen:derive-neq=true
type WrEpoch = Epoch;
/// cbindgen:field-names=[mHandle]
/// cbindgen:derive-lt=true
/// cbindgen:derive-lte=true
/// cbindgen:derive-neq=true
pub type WrIdNamespace = IdNamespace;

/// cbindgen:field-names=[mNamespace, mHandle]
type WrDocumentId = DocumentId;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrPipelineId = PipelineId;
/// cbindgen:field-names=[mNamespace, mHandle]
/// cbindgen:derive-neq=true
type WrImageKey = ImageKey;
/// cbindgen:field-names=[mNamespace, mHandle]
pub type WrFontKey = FontKey;
/// cbindgen:field-names=[mNamespace, mHandle]
pub type WrFontInstanceKey = FontInstanceKey;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrYuvColorSpace = YuvColorSpace;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrColorDepth = ColorDepth;
/// cbindgen:field-names=[mNamespace, mHandle]
type WrColorRange = ColorRange;


#[repr(C)]
pub struct WrSpaceAndClip {
    space: WrSpatialId,
    clip: WrClipId,
}

impl WrSpaceAndClip {
    fn from_webrender(sac: SpaceAndClipInfo) -> Self {
        WrSpaceAndClip {
            space: WrSpatialId { id: sac.spatial_id.0 },
            clip: WrClipId::from_webrender(sac.clip_id),
        }
    }

    fn to_webrender(&self, pipeline_id: WrPipelineId) -> SpaceAndClipInfo {
        SpaceAndClipInfo {
            spatial_id: self.space.to_webrender(pipeline_id),
            clip_id: self.clip.to_webrender(pipeline_id),
        }
    }
}

#[inline]
fn clip_chain_id_to_webrender(id: u64, pipeline_id: WrPipelineId) -> ClipId {
    if id == ROOT_CLIP_CHAIN {
        ClipId::root(pipeline_id)
    } else {
        ClipId::ClipChain(ClipChainId(id, pipeline_id))
    }
}

#[repr(C)]
pub struct WrSpaceAndClipChain {
    space: WrSpatialId,
    clip_chain: u64,
}

impl WrSpaceAndClipChain {
    fn to_webrender(&self, pipeline_id: WrPipelineId) -> SpaceAndClipInfo {
        //Warning: special case here to support dummy clip chain
        SpaceAndClipInfo {
            spatial_id: self.space.to_webrender(pipeline_id),
            clip_id: clip_chain_id_to_webrender(self.clip_chain, pipeline_id),
        }
    }
}

#[repr(C)]
pub enum WrStackingContextClip {
    None,
    ClipId(WrClipId),
    ClipChain(u64),
}

impl WrStackingContextClip {
    fn to_webrender(&self, pipeline_id: WrPipelineId) -> Option<ClipId> {
        match *self {
            WrStackingContextClip::None => None,
            WrStackingContextClip::ClipChain(id) => Some(clip_chain_id_to_webrender(id, pipeline_id)),
            WrStackingContextClip::ClipId(id) => Some(id.to_webrender(pipeline_id)),
        }
    }
}

unsafe fn make_slice<'a, T>(ptr: *const T, len: usize) -> &'a [T] {
    if ptr.is_null() {
        &[]
    } else {
        slice::from_raw_parts(ptr, len)
    }
}

unsafe fn make_slice_mut<'a, T>(ptr: *mut T, len: usize) -> &'a mut [T] {
    if ptr.is_null() {
        &mut []
    } else {
        slice::from_raw_parts_mut(ptr, len)
    }
}

pub struct DocumentHandle {
    api: RenderApi,
    document_id: DocumentId,
}

impl DocumentHandle {
    pub fn new(api: RenderApi, size: DeviceIntSize, layer: i8) -> DocumentHandle {
        let doc = api.add_document(size, layer);
        DocumentHandle {
            api: api,
            document_id: doc
        }
    }

    pub fn new_with_id(api: RenderApi, size: DeviceIntSize, layer: i8, id: u32) -> DocumentHandle {
        let doc = api.add_document_with_id(size, layer, id);
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
pub struct ByteSlice<'a> {
    buffer: *const u8,
    len: usize,
    _phantom: PhantomData<&'a ()>,
}

impl<'a> ByteSlice<'a> {
    pub fn new(slice: &'a [u8]) -> ByteSlice<'a> {
        ByteSlice {
            buffer: slice.as_ptr(),
            len: slice.len(),
            _phantom: PhantomData,
        }
    }

    pub fn as_slice(&self) -> &'a [u8] {
        unsafe { make_slice(self.buffer, self.len) }
    }
}

#[repr(C)]
pub struct MutByteSlice<'a> {
    buffer: *mut u8,
    len: usize,
    _phantom: PhantomData<&'a ()>,
}

impl<'a> MutByteSlice<'a> {
    pub fn new(slice: &'a mut [u8]) -> MutByteSlice<'a> {
        let len = slice.len();
        MutByteSlice {
            buffer: slice.as_mut_ptr(),
            len: len,
            _phantom: PhantomData,
        }
    }

    pub fn as_mut_slice(&mut self) -> &'a mut [u8] {
        unsafe { make_slice_mut(self.buffer, self.len) }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct WrImageDescriptor {
    pub format: ImageFormat,
    pub width: i32,
    pub height: i32,
    pub stride: i32,
    pub opacity: OpacityType,
}

impl<'a> Into<ImageDescriptor> for &'a WrImageDescriptor {
    fn into(self) -> ImageDescriptor {
        ImageDescriptor {
            size: DeviceIntSize::new(self.width, self.height),
            stride: if self.stride != 0 {
                Some(self.stride)
            } else {
                None
            },
            format: self.format,
            is_opaque: self.opacity == OpacityType::Opaque,
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

extern "C" {
    fn wr_renderer_lock_external_image(
        renderer: *mut c_void,
        external_image_id: WrExternalImageId,
        channel_index: u8,
        rendering: ImageRendering) -> WrExternalImage;
    fn wr_renderer_unlock_external_image(
        renderer: *mut c_void,
        external_image_id: WrExternalImageId,
        channel_index: u8);
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct WrExternalImageHandler {
    external_image_obj: *mut c_void,
}

impl ExternalImageHandler for WrExternalImageHandler {
    fn lock(&mut self,
            id: ExternalImageId,
            channel_index: u8,
            rendering: ImageRendering)
            -> ExternalImage {

        let image = unsafe { wr_renderer_lock_external_image(self.external_image_obj, id.into(), channel_index, rendering) };
        ExternalImage {
            uv: TexelRect::new(image.u0, image.v0, image.u1, image.v1),
            source: match image.image_type {
                WrExternalImageType::NativeTexture => ExternalImageSource::NativeTexture(image.handle),
                WrExternalImageType::RawData => ExternalImageSource::RawData(unsafe { make_slice(image.buff, image.size) }),
                WrExternalImageType::Invalid => ExternalImageSource::Invalid,
            },
        }
    }

    fn unlock(&mut self,
              id: ExternalImageId,
              channel_index: u8) {
        unsafe {
            wr_renderer_unlock_external_image(self.external_image_obj, id.into(), channel_index);
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
// Used for ComponentTransfer only
pub struct WrFilterData {
    funcR_type: ComponentTransferFuncType,
    R_values: *mut c_float,
    R_values_count: usize,
    funcG_type: ComponentTransferFuncType,
    G_values: *mut c_float,
    G_values_count: usize,
    funcB_type: ComponentTransferFuncType,
    B_values: *mut c_float,
    B_values_count: usize,
    funcA_type: ComponentTransferFuncType,
    A_values: *mut c_float,
    A_values_count: usize,
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

#[repr(C)]
pub enum TelemetryProbe {
    SceneBuildTime = 0,
    SceneSwapTime = 1,
    RenderTime = 2,
}

extern "C" {
    fn is_in_compositor_thread() -> bool;
    fn is_in_render_thread() -> bool;
    fn is_in_main_thread() -> bool;
    fn is_glcontext_gles(glcontext_ptr: *mut c_void) -> bool;
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
    fn record_telemetry_time(probe: TelemetryProbe, time_ns: u64);
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
    fn wr_schedule_render(window_id: WrWindowId,
                          document_id_array: *const WrDocumentId,
                          document_id_count: usize);
    fn wr_finished_scene_build(window_id: WrWindowId,
                               document_id_array: *const WrDocumentId,
                               document_id_count: usize,
                               pipeline_info: WrPipelineInfo);

    fn wr_transaction_notification_notified(handler: usize, when: Checkpoint);
}

impl RenderNotifier for CppNotifier {
    fn clone(&self) -> Box<dyn RenderNotifier> {
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
                       composite_needed: bool,
                       render_time_ns: Option<u64>) {
        unsafe {
            if let Some(time) = render_time_ns {
                record_telemetry_time(TelemetryProbe::RenderTime, time);
            }
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
                                                         external_image_handler: &mut WrExternalImageHandler) {
    renderer.set_external_image_handler(Box::new(external_image_handler.clone()));
}

#[no_mangle]
pub extern "C" fn wr_renderer_update(renderer: &mut Renderer) {
    renderer.update();
}

#[no_mangle]
pub extern "C" fn wr_renderer_render(renderer: &mut Renderer,
                                     width: i32,
                                     height: i32,
                                     had_slow_frame: bool,
                                     out_stats: &mut RendererStats) -> bool {
    if had_slow_frame {
      renderer.notify_slow_frame();
    }
    match renderer.render(DeviceIntSize::new(width, height)) {
        Ok(results) => {
            *out_stats = results.stats;
            true
        }
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

#[no_mangle]
pub extern "C" fn wr_renderer_record_frame(
    renderer: &mut Renderer,
    image_format: ImageFormat,
    out_handle: &mut RecordedFrameHandle,
    out_width: &mut i32,
    out_height: &mut i32,
) -> bool {
    if let Some((handle, size)) = renderer.record_frame(image_format) {
        *out_handle = handle;
        *out_width = size.width;
        *out_height = size.height;

        true
    } else {
        false
    }
}

#[no_mangle]
pub extern "C" fn wr_renderer_map_recorded_frame(
    renderer: &mut Renderer,
    handle: RecordedFrameHandle,
    dst_buffer: *mut u8,
    dst_buffer_len: usize,
    dst_stride: usize,
) -> bool {
    renderer.map_recorded_frame(
        handle,
        unsafe { make_slice_mut(dst_buffer, dst_buffer_len) },
        dst_stride,
    )
}

#[no_mangle]
pub extern "C" fn wr_renderer_release_composition_recorder_structures(
    renderer: &mut Renderer,
) {
    renderer.release_composition_recorder_structures();
}

#[no_mangle]
pub extern "C" fn wr_renderer_get_screenshot_async(
    renderer: &mut Renderer,
    window_x: i32,
    window_y: i32,
    window_width: i32,
    window_height: i32,
    buffer_width: i32,
    buffer_height: i32,
    image_format: ImageFormat,
    screenshot_width: *mut i32,
    screenshot_height: *mut i32,
) -> AsyncScreenshotHandle {
    assert!(!screenshot_width.is_null());
    assert!(!screenshot_height.is_null());

    let (handle, size) = renderer.get_screenshot_async(
        DeviceIntRect::new(
            DeviceIntPoint::new(window_x, window_y),
            DeviceIntSize::new(window_width, window_height),
        ),
        DeviceIntSize::new(buffer_width, buffer_height),
        image_format,
    );

    unsafe {
        *screenshot_width = size.width;
        *screenshot_height = size.height;
    }

    handle
}

#[no_mangle]
pub extern "C" fn wr_renderer_map_and_recycle_screenshot(
    renderer: &mut Renderer,
    handle: AsyncScreenshotHandle,
    dst_buffer: *mut u8,
    dst_buffer_len: usize,
    dst_stride: usize,
) -> bool {
    renderer.map_and_recycle_screenshot(
        handle,
        unsafe { make_slice_mut(dst_buffer, dst_buffer_len) },
        dst_stride,
    )
}

#[no_mangle]
pub extern "C" fn wr_renderer_release_profiler_structures(renderer: &mut Renderer) {
    renderer.release_profiler_structures();
}

// Call wr_renderer_render() before calling this function.
#[no_mangle]
pub unsafe extern "C" fn wr_renderer_readback(renderer: &mut Renderer,
                                              width: i32,
                                              height: i32,
                                              format: ImageFormat,
                                              dst_buffer: *mut u8,
                                              buffer_size: usize) {
    assert!(is_in_render_thread());

    let mut slice = make_slice_mut(dst_buffer, buffer_size);
    renderer.read_pixels_into(FramebufferIntSize::new(width, height).into(),
                              format, &mut slice);
}

#[no_mangle]
pub unsafe extern "C" fn wr_renderer_delete(renderer: *mut Renderer) {
    let renderer = Box::from_raw(renderer);
    renderer.deinit();
    // let renderer go out of scope and get dropped
}

#[no_mangle]
pub unsafe extern "C" fn wr_renderer_accumulate_memory_report(renderer: &mut Renderer,
                                                              report: &mut MemoryReport) {
    *report += renderer.report_memory();
}

// cbindgen doesn't support tuples, so we have a little struct instead, with
// an Into implementation to convert from the tuple to the struct.
#[repr(C)]
pub struct WrPipelineEpoch {
    pipeline_id: WrPipelineId,
    document_id: WrDocumentId,
    epoch: WrEpoch,
}

impl<'a> From<(&'a(WrPipelineId, WrDocumentId), &'a WrEpoch)> for WrPipelineEpoch {
    fn from(tuple: (&(WrPipelineId, WrDocumentId), &WrEpoch)) -> WrPipelineEpoch {
        WrPipelineEpoch {
            pipeline_id: (tuple.0).0,
            document_id: (tuple.0).1,
            epoch: *tuple.1
        }
    }
}

#[repr(C)]
pub struct WrRemovedPipeline {
    pipeline_id: WrPipelineId,
    document_id: WrDocumentId,
}

impl<'a> From<&'a (WrPipelineId, WrDocumentId)> for WrRemovedPipeline {
    fn from(tuple: &(WrPipelineId, WrDocumentId)) -> WrRemovedPipeline {
        WrRemovedPipeline {
            pipeline_id: tuple.0,
            document_id: tuple.1,
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
    removed_pipelines: FfiVec<WrRemovedPipeline>,
}

impl WrPipelineInfo {
    fn new(info: &PipelineInfo) -> Self {
        WrPipelineInfo {
            epochs: FfiVec::from_vec(info.epochs.iter().map(WrPipelineEpoch::from).collect()),
            removed_pipelines: FfiVec::from_vec(info.removed_pipelines.iter()
                                                .map(WrRemovedPipeline::from).collect()),
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wr_renderer_flush_pipeline_info(renderer: &mut Renderer) -> WrPipelineInfo {
    let info = renderer.flush_pipeline_info();
    WrPipelineInfo::new(&info)
}

#[no_mangle]
pub unsafe extern "C" fn wr_pipeline_info_delete(_info: WrPipelineInfo) {
    // _info will be dropped here, and the drop impl on FfiVec will free
    // the underlying vec memory
}

extern "C" {
    pub fn gecko_profiler_start_marker(name: *const c_char);
    pub fn gecko_profiler_end_marker(name: *const c_char);
    pub fn gecko_profiler_add_text_marker(
        name: *const c_char, text_bytes: *const c_char, text_len: usize, microseconds: u64);
    pub fn gecko_profiler_thread_is_being_profiled() -> bool;
}

/// Simple implementation of the WR ProfilerHooks trait to allow profile
/// markers to be seen in the Gecko profiler.
struct GeckoProfilerHooks;

impl ProfilerHooks for GeckoProfilerHooks {
    fn begin_marker(&self, label: &CStr) {
        unsafe {
            gecko_profiler_start_marker(label.as_ptr());
        }
    }

    fn end_marker(&self, label: &CStr) {
        unsafe {
            gecko_profiler_end_marker(label.as_ptr());
        }
    }

    fn add_text_marker(&self, label: &CStr, text: &str, duration: Duration) {
        unsafe {
            // NB: This can be as_micros() once we require Rust 1.33.
            let micros = duration.subsec_micros() as u64 + duration.as_secs() * 1000 * 1000;
            let text_bytes = text.as_bytes();
            gecko_profiler_add_text_marker(label.as_ptr(), text_bytes.as_ptr() as *const c_char, text_bytes.len(), micros);
        }
    }

    fn thread_is_being_profiled(&self) -> bool {
        unsafe { gecko_profiler_thread_is_being_profiled() }
    }
}

static PROFILER_HOOKS: GeckoProfilerHooks = GeckoProfilerHooks {};

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
    fn apz_sample_transforms(window_id: WrWindowId, transaction: &mut Transaction,
                             document_id: WrDocumentId);
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

    fn pre_scene_build(&self) {
        unsafe { gecko_profiler_start_marker(b"SceneBuilding\0".as_ptr() as *const c_char); }
    }

    fn pre_scene_swap(&self, scenebuild_time: u64) {
        unsafe {
            record_telemetry_time(TelemetryProbe::SceneBuildTime, scenebuild_time);
            apz_pre_scene_swap(self.window_id);
        }
    }

    fn post_scene_swap(&self, document_ids: &Vec<DocumentId>, info: PipelineInfo, sceneswap_time: u64) {
        unsafe {
            let info = WrPipelineInfo::new(&info);
            record_telemetry_time(TelemetryProbe::SceneSwapTime, sceneswap_time);
            apz_post_scene_swap(self.window_id, info);
        }
        let info = WrPipelineInfo::new(&info);

        // After a scene swap we should schedule a render for the next vsync,
        // otherwise there's no guarantee that the new scene will get rendered
        // anytime soon
        unsafe { wr_finished_scene_build(self.window_id, document_ids.as_ptr(), document_ids.len(), info) }
        unsafe { gecko_profiler_end_marker(b"SceneBuilding\0".as_ptr() as *const c_char); }
    }

    fn post_resource_update(&self, document_ids: &Vec<DocumentId>) {
        unsafe { wr_schedule_render(self.window_id, document_ids.as_ptr(), document_ids.len()) }
        unsafe { gecko_profiler_end_marker(b"SceneBuilding\0".as_ptr() as *const c_char); }
    }

    fn post_empty_scene_build(&self) {
        unsafe { gecko_profiler_end_marker(b"SceneBuilding\0".as_ptr() as *const c_char); }
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

    fn sample(&self, document_id: DocumentId) -> Vec<FrameMsg> {
        let mut transaction = Transaction::new();
        unsafe { apz_sample_transforms(self.window_id, &mut transaction, document_id) };
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
    fn wr_register_thread_local_arena();
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
    // Clamp the number of workers between 1 and 8. We get diminishing returns
    // with high worker counts and extra overhead because of rayon and font
    // management.
    let num_threads = num_cpus::get().max(2).min(8);

    let worker = rayon::ThreadPoolBuilder::new()
        .thread_name(|idx|{ format!("WRWorker#{}", idx) })
        .num_threads(num_threads)
        .start_handler(|idx| {
            wr_register_thread_local_arena();
            let name = format!("WRWorker#{}", idx);
            register_thread_with_profiler(name.clone());
            gecko_profiler_register_thread(CString::new(name).unwrap().as_ptr());
        })
        .exit_handler(|_idx| {
            gecko_profiler_unregister_thread();
        })
        .build();

    let workers = Arc::new(worker.unwrap());

    // This effectively leaks the thread pool. Not great but we only create one and it lives
    // for as long as the browser.
    // Do this to avoid intermittent race conditions with nsThreadManager shutdown.
    // A better fix would involve removing the dependency between implicit nsThreadManager
    // and webrender's threads, or be able to synchronously terminate rayon's thread pool.
    mem::forget(Arc::clone(&workers));

    Box::into_raw(Box::new(WrThreadPool(workers)))
}

#[no_mangle]
pub unsafe extern "C" fn wr_thread_pool_delete(thread_pool: *mut WrThreadPool) {
    Box::from_raw(thread_pool);
}

#[no_mangle]
pub unsafe extern "C" fn wr_program_cache_new(prof_path: &nsAString, thread_pool: *mut WrThreadPool) -> *mut WrProgramCache {
    let workers = &(*thread_pool).0;
    let program_cache = WrProgramCache::new(prof_path, workers);
    Box::into_raw(Box::new(program_cache))
}

#[no_mangle]
pub unsafe extern "C" fn wr_program_cache_delete(program_cache: *mut WrProgramCache) {
    Box::from_raw(program_cache);
}

#[no_mangle]
pub unsafe extern "C" fn wr_try_load_startup_shaders_from_disk(program_cache: *mut WrProgramCache) {
    (*program_cache).try_load_startup_shaders_from_disk();
}

#[no_mangle]
pub unsafe extern "C" fn remove_program_binary_disk_cache(prof_path: &nsAString) -> bool {
    match remove_disk_cache(prof_path) {
        Ok(_) => true,
        Err(_) => {
            error!("Failed to remove program binary disk cache");
            false
        }
    }
}

#[no_mangle]
pub extern "C" fn wr_renderer_update_program_cache(renderer: &mut Renderer, program_cache: &mut WrProgramCache) {
    let program_cache = Rc::clone(&program_cache.rc_get());
    renderer.update_program_cache(program_cache);
}

// This matches IsEnvSet in gfxEnv.h
fn env_var_to_bool(key: &'static str) -> bool {
    env::var(key).ok().map_or(false, |v| !v.is_empty())
}

// Call MakeCurrent before this.
fn wr_device_new(gl_context: *mut c_void, pc: Option<&mut WrProgramCache>)
    -> Device
{
    assert!(unsafe { is_in_render_thread() });

    let gl;
    if unsafe { is_glcontext_gles(gl_context) } {
        gl = unsafe { gl::GlesFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    } else {
        gl = unsafe { gl::GlFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    }

    let version = gl.get_string(gl::VERSION);

    info!("WebRender - OpenGL version new {}", version);

    let upload_method = if unsafe { is_glcontext_angle(gl_context) } {
        UploadMethod::Immediate
    } else {
        UploadMethod::PixelBuffer(VertexUsageHint::Dynamic)
    };

    let resource_override_path = unsafe {
        let override_charptr = gfx_wr_resource_path_override();
        if override_charptr.is_null() {
            None
        } else {
            match CStr::from_ptr(override_charptr).to_str() {
                Ok(override_str) => Some(PathBuf::from(override_str)),
                _ => None
            }
        }
    };

    let cached_programs = match pc {
      Some(cached_programs) => Some(Rc::clone(cached_programs.rc_get())),
      None => None,
    };

    Device::new(gl, resource_override_path, upload_method, cached_programs, false, true, true, None)
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern "C" fn wr_window_new(window_id: WrWindowId,
                                window_width: i32,
                                window_height: i32,
                                support_low_priority_transactions: bool,
                                allow_texture_swizzling: bool,
                                enable_picture_caching: bool,
                                start_debug_server: bool,
                                gl_context: *mut c_void,
                                program_cache: Option<&mut WrProgramCache>,
                                shaders: Option<&mut WrShaders>,
                                thread_pool: *mut WrThreadPool,
                                size_of_op: VoidPtrToSizeFn,
                                enclosing_size_of_op: VoidPtrToSizeFn,
                                document_id: u32,
                                out_handle: &mut *mut DocumentHandle,
                                out_renderer: &mut *mut Renderer,
                                out_max_texture_size: *mut i32)
                                -> bool {
    assert!(unsafe { is_in_render_thread() });

    let recorder: Option<Box<dyn ApiRecordingReceiver>> = if unsafe { gfx_use_wrench() } {
        let name = format!("wr-record-{}.bin", window_id.0);
        Some(Box::new(BinaryRecorder::new(&PathBuf::from(name))))
    } else {
        None
    };

    let gl;
    if unsafe { is_glcontext_gles(gl_context) } {
        gl = unsafe { gl::GlesFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    } else {
        gl = unsafe { gl::GlFns::load_with(|symbol| get_proc_address(gl_context, symbol)) };
    }

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

    let precache_flags = if env_var_to_bool("MOZ_WR_PRECACHE_SHADERS") {
        ShaderPrecacheFlags::FULL_COMPILE
    } else {
        ShaderPrecacheFlags::empty()
    };

    let cached_programs = match program_cache {
        Some(program_cache) => Some(Rc::clone(&program_cache.rc_get())),
        None => None,
    };

    let color = if cfg!(target_os = "android") {
        // The color is for avoiding black flash before receiving display list.
        ColorF::new(1.0, 1.0, 1.0, 1.0)
    } else {
        ColorF::new(0.0, 0.0, 0.0, 0.0)
    };

    let opts = RendererOptions {
        enable_aa: true,
        enable_subpixel_aa: cfg!(not(target_os = "android")),
        support_low_priority_transactions,
        allow_texture_swizzling,
        recorder: recorder,
        blob_image_handler: Some(Box::new(Moz2dBlobImageHandler::new(workers.clone()))),
        workers: Some(workers.clone()),
        thread_listener: Some(Box::new(GeckoProfilerThreadListener::new())),
        size_of_op: Some(size_of_op),
        enclosing_size_of_op: Some(enclosing_size_of_op),
        cached_programs,
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
        max_texture_size: Some(8192), // Moz2D doesn't like textures bigger than this
        clear_color: Some(color),
        precache_flags,
        namespace_alloc_by_client: true,
        enable_picture_caching,
        allow_pixel_local_storage_support: false,
        start_debug_server,
        ..Default::default()
    };

    // Ensure the WR profiler callbacks are hooked up to the Gecko profiler.
    set_profiler_hooks(Some(&PROFILER_HOOKS));

    let window_size = DeviceIntSize::new(window_width, window_height);
    let notifier = Box::new(CppNotifier {
        window_id: window_id,
    });
    let (renderer, sender) = match Renderer::new(gl, notifier, opts, shaders, window_size) {
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
    let layer = 0;
    *out_handle = Box::into_raw(Box::new(
            DocumentHandle::new_with_id(sender.create_api_by_client(next_namespace_id()),
                                        window_size, layer, document_id)));
    *out_renderer = Box::into_raw(Box::new(renderer));

    return true;
}

#[no_mangle]
pub extern "C" fn wr_api_create_document(
    root_dh: &mut DocumentHandle,
    out_handle: &mut *mut DocumentHandle,
    doc_size: DeviceIntSize,
    layer: i8,
    document_id: u32
) {
    assert!(unsafe { is_in_compositor_thread() });

    *out_handle = Box::into_raw(Box::new(DocumentHandle::new_with_id(
        root_dh.api.clone_sender().create_api_by_client(next_namespace_id()),
        doc_size,
        layer,
        document_id
    )));
}

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
        api: dh.api.clone_sender().create_api_by_client(next_namespace_id()),
        document_id: dh.document_id,
    };
    *out_handle = Box::into_raw(Box::new(handle));
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_delete(dh: *mut DocumentHandle) {
    let _ = Box::from_raw(dh);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_shut_down(dh: &mut DocumentHandle) {
    dh.api.shut_down(true);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_notify_memory_pressure(dh: &mut DocumentHandle) {
    dh.api.notify_memory_pressure();
}

#[no_mangle]
pub extern "C" fn wr_api_set_debug_flags(dh: &mut DocumentHandle, flags: DebugFlags) {
    dh.api.set_debug_flags(flags);
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_accumulate_memory_report(
    dh: &mut DocumentHandle,
    report: &mut MemoryReport
) {
    *report += dh.api.report_memory();
}

#[no_mangle]
pub unsafe extern "C" fn wr_api_clear_all_caches(dh: &mut DocumentHandle) {
    dh.api.send_debug_cmd(DebugCommand::ClearCaches(ClearCache::all()));
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

#[no_mangle]
pub extern "C" fn wr_transaction_delete(txn: *mut Transaction) {
    unsafe { let _ = Box::from_raw(txn); }
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_low_priority(txn: &mut Transaction, low_priority: bool) {
    txn.set_low_priority(low_priority);
}

#[no_mangle]
pub extern "C" fn wr_transaction_is_empty(txn: &Transaction) -> bool {
    txn.is_empty()
}

#[no_mangle]
pub extern "C" fn wr_transaction_resource_updates_is_empty(txn: &Transaction) -> bool {
    txn.resource_updates.is_empty()
}

#[no_mangle]
pub extern "C" fn wr_transaction_is_rendered_frame_invalidated(txn: &Transaction) -> bool {
    txn.invalidate_rendered_frame
}

#[no_mangle]
pub extern "C" fn wr_transaction_notify(txn: &mut Transaction, when: Checkpoint, event: usize) {
    struct GeckoNotification(usize);
    impl NotificationHandler for GeckoNotification {
        fn notify(&self, when: Checkpoint) {
            unsafe {
                wr_transaction_notification_notified(self.0, when);
            }
        }
    }

    let handler = Box::new(GeckoNotification(event));
    txn.notify(NotificationRequest::new(when, handler));
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
    viewport_size: LayoutSize,
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
        viewport_size,
        (pipeline_id, content_size, dl),
        preserve_frame_state,
    );
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_document_view(
    txn: &mut Transaction,
    doc_rect: &DeviceIntRect,
) {
    txn.set_document_view(
        *doc_rect,
        1.0,
    );
}

#[no_mangle]
pub extern "C" fn wr_transaction_generate_frame(
    txn: &mut Transaction) {
    txn.generate_frame();
}

#[no_mangle]
pub extern "C" fn wr_transaction_invalidate_rendered_frame(txn: &mut Transaction) {
    txn.invalidate_rendered_frame();
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
        let transform_slice = unsafe { make_slice(transform_array, transform_count) };

        properties.transforms.reserve(transform_slice.len());
        for element in transform_slice.iter() {
            let prop = PropertyValue {
                key: PropertyBindingKey::new(element.id),
                value: element.transform.into(),
            };

            properties.transforms.push(prop);
        }
    }

    if opacity_count > 0 {
        let opacity_slice = unsafe { make_slice(opacity_array, opacity_count) };

        properties.floats.reserve(opacity_slice.len());
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

    let transform_slice = unsafe { make_slice(transform_array, transform_count) };
    properties.transforms.reserve(transform_slice.len());
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
pub extern "C" fn wr_transaction_pinch_zoom(
    txn: &mut Transaction,
    pinch_zoom: f32
) {
    txn.set_pinch_zoom(ZoomFactor::new(pinch_zoom));
}

#[no_mangle]
pub extern "C" fn wr_transaction_set_is_transform_pinch_zooming(
    txn: &mut Transaction,
    animation_id: u64,
    is_zooming: bool
) {
    txn.set_is_transform_pinch_zooming(is_zooming, PropertyBindingId::new(animation_id));
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_image(
    txn: &mut Transaction,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
) {
    txn.add_image(
        image_key,
        descriptor.into(),
        ImageData::new(bytes.flush_into_vec()),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_blob_image(
    txn: &mut Transaction,
    image_key: BlobImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
    visible_rect: DeviceIntRect,
) {
    txn.add_blob_image(
        image_key,
        descriptor.into(),
        Arc::new(bytes.flush_into_vec()),
        visible_rect,
        if descriptor.format == ImageFormat::BGRA8 { Some(256) } else { None }
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_external_image(
    txn: &mut Transaction,
    image_key: WrImageKey,
    descriptor: &WrImageDescriptor,
    external_image_id: WrExternalImageId,
    image_type: &ExternalImageType,
    channel_index: u8
) {
    txn.add_image(
        image_key,
        descriptor.into(),
        ImageData::External(
            ExternalImageData {
                id: external_image_id.into(),
                channel_index: channel_index,
                image_type: *image_type,
            }
        ),
        None
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_image(
    txn: &mut Transaction,
    key: WrImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
) {
    txn.update_image(
        key,
        descriptor.into(),
        ImageData::new(bytes.flush_into_vec()),
        &DirtyRect::All,
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_set_blob_image_visible_area(
    txn: &mut Transaction,
    key: BlobImageKey,
    area: &DeviceIntRect,
) {
    txn.set_blob_image_visible_area(key, *area);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_external_image(
    txn: &mut Transaction,
    key: WrImageKey,
    descriptor: &WrImageDescriptor,
    external_image_id: WrExternalImageId,
    image_type: &ExternalImageType,
    channel_index: u8
) {
    txn.update_image(
        key,
        descriptor.into(),
        ImageData::External(
            ExternalImageData {
                id: external_image_id.into(),
                channel_index,
                image_type: *image_type,
            }
        ),
        &DirtyRect::All,
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_external_image_with_dirty_rect(
    txn: &mut Transaction,
    key: WrImageKey,
    descriptor: &WrImageDescriptor,
    external_image_id: WrExternalImageId,
    image_type: &ExternalImageType,
    channel_index: u8,
    dirty_rect: DeviceIntRect,
) {
    txn.update_image(
        key,
        descriptor.into(),
        ImageData::External(
            ExternalImageData {
                id: external_image_id.into(),
                channel_index,
                image_type: *image_type,
            }
        ),
        &DirtyRect::Partial(dirty_rect)
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_update_blob_image(
    txn: &mut Transaction,
    image_key: BlobImageKey,
    descriptor: &WrImageDescriptor,
    bytes: &mut WrVecU8,
    visible_rect: DeviceIntRect,
    dirty_rect: LayoutIntRect,
) {
    txn.update_blob_image(
        image_key,
        descriptor.into(),
        Arc::new(bytes.flush_into_vec()),
        visible_rect,
        &DirtyRect::Partial(dirty_rect)
    );
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_image(
    txn: &mut Transaction,
    key: WrImageKey
) {
    txn.delete_image(key);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_blob_image(
    txn: &mut Transaction,
    key: BlobImageKey
) {
    txn.delete_blob_image(key);
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
pub unsafe extern "C" fn wr_api_send_transactions(
    document_handles: *const *const DocumentHandle,
    transactions: *const *mut Transaction,
    transaction_count: usize,
    is_async: bool
) {
    if transaction_count == 0 {
        return;
    }
    let mut out_transactions = Vec::with_capacity(transaction_count);
    let mut out_documents = Vec::with_capacity(transaction_count);
    for i in 0..transaction_count {
        let txn = &mut **transactions.offset(i as isize);
        debug_assert!(!txn.is_empty());
        let new_txn = make_transaction(is_async);
        out_transactions.push(mem::replace(txn, new_txn));
        out_documents.push((**document_handles.offset(i as isize)).document_id);
    }
    (**document_handles).api.send_transactions(
        out_documents,
        out_transactions);
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

#[no_mangle]
pub extern "C" fn wr_api_send_external_event(dh: &mut DocumentHandle,
                                             evt: usize) {
    assert!(unsafe { !is_in_render_thread() });

    dh.api.send_external_event(ExternalEvent::from_raw(evt));
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_raw_font(
    txn: &mut Transaction,
    key: WrFontKey,
    bytes: &mut WrVecU8,
    index: u32
) {
    txn.add_raw_font(key, bytes.flush_into_vec(), index);
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
    let mut path = PathBuf::from(&*cstr.to_string_lossy());

    #[cfg(target_os = "android")]
    {
        // On Android we need to write into a particular folder on external
        // storage so that (a) it can be written without requiring permissions
        // and (b) it can be pulled off via `adb pull`. This env var is set
        // in GeckoLoader.java.
        if let Ok(storage_path) = env::var("PUBLIC_STORAGE") {
            path = PathBuf::from(storage_path).join(path);
        }
    }

    // Increment the extension until we find a fresh path
    while path.is_dir() {
        let count: u32 = path.extension()
            .and_then(|x| x.to_str())
            .and_then(|x| x.parse().ok())
            .unwrap_or(0);
        path.set_extension((count + 1).to_string());
    }

    // Use warn! so that it gets emitted to logcat on android as well
    let border = "--------------------------\n";
    warn!("{} Capturing WR state to: {:?}\n{}", &border, &path, &border);

    let _ = create_dir_all(&path);
    match File::create(path.join("wr.txt")) {
        Ok(mut file) => {
            // The Gecko HG revision is available at compile time
            if let Some(moz_revision) = option_env!("GECKO_HEAD_REV") {
                writeln!(file, "mozilla-central {}", moz_revision).unwrap();
            }
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
    NativeFontHandle {
        path: PathBuf::from(OsString::from_wide(&wchars)),
        index,
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
    let chars = bytes.flush_into_vec();
    NativeFontHandle {
        path: PathBuf::from(OsString::from_vec(chars)),
        index,
    }
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_font_descriptor(
    txn: &mut Transaction,
    key: WrFontKey,
    bytes: &mut WrVecU8,
    index: u32
) {
    let native_font_handle = read_font_descriptor(bytes, index);
    txn.add_native_font(key, native_font_handle);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_delete_font(
    txn: &mut Transaction,
    key: WrFontKey
) {
    txn.delete_font(key);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_add_font_instance(
    txn: &mut Transaction,
    key: WrFontInstanceKey,
    font_key: WrFontKey,
    glyph_size: f32,
    options: *const FontInstanceOptions,
    platform_options: *const FontInstancePlatformOptions,
    variations: &mut WrVecU8,
) {
    txn.add_font_instance(
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
    txn: &mut Transaction,
    key: WrFontInstanceKey
) {
    txn.delete_font_instance(key);
}

#[no_mangle]
pub extern "C" fn wr_resource_updates_clear(txn: &mut Transaction) {
    txn.resource_updates.clear();
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

#[repr(u8)]
#[derive(PartialEq, Eq, Debug)]
pub enum WrReferenceFrameKind {
    Transform,
    Perspective,
}

/// IMPORTANT: If you add fields to this struct, you need to also add initializers
/// for those fields in WebRenderAPI.h.
#[repr(C)]
pub struct WrStackingContextParams {
    pub clip: WrStackingContextClip,
    pub animation: *const WrAnimationProperty,
    pub opacity: *const f32,
    pub transform_style: TransformStyle,
    pub reference_frame_kind: WrReferenceFrameKind,
    pub scrolling_relative_to: *const u64,
    pub is_backface_visible: bool,
    /// True if picture caching should be enabled for this stacking context.
    pub cache_tiles: bool,
    pub mix_blend_mode: MixBlendMode,
    /// True if this stacking context is a backdrop root.
    /// https://drafts.fxtf.org/filter-effects-2/#BackdropRoot
    pub is_backdrop_root: bool,
}

#[no_mangle]
pub extern "C" fn wr_dp_push_stacking_context(
    state: &mut WrState,
    mut bounds: LayoutRect,
    spatial_id: WrSpatialId,
    params: &WrStackingContextParams,
    transform: *const LayoutTransform,
    filters: *const FilterOp,
    filter_count: usize,
    filter_datas: *const WrFilterData,
    filter_datas_count: usize,
    glyph_raster_space: RasterSpace,
) -> WrSpatialId {
    debug_assert!(unsafe { !is_in_render_thread() });

    let c_filters = unsafe { make_slice(filters, filter_count) };
    let mut filters : Vec<FilterOp> = c_filters.iter()
        .map(|c_filter| { c_filter.clone() })
        .collect();

    let c_filter_datas = unsafe { make_slice(filter_datas, filter_datas_count) };
    let r_filter_datas : Vec<FilterData> = c_filter_datas.iter().map(|c_filter_data| {
        FilterData {
            func_r_type: c_filter_data.funcR_type,
            r_values: unsafe { make_slice(c_filter_data.R_values, c_filter_data.R_values_count).to_vec() },
            func_g_type: c_filter_data.funcG_type,
            g_values: unsafe { make_slice(c_filter_data.G_values, c_filter_data.G_values_count).to_vec() },
            func_b_type: c_filter_data.funcB_type,
            b_values: unsafe { make_slice(c_filter_data.B_values, c_filter_data.B_values_count).to_vec() },
            func_a_type: c_filter_data.funcA_type,
            a_values: unsafe { make_slice(c_filter_data.A_values, c_filter_data.A_values_count).to_vec() },
        }
    }).collect();

    let transform_ref = unsafe { transform.as_ref() };
    let mut transform_binding = match transform_ref {
        Some(t) => Some(PropertyBinding::Value(t.clone())),
        None => None,
    };

    let opacity_ref = unsafe { params.opacity.as_ref() };
    let mut has_opacity_animation = false;
    let anim = unsafe { params.animation.as_ref() };
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

    let mut wr_spatial_id = spatial_id.to_webrender(state.pipeline_id);
    let wr_clip_id = params.clip.to_webrender(state.pipeline_id);

    // Note: 0 has special meaning in WR land, standing for ROOT_REFERENCE_FRAME.
    // However, it is never returned by `push_reference_frame`, and we need to return
    // an option here across FFI, so we take that 0 value for the None semantics.
    // This is resolved into proper `Maybe<WrSpatialId>` inside `WebRenderAPI::PushStackingContext`.
    let mut result = WrSpatialId { id: 0 };
    if let Some(transform_binding) = transform_binding {
        let scrolling_relative_to = match unsafe { params.scrolling_relative_to.as_ref() } {
            Some(scroll_id) => {
                debug_assert_eq!(params.reference_frame_kind, WrReferenceFrameKind::Perspective);
                Some(ExternalScrollId(*scroll_id, state.pipeline_id))
            }
            None => None,
        };

        let reference_frame_kind = match params.reference_frame_kind {
            WrReferenceFrameKind::Transform => ReferenceFrameKind::Transform,
            WrReferenceFrameKind::Perspective => ReferenceFrameKind::Perspective {
                scrolling_relative_to,
            },
        };
        wr_spatial_id = state.frame_builder.dl_builder.push_reference_frame(
            bounds.origin,
            wr_spatial_id,
            params.transform_style,
            transform_binding,
            reference_frame_kind,
        );

        bounds.origin = LayoutPoint::zero();
        result.id = wr_spatial_id.0;
        assert_ne!(wr_spatial_id.0, 0);
    }

    state.frame_builder
         .dl_builder
         .push_stacking_context(bounds.origin,
                                wr_spatial_id,
                                params.is_backface_visible,
                                wr_clip_id,
                                params.transform_style,
                                params.mix_blend_mode,
                                &filters,
                                &r_filter_datas,
                                &[],
                                glyph_raster_space,
                                params.cache_tiles,
                                params.is_backdrop_root);

    result
}

#[no_mangle]
pub extern "C" fn wr_dp_pop_stacking_context(state: &mut WrState,
                                             is_reference_frame: bool) {
    debug_assert!(unsafe { !is_in_render_thread() });
    state.frame_builder.dl_builder.pop_stacking_context();
    if is_reference_frame {
        state.frame_builder.dl_builder.pop_reference_frame();
    }
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clipchain(state: &mut WrState,
                                         parent_clipchain_id: *const u64,
                                         clips: *const WrClipId,
                                         clips_count: usize)
                                         -> u64 {
    debug_assert!(unsafe { is_in_main_thread() });
    let parent = unsafe { parent_clipchain_id.as_ref() }
        .map(|id| ClipChainId(*id, state.pipeline_id));

    let pipeline_id = state.pipeline_id;
    let clips = unsafe { make_slice(clips, clips_count) }
        .iter()
        .map(|clip_id| clip_id.to_webrender(pipeline_id));

    let clipchain_id = state.frame_builder.dl_builder.define_clip_chain(parent, clips);
    assert!(clipchain_id.1 == state.pipeline_id);
    clipchain_id.0
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clip_with_parent_clip(
    state: &mut WrState,
    parent: &WrSpaceAndClip,
    clip_rect: LayoutRect,
    complex: *const ComplexClipRegion,
    complex_count: usize,
    mask: *const ImageMask,
) -> WrClipId {
    wr_dp_define_clip_impl(
        &mut state.frame_builder,
        parent.to_webrender(state.pipeline_id),
        clip_rect,
        unsafe { make_slice(complex, complex_count) },
        unsafe { mask.as_ref() }.map(|m| *m),
    )
}

#[no_mangle]
pub extern "C" fn wr_dp_define_clip_with_parent_clip_chain(
    state: &mut WrState,
    parent: &WrSpaceAndClipChain,
    clip_rect: LayoutRect,
    complex: *const ComplexClipRegion,
    complex_count: usize,
    mask: *const ImageMask,
) -> WrClipId {
    wr_dp_define_clip_impl(
        &mut state.frame_builder,
        parent.to_webrender(state.pipeline_id),
        clip_rect,
        unsafe { make_slice(complex, complex_count) },
        unsafe { mask.as_ref() }.map(|m| *m),
    )
}

fn wr_dp_define_clip_impl(
    frame_builder: &mut WebRenderFrameBuilder,
    parent: SpaceAndClipInfo,
    clip_rect: LayoutRect,
    complex_regions: &[ComplexClipRegion],
    mask: Option<ImageMask>,
) -> WrClipId {
    debug_assert!(unsafe { is_in_main_thread() });
    let clip_id = frame_builder.dl_builder.define_clip(
        &parent,
        clip_rect,
        complex_regions.iter().cloned(),
        mask,
    );
    WrClipId::from_webrender(clip_id)
}

#[no_mangle]
pub extern "C" fn wr_dp_define_sticky_frame(state: &mut WrState,
                                            parent_spatial_id: WrSpatialId,
                                            content_rect: LayoutRect,
                                            top_margin: *const f32,
                                            right_margin: *const f32,
                                            bottom_margin: *const f32,
                                            left_margin: *const f32,
                                            vertical_bounds: StickyOffsetBounds,
                                            horizontal_bounds: StickyOffsetBounds,
                                            applied_offset: LayoutVector2D)
                                            -> WrSpatialId {
    assert!(unsafe { is_in_main_thread() });
    let spatial_id = state.frame_builder.dl_builder.define_sticky_frame(
        parent_spatial_id.to_webrender(state.pipeline_id),
        content_rect,
        SideOffsets2D::new(
            unsafe { top_margin.as_ref() }.cloned(),
            unsafe { right_margin.as_ref() }.cloned(),
            unsafe { bottom_margin.as_ref() }.cloned(),
            unsafe { left_margin.as_ref() }.cloned()
        ),
        vertical_bounds,
        horizontal_bounds,
        applied_offset,
    );

    WrSpatialId { id: spatial_id.0 }
}

#[no_mangle]
pub extern "C" fn wr_dp_define_scroll_layer(state: &mut WrState,
                                            external_scroll_id: u64,
                                            parent: &WrSpaceAndClip,
                                            content_rect: LayoutRect,
                                            clip_rect: LayoutRect,
                                            scroll_offset: LayoutPoint)
                                            -> WrSpaceAndClip {
    assert!(unsafe { is_in_main_thread() });

    let space_and_clip = state.frame_builder.dl_builder.define_scroll_frame(
        &parent.to_webrender(state.pipeline_id),
        Some(ExternalScrollId(external_scroll_id, state.pipeline_id)),
        content_rect,
        clip_rect,
        vec![],
        None,
        ScrollSensitivity::Script,
        // TODO(gw): We should also update the Gecko-side APIs to provide
        //           this as a vector rather than a point.
        scroll_offset.to_vector(),
    );

    WrSpaceAndClip::from_webrender(space_and_clip)
}

#[no_mangle]
pub extern "C" fn wr_dp_push_iframe(state: &mut WrState,
                                    rect: LayoutRect,
                                    clip: LayoutRect,
                                    _is_backface_visible: bool,
                                    parent: &WrSpaceAndClipChain,
                                    pipeline_id: WrPipelineId,
                                    ignore_missing_pipeline: bool) {
    debug_assert!(unsafe { is_in_main_thread() });

    state.frame_builder.dl_builder.push_iframe(
        rect,
        clip,
        &parent.to_webrender(state.pipeline_id),
        pipeline_id,
        ignore_missing_pipeline,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_rect(state: &mut WrState,
                                  rect: LayoutRect,
                                  clip: LayoutRect,
                                  is_backface_visible: bool,
                                  parent: &WrSpaceAndClipChain,
                                  color: ColorF) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);
    let clip_rect = clip.intersection(&rect);

    let prim_info = CommonItemProperties {
        // NB: the damp-e10s talos-test will frequently crash on startup if we
        // early-return here for empty rects. I couldn't figure out why, but
        // it's pretty harmless to feed these through, so, uh, we do?
        clip_rect: clip_rect.unwrap_or(LayoutRect::zero()),
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_rect(
        &prim_info,
        color,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_rect_with_parent_clip(
    state: &mut WrState,
    rect: LayoutRect,
    clip: LayoutRect,
    is_backface_visible: bool,
    parent: &WrSpaceAndClip,
    color: ColorF,
) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let clip_rect = clip.intersection(&rect);
    if clip_rect.is_none() { return; }

    let prim_info = CommonItemProperties {
        clip_rect: clip_rect.unwrap(),
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_rect(
        &prim_info,
        color,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clear_rect(state: &mut WrState,
                                        rect: LayoutRect,
                                        clip: LayoutRect,
                                        parent: &WrSpaceAndClipChain) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let clip_rect = clip.intersection(&rect);
    if clip_rect.is_none() { return; }

    let prim_info = CommonItemProperties {
        clip_rect: clip_rect.unwrap(),
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible: true,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_clear_rect(
        &prim_info,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_hit_test(state: &mut WrState,
                                      rect: LayoutRect,
                                      clip: LayoutRect,
                                      is_backface_visible: bool,
                                      parent: &WrSpaceAndClipChain) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let clip_rect = clip.intersection(&rect);
    if clip_rect.is_none() { return; }

    let prim_info = CommonItemProperties {
        clip_rect: clip_rect.unwrap(),
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_hit_test(
        &prim_info,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_clear_rect_with_parent_clip(
    state: &mut WrState,
    rect: LayoutRect,
    clip: LayoutRect,
    parent: &WrSpaceAndClip,
) {
    debug_assert!(unsafe { !is_in_render_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let clip_rect = clip.intersection(&rect);
    if clip_rect.is_none() { return; }

    let prim_info = CommonItemProperties {
        clip_rect: clip_rect.unwrap(),
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible: true,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_clear_rect(
        &prim_info,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_image(state: &mut WrState,
                                   bounds: LayoutRect,
                                   clip: LayoutRect,
                                   is_backface_visible: bool,
                                   parent: &WrSpaceAndClipChain,
                                   stretch_size: LayoutSize,
                                   tile_spacing: LayoutSize,
                                   image_rendering: ImageRendering,
                                   key: WrImageKey,
                                   premultiplied_alpha: bool,
                                   color: ColorF) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    let alpha_type = if premultiplied_alpha {
        AlphaType::PremultipliedAlpha
    } else {
        AlphaType::Alpha
    };

    state.frame_builder
         .dl_builder
         .push_image(&prim_info,
                     bounds,
                     stretch_size,
                     tile_spacing,
                     image_rendering,
                     alpha_type,
                     key,
                     color);
}

/// Push a 3 planar yuv image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_planar_image(state: &mut WrState,
                                              bounds: LayoutRect,
                                              clip: LayoutRect,
                                              is_backface_visible: bool,
                                              parent: &WrSpaceAndClipChain,
                                              image_key_0: WrImageKey,
                                              image_key_1: WrImageKey,
                                              image_key_2: WrImageKey,
                                              color_depth: WrColorDepth,
                                              color_space: WrYuvColorSpace,
                                              color_range: WrColorRange,
                                              image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         bounds,
                         YuvData::PlanarYCbCr(image_key_0, image_key_1, image_key_2),
                         color_depth,
                         color_space,
                         color_range,
                         image_rendering);
}

/// Push a 2 planar NV12 image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_NV12_image(state: &mut WrState,
                                            bounds: LayoutRect,
                                            clip: LayoutRect,
                                            is_backface_visible: bool,
                                            parent: &WrSpaceAndClipChain,
                                            image_key_0: WrImageKey,
                                            image_key_1: WrImageKey,
                                            color_depth: WrColorDepth,
                                            color_space: WrYuvColorSpace,
                                            color_range: WrColorRange,
                                            image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         bounds,
                         YuvData::NV12(image_key_0, image_key_1),
                         color_depth,
                         color_space,
                         color_range,
                         image_rendering);
}

/// Push a yuv interleaved image.
#[no_mangle]
pub extern "C" fn wr_dp_push_yuv_interleaved_image(state: &mut WrState,
                                                   bounds: LayoutRect,
                                                   clip: LayoutRect,
                                                   is_backface_visible: bool,
                                                   parent: &WrSpaceAndClipChain,
                                                   image_key_0: WrImageKey,
                                                   color_depth: WrColorDepth,
                                                   color_space: WrYuvColorSpace,
                                                   color_range: WrColorRange,
                                                   image_rendering: ImageRendering) {
    debug_assert!(unsafe { is_in_main_thread() || is_in_compositor_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder
         .dl_builder
         .push_yuv_image(&prim_info,
                         bounds,
                         YuvData::InterleavedYCbCr(image_key_0),
                         color_depth,
                         color_space,
                         color_range,
                         image_rendering);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_text(state: &mut WrState,
                                  bounds: LayoutRect,
                                  clip: LayoutRect,
                                  is_backface_visible: bool,
                                  parent: &WrSpaceAndClipChain,
                                  color: ColorF,
                                  font_key: WrFontInstanceKey,
                                  glyphs: *const GlyphInstance,
                                  glyph_count: u32,
                                  glyph_options: *const GlyphOptions) {
    debug_assert!(unsafe { is_in_main_thread() });

    let glyph_slice = unsafe { make_slice(glyphs, glyph_count as usize) };

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        spatial_id: space_and_clip.spatial_id,
        clip_id: space_and_clip.clip_id,
        is_backface_visible,
        hit_info: state.current_tag
    };

    state.frame_builder
         .dl_builder
         .push_text(&prim_info,
                    bounds,
                    &glyph_slice,
                    font_key,
                    color,
                    unsafe { glyph_options.as_ref().cloned() });
}

#[no_mangle]
pub extern "C" fn wr_dp_push_shadow(state: &mut WrState,
                                    _bounds: LayoutRect,
                                    _clip: LayoutRect,
                                    _is_backface_visible: bool,
                                    parent: &WrSpaceAndClipChain,
                                    shadow: Shadow,
                                    should_inflate: bool) {
    debug_assert!(unsafe { is_in_main_thread() });

    state.frame_builder.dl_builder.push_shadow(
        &parent.to_webrender(state.pipeline_id),
        shadow.into(),
        should_inflate,
    );
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
                                  parent: &WrSpaceAndClipChain,
                                  bounds: &LayoutRect,
                                  wavy_line_thickness: f32,
                                  orientation: LineOrientation,
                                  color: &ColorF,
                                  style: LineStyle) {
    debug_assert!(unsafe { is_in_main_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: *clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder
         .dl_builder
         .push_line(&prim_info,
                    bounds,
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
                                    parent: &WrSpaceAndClipChain,
                                    do_aa: AntialiasBorder,
                                    widths: LayoutSideOffsets,
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
        do_aa: do_aa == AntialiasBorder::Yes,
    });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder
         .dl_builder
         .push_border(&prim_info,
                      rect,
                      widths,
                      border_details);
}

#[repr(C)]
pub struct WrBorderImage {
    widths: LayoutSideOffsets,
    image: WrImageKey,
    width: i32,
    height: i32,
    fill: bool,
    slice: DeviceIntSideOffsets,
    outset: LayoutSideOffsets,
    repeat_horizontal: RepeatMode,
    repeat_vertical: RepeatMode,
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_image(state: &mut WrState,
                                          rect: LayoutRect,
                                          clip: LayoutRect,
                                          is_backface_visible: bool,
                                          parent: &WrSpaceAndClipChain,
                                          params: &WrBorderImage) {
    debug_assert!(unsafe { is_in_main_thread() });
    let border_details = BorderDetails::NinePatch(NinePatchBorder {
        source: NinePatchBorderSource::Image(params.image),
        width: params.width,
        height: params.height,
        slice: params.slice,
        fill: params.fill,
        outset: params.outset,
        repeat_horizontal: params.repeat_horizontal,
        repeat_vertical: params.repeat_vertical,
    });
    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_border(
        &prim_info,
        rect,
        params.widths,
        border_details,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             parent: &WrSpaceAndClipChain,
                                             widths: LayoutSideOffsets,
                                             width: i32,
                                             height: i32,
                                             fill: bool,
                                             slice: DeviceIntSideOffsets,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             outset: LayoutSideOffsets) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = unsafe { make_slice(stops, stops_count) };
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder.dl_builder.create_gradient(
        start_point.into(),
        end_point.into(),
        stops_vector,
        extend_mode.into()
    );

    let border_details = BorderDetails::NinePatch(NinePatchBorder {
        source: NinePatchBorderSource::Gradient(gradient),
        width,
        height,
        slice,
        fill,
        outset: outset.into(),
        repeat_horizontal: RepeatMode::Stretch,
        repeat_vertical: RepeatMode::Stretch,
    });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_border(
        &prim_info,
        rect,
        widths.into(),
        border_details,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_border_radial_gradient(state: &mut WrState,
                                                    rect: LayoutRect,
                                                    clip: LayoutRect,
                                                    is_backface_visible: bool,
                                                    parent: &WrSpaceAndClipChain,
                                                    widths: LayoutSideOffsets,
                                                    fill: bool,
                                                    center: LayoutPoint,
                                                    radius: LayoutSize,
                                                    stops: *const GradientStop,
                                                    stops_count: usize,
                                                    extend_mode: ExtendMode,
                                                    outset: LayoutSideOffsets) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = unsafe { make_slice(stops, stops_count) };
    let stops_vector = stops_slice.to_owned();

    let slice = SideOffsets2D::new(
        widths.top as i32,
        widths.right as i32,
        widths.bottom as i32,
        widths.left as i32,
    );

    let gradient = state.frame_builder.dl_builder.create_radial_gradient(
        center.into(),
        radius.into(),
        stops_vector,
        extend_mode.into()
    );

    let border_details = BorderDetails::NinePatch(NinePatchBorder {
        source: NinePatchBorderSource::RadialGradient(gradient),
        width: rect.size.width as i32,
        height: rect.size.height as i32,
        slice,
        fill,
        outset: outset.into(),
        repeat_horizontal: RepeatMode::Stretch,
        repeat_vertical: RepeatMode::Stretch,
    });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_border(
        &prim_info,
        rect,
        widths.into(),
        border_details,
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_linear_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             parent: &WrSpaceAndClipChain,
                                             start_point: LayoutPoint,
                                             end_point: LayoutPoint,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = unsafe { make_slice(stops, stops_count) };
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_gradient(start_point.into(),
                                         end_point.into(),
                                         stops_vector,
                                         extend_mode.into());

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_gradient(
        &prim_info,
        rect,
        gradient,
        tile_size.into(),
        tile_spacing.into(),
    );
}

#[no_mangle]
pub extern "C" fn wr_dp_push_radial_gradient(state: &mut WrState,
                                             rect: LayoutRect,
                                             clip: LayoutRect,
                                             is_backface_visible: bool,
                                             parent: &WrSpaceAndClipChain,
                                             center: LayoutPoint,
                                             radius: LayoutSize,
                                             stops: *const GradientStop,
                                             stops_count: usize,
                                             extend_mode: ExtendMode,
                                             tile_size: LayoutSize,
                                             tile_spacing: LayoutSize) {
    debug_assert!(unsafe { is_in_main_thread() });

    let stops_slice = unsafe { make_slice(stops, stops_count) };
    let stops_vector = stops_slice.to_owned();

    let gradient = state.frame_builder
                        .dl_builder
                        .create_radial_gradient(center.into(),
                                                radius.into(),
                                                stops_vector,
                                                extend_mode.into());

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

    state.frame_builder.dl_builder.push_radial_gradient(
        &prim_info,
        rect,
        gradient,
        tile_size,
        tile_spacing);
}

#[no_mangle]
pub extern "C" fn wr_dp_push_box_shadow(state: &mut WrState,
                                        _rect: LayoutRect,
                                        clip: LayoutRect,
                                        is_backface_visible: bool,
                                        parent: &WrSpaceAndClipChain,
                                        box_bounds: LayoutRect,
                                        offset: LayoutVector2D,
                                        color: ColorF,
                                        blur_radius: f32,
                                        spread_radius: f32,
                                        border_radius: BorderRadius,
                                        clip_mode: BoxShadowClipMode) {
    debug_assert!(unsafe { is_in_main_thread() });

    let space_and_clip = parent.to_webrender(state.pipeline_id);

    let prim_info = CommonItemProperties {
        clip_rect: clip,
        clip_id: space_and_clip.clip_id,
        spatial_id: space_and_clip.spatial_id,
        is_backface_visible,
        hit_info: state.current_tag,
    };

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
pub extern "C" fn wr_dump_display_list(state: &mut WrState,
                                       indent: usize,
                                       start: *const usize,
                                       end: *const usize) -> usize {
    let start = unsafe { start.as_ref().cloned() };
    let end = unsafe { end.as_ref().cloned() };
    let range = Range { start, end };
    let mut sink = Cursor::new(Vec::new());
    let index = state.frame_builder
                     .dl_builder
                     .emit_display_list(indent, range, &mut sink);

    // For Android, dump to logcat instead of stderr. This is the same as
    // what printf_stderr does on the C++ side.

    #[cfg(target_os = "android")]
    unsafe {
        __android_log_write(4 /* info */,
                            CString::new("Gecko").unwrap().as_ptr(),
                            CString::new(sink.into_inner()).unwrap().as_ptr());
    }

    #[cfg(not(target_os = "android"))]
    eprint!("{}", String::from_utf8(sink.into_inner()).unwrap());

    index
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
        // 0 (== CompositorHitTestInvisibleToHit). In the future if we allow this,
        // we'll want to |continue| on the loop in this scenario.
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
                               width: i32,
                               height: i32,
                               format: ImageFormat,
                               visible_rect: &DeviceIntRect,
                               tile_size: Option<&u16>,
                               tile_offset: Option<&TileOffset>,
                               dirty_rect: Option<&LayoutIntRect>,
                               output: MutByteSlice)
                               -> bool;
}

#[no_mangle]
pub extern "C" fn wr_root_scroll_node_id() -> WrSpatialId {
    // The PipelineId doesn't matter here, since we just want the numeric part of the id
    // produced for any given root reference frame.
    WrSpatialId { id: SpatialId::root_scroll_node(PipelineId(0, 0)).0 }
}

#[no_mangle]
pub extern "C" fn wr_root_clip_id() -> WrClipId {
    // The PipelineId doesn't matter here, since we just want the numeric part of the id
    // produced for any given root reference frame.
    WrClipId::from_webrender(ClipId::root(PipelineId(0, 0)))
 }


#[repr(C)]
#[derive(Clone, Copy)]
pub struct WrClipId {
    id: usize,
}

impl WrClipId {
    fn to_webrender(&self, pipeline_id: WrPipelineId) -> ClipId {
        ClipId::Clip(self.id, pipeline_id)
    }

    fn from_webrender(clip_id: ClipId) -> Self {
        match clip_id {
            ClipId::Clip(id, _) => WrClipId { id },
            ClipId::ClipChain(_) => panic!("Unexpected clip chain"),
        }
    }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct WrSpatialId {
    id: usize,
}

impl WrSpatialId {
    fn to_webrender(&self, pipeline_id: WrPipelineId) -> SpatialId {
        SpatialId::new(self.id, pipeline_id)
    }
}

#[no_mangle]
pub unsafe extern "C" fn wr_device_delete(device: *mut Device) {
    Box::from_raw(device);
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern "C" fn wr_shaders_new(gl_context: *mut c_void,
                                 program_cache: Option<&mut WrProgramCache>) -> *mut WrShaders {
    let mut device = wr_device_new(gl_context, program_cache);

    let precache_flags = if env_var_to_bool("MOZ_WR_PRECACHE_SHADERS") {
        ShaderPrecacheFlags::FULL_COMPILE
    } else {
        ShaderPrecacheFlags::ASYNC_COMPILE
    };

    let opts = RendererOptions {
        precache_flags,
        allow_pixel_local_storage_support: false,
        ..Default::default()
    };

    let gl_type = device.gl().get_type();
    device.begin_frame();

    let shaders = Rc::new(RefCell::new(match Shaders::new(&mut device, gl_type, &opts) {
        Ok(shaders) => shaders,
        Err(e) => {
            warn!(" Failed to create a Shaders: {:?}", e);
            let msg = CString::new(format!("wr_shaders_new: {:?}", e)).unwrap();
            unsafe {
                gfx_critical_note(msg.as_ptr());
            }
            return ptr::null_mut();
        }
    }));

    let shaders = WrShaders { shaders };

    device.end_frame();
    Box::into_raw(Box::new(shaders))
}

#[no_mangle]
pub unsafe extern "C" fn wr_shaders_delete(shaders: *mut WrShaders, gl_context: *mut c_void) {
    let mut device = wr_device_new(gl_context, None);
    let shaders = Box::from_raw(shaders);
    if let Ok(shaders) = Rc::try_unwrap(shaders.shaders) {
      shaders.into_inner().deinit(&mut device);
    }
    // let shaders go out of scope and get dropped
}

#[no_mangle]
pub unsafe extern "C" fn wr_program_cache_report_memory(
    cache: *const WrProgramCache,
    size_of_op: VoidPtrToSizeFn,
    ) -> usize {
    (*cache).program_cache.report_memory(size_of_op)
}

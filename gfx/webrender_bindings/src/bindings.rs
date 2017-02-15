use std::ffi::CString;
use std::{mem, slice};
use std::path::PathBuf;
use std::os::raw::{c_void, c_char};
use gleam::gl;
use webrender_traits::{BorderSide, BorderStyle, BorderRadius};
use webrender_traits::{PipelineId, ClipRegion, PropertyBinding};
use webrender_traits::{Epoch, ColorF, GlyphInstance, ImageDescriptor};
use webrender_traits::{FilterOp, ImageData, ImageFormat, ImageKey, ImageMask, ImageRendering, RendererKind, MixBlendMode};
use webrender_traits::{ExternalImageId, RenderApi, FontKey};
use webrender_traits::{DeviceUintSize, ExternalEvent};
use webrender_traits::{LayoutPoint, LayoutRect, LayoutSize, LayoutTransform};
use webrender_traits::ServoScrollRootId;
use webrender::renderer::{Renderer, RendererOptions};
use webrender::renderer::{ExternalImage, ExternalImageHandler, ExternalImageSource};
use webrender::{ApiRecordingReceiver, BinaryRecorder};
use app_units::Au;

extern crate webrender_traits;

static ENABLE_RECORDING: bool = false;

// This macro adds some checks to make sure we notice when the memory representation of
// types change.
macro_rules! check_ffi_type {
    ($check_sizes_match:ident struct $TypeName:ident as ($T1:ident, $T2:ident)) => (
        fn $check_sizes_match() {
            #[repr(C)] struct TestType($T1, $T2);
            let _ = mem::transmute::<$TypeName, TestType>;
        }
    );
    ($check_sizes_match:ident struct $TypeName:ident as ($T:ident)) => (
        fn $check_sizes_match() {
            #[repr(C)] struct TestType($T);
            let _ = mem::transmute::<$TypeName, TestType>;
        }
    );
    ($check_sizes_match:ident enum $TypeName:ident as $T:ident) => (
        fn $check_sizes_match() { let _ = mem::transmute::<$TypeName, $T>; }
    );
}

check_ffi_type!(_pipeline_id_repr struct PipelineId as (u32, u32));
check_ffi_type!(_image_key_repr struct ImageKey as (u32, u32));
check_ffi_type!(_font_key_repr struct FontKey as (u32, u32));
check_ffi_type!(_epoch_repr struct Epoch as (u32));
check_ffi_type!(_image_format_repr enum ImageFormat as u32);
check_ffi_type!(_border_style_repr enum BorderStyle as u32);
check_ffi_type!(_image_rendering_repr enum ImageRendering as u32);

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct WrWindowId(u64);

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub struct WrImageDescriptor {
    pub format: ImageFormat,
    pub width: u32,
    pub height: u32,
    pub stride: u32,
    pub is_opaque: bool,
}

fn get_proc_address(glcontext_ptr: *mut c_void, name: &str) -> *const c_void{

    extern  {
        fn get_proc_address_from_glcontext(glcontext_ptr: *mut c_void, procname: *const c_char) -> *const c_void;
    }

    let symbol_name = CString::new(name).unwrap();
    let symbol = unsafe {
        get_proc_address_from_glcontext(glcontext_ptr, symbol_name.as_ptr())
    };

    // For now panic, not sure we should be though or if we can recover
    if symbol.is_null() {
        // XXX Bug 1322949 Make whitelist for extensions
        println!("Could not find symbol {:?} by glcontext", symbol_name);
    }

    symbol as *const _
}

extern  {
    fn is_in_compositor_thread() -> bool;
    fn is_in_render_thread() -> bool;
}

#[no_mangle]
pub extern fn wr_renderer_update(renderer: &mut Renderer) {
    renderer.update();
}

#[no_mangle]
pub extern fn wr_renderer_render(renderer: &mut Renderer, width: u32, height: u32) {
    renderer.render(DeviceUintSize::new(width, height));
}

// Call wr_renderer_render() before calling this function.
#[no_mangle]
pub unsafe extern fn wr_renderer_readback(width: u32, height: u32,
                                          dst_buffer: *mut u8, buffer_size: usize) {
    assert!(is_in_render_thread());

    gl::flush();

    let mut slice = slice::from_raw_parts_mut(dst_buffer, buffer_size);
    gl::read_pixels_into_buffer(0, 0,
                                width as gl::GLsizei,
                                height as gl::GLsizei,
                                gl::BGRA,
                                gl::UNSIGNED_BYTE,
                                slice);
}

#[no_mangle]
pub extern fn wr_renderer_set_profiler_enabled(renderer: &mut Renderer, enabled: bool) {
    renderer.set_profiler_enabled(enabled);
}

#[no_mangle]
pub extern fn wr_renderer_current_epoch(renderer: &mut Renderer,
                                        pipeline_id: PipelineId,
                                        out_epoch: &mut Epoch) -> bool {
    if let Some(epoch) = renderer.current_epoch(pipeline_id) {
        *out_epoch = epoch;
        return true;
    }
    return false;
}

#[no_mangle]
pub unsafe extern fn wr_renderer_delete(renderer: *mut Renderer) {
    Box::from_raw(renderer);
}

#[no_mangle]
pub unsafe extern fn wr_api_delete(api: *mut RenderApi) {
    let api = Box::from_raw(api);
    api.shut_down();
}

#[no_mangle]
pub unsafe extern fn wr_api_set_root_display_list(api: &mut RenderApi,
                                                  state: &mut WrState,
                                                  epoch: Epoch,
                                                  viewport_width: f32,
                                                  viewport_height: f32) {
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);
    let frame_builder = mem::replace(&mut state.frame_builder,
                                     WebRenderFrameBuilder::new(state.pipeline_id));
    // See the documentation of set_root_display_list in api.rs. I don't think
    // it makes a difference in gecko at the moment(until APZ is figured out)
    // but I suppose it is a good default.
    let preserve_frame_state = true;
    //let (dl_builder, aux_builder) = fb.dl_builder.finalize();
    api.set_root_display_list(Some(root_background_color),
                              epoch,
                              LayoutSize::new(viewport_width, viewport_height),
                              frame_builder.dl_builder,
                              preserve_frame_state);
    api.generate_frame(None);
}
#[no_mangle]
pub extern fn wr_window_new(window_id: WrWindowId,
                            enable_profiler: bool,
                            out_api: &mut *mut RenderApi,
                            out_renderer: &mut *mut Renderer) -> bool {
    assert!(unsafe { is_in_render_thread() });

    let recorder: Option<Box<ApiRecordingReceiver>> = if ENABLE_RECORDING {
        Some(Box::new(BinaryRecorder::new(&PathBuf::from("wr-record.bin"))))
    } else {
        None
    };

    let opts = RendererOptions {
        device_pixel_ratio: 1.0,
        resource_override_path: None,
        enable_aa: false,
        enable_subpixel_aa: false,
        enable_profiler: enable_profiler,
        enable_scrollbars: false,
        precache_shaders: false,
        renderer_kind: RendererKind::Native,
        debug: false,
        clear_framebuffer: true,
        render_target_debug: false,
        clear_color: ColorF::new(1.0, 1.0, 1.0, 1.0),
        recorder: recorder,
        workers: None,
    };

    let (renderer, sender) = match Renderer::new(opts) {
        Ok((renderer, sender)) => { (renderer, sender) }
        Err(e) => {
            println!(" Failed to create a Renderer: {:?}", e);
            return false;
        }
    };

    renderer.set_render_notifier(Box::new(CppNotifier { window_id: window_id }));

    *out_api = Box::into_raw(Box::new(sender.create_api()));
    *out_renderer = Box::into_raw(Box::new(renderer));

    return true;
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern fn wr_gl_init(gl_context: *mut c_void) {
    assert!(unsafe { is_in_render_thread() });

    gl::load_with(|symbol| get_proc_address(gl_context, symbol));
    gl::clear_color(0.3, 0.0, 0.0, 1.0);

    let version = gl::get_string(gl::VERSION);

    println!("WebRender - OpenGL version new {}", version);
}

#[no_mangle]
pub extern fn wr_state_new(width: u32, height: u32, pipeline_id: PipelineId) -> *mut WrState {
    assert!(unsafe { is_in_compositor_thread() });

    let state = Box::new(WrState {
        size: (width, height),
        pipeline_id: pipeline_id,
        z_index: 0,
        frame_builder: WebRenderFrameBuilder::new(pipeline_id),
    });

    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_state_delete(state:*mut WrState) {
    assert!(unsafe { is_in_compositor_thread() });

    unsafe {
        Box::from_raw(state);
    }
}

#[no_mangle]
pub extern fn wr_dp_begin(state: &mut WrState, width: u32, height: u32) {
    assert!( unsafe { is_in_compositor_thread() });
    state.size = (width, height);
    state.frame_builder.dl_builder.list.clear();
    state.z_index = 0;

    let bounds = LayoutRect::new(LayoutPoint::new(0.0, 0.0), LayoutSize::new(width as f32, height as f32));

    state.frame_builder.dl_builder.push_stacking_context(
        webrender_traits::ScrollPolicy::Scrollable,
        bounds,
        ClipRegion::simple(&bounds),
        0,
        PropertyBinding::Value(LayoutTransform::identity()),
        LayoutTransform::identity(),
        webrender_traits::MixBlendMode::Normal,
        Vec::new(),
    );
}

#[no_mangle]
pub extern fn wr_dp_end(state: &mut WrState, api: &mut RenderApi, epoch: u32) {
    assert!( unsafe { is_in_compositor_thread() });
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);
    let pipeline_id = state.pipeline_id;
    let (width, height) = state.size;

    state.frame_builder.dl_builder.pop_stacking_context();

    let fb = mem::replace(&mut state.frame_builder, WebRenderFrameBuilder::new(pipeline_id));

    let preserve_frame_state = true;
    api.set_root_display_list(Some(root_background_color),
                              Epoch(epoch),
                              LayoutSize::new(width as f32, height as f32),
                              fb.dl_builder,
                              preserve_frame_state);
    api.generate_frame(None);
}

#[no_mangle]
pub unsafe extern fn wr_renderer_flush_rendered_epochs(renderer: &mut Renderer) -> *mut Vec<(PipelineId, Epoch)> {
    let map = renderer.flush_rendered_epochs();
    let pipeline_epochs = Box::new(map.into_iter().collect());
    return Box::into_raw(pipeline_epochs);
}

#[no_mangle]
pub unsafe extern fn wr_rendered_epochs_next(pipeline_epochs: &mut Vec<(PipelineId, Epoch)>,
                                             out_pipeline: &mut PipelineId,
                                             out_epoch: &mut Epoch) -> bool {
    if let Some((pipeline, epoch)) = pipeline_epochs.pop() {
        *out_pipeline = pipeline;
        *out_epoch = epoch;
        return true;
    }
    return false;
}

#[no_mangle]
pub unsafe extern fn wr_rendered_epochs_delete(pipeline_epochs: *mut Vec<(PipelineId, Epoch)>) {
    Box::from_raw(pipeline_epochs);
}


struct CppNotifier {
    window_id: WrWindowId,
}

unsafe impl Send for CppNotifier {}

extern {
    fn wr_notifier_new_frame_ready(window_id: WrWindowId);
    fn wr_notifier_new_scroll_frame_ready(window_id: WrWindowId, composite_needed: bool);
    fn wr_notifier_external_event(window_id: WrWindowId, raw_event: usize);
}

impl webrender_traits::RenderNotifier for CppNotifier {
    fn new_frame_ready(&mut self) {
        unsafe {
            wr_notifier_new_frame_ready(self.window_id);
        }
    }

    fn new_scroll_frame_ready(&mut self, composite_needed: bool) {
        unsafe {
            wr_notifier_new_scroll_frame_ready(self.window_id, composite_needed);
        }
    }

    fn external_event(&mut self, event: ExternalEvent) {
        unsafe {
            wr_notifier_external_event(self.window_id, event.unwrap());
        }
    }
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
    pub root_pipeline_id: PipelineId,
    pub dl_builder: webrender_traits::DisplayListBuilder,
}

impl WebRenderFrameBuilder {
    pub fn new(root_pipeline_id: PipelineId) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            root_pipeline_id: root_pipeline_id,
            dl_builder: webrender_traits::DisplayListBuilder::new(root_pipeline_id),
        }
    }
}

pub struct WrState {
    size: (u32, u32),
    pipeline_id: PipelineId,
    z_index: i32,
    frame_builder: WebRenderFrameBuilder,
}

#[repr(C)]
enum WrExternalImageType {
    TEXTURE_HANDLE,

    // TODO(Jerry): handle shmem or cpu raw buffers.
    //// MEM_OR_SHMEM,
}

#[repr(C)]
struct WrExternalImageStruct {
    image_type: WrExternalImageType,

    // Texture coordinate
    u0: f32,
    v0: f32,
    u1: f32,
    v1: f32,

    // external buffer handle
    handle: u32,

    // TODO(Jerry): handle shmem or cpu raw buffers.
    //// buff: *const u8,
    //// size: usize,
}

type LockExternalImageCallback = fn(*mut c_void, ExternalImageId) -> WrExternalImageStruct;
type UnlockExternalImageCallback = fn(*mut c_void, ExternalImageId);
type ReleaseExternalImageCallback = fn(*mut c_void, ExternalImageId);

#[repr(C)]
pub struct WrExternalImageHandler {
    external_image_obj: *mut c_void,
    lock_func: LockExternalImageCallback,
    unlock_func: UnlockExternalImageCallback,
    release_func: ReleaseExternalImageCallback,
}

impl ExternalImageHandler for WrExternalImageHandler {
    fn lock(&mut self, id: ExternalImageId) -> ExternalImage {
        let image = (self.lock_func)(self.external_image_obj, id);

        match image.image_type {
            WrExternalImageType::TEXTURE_HANDLE =>
                ExternalImage {
                    u0: image.u0,
                    v0: image.v0,
                    u1: image.u1,
                    v1: image.v1,
                    source: ExternalImageSource::NativeTexture(image.handle)
                },
        }
    }

    fn unlock(&mut self, id: ExternalImageId) {
        (self.unlock_func)(self.external_image_obj, id);
    }

    fn release(&mut self, id: ExternalImageId) {
        (self.release_func)(self.external_image_obj, id);
    }
}

#[repr(C)]
pub enum WrMixBlendMode
{
    Normal,
    Multiply,
    Screen,
    Overlay,
    Darken,
    Lighten,
    ColorDodge,
    ColorBurn,
    HardLight,
    SoftLight,
    Difference,
    Exclusion,
    Hue,
    Saturation,
    Color,
    Luminosity,
}

impl WrMixBlendMode
{
    pub fn to_mix_blend_mode(self) -> MixBlendMode
    {
        match self
        {
            WrMixBlendMode::Normal => MixBlendMode::Normal,
            WrMixBlendMode::Multiply => MixBlendMode::Multiply,
            WrMixBlendMode::Screen => MixBlendMode::Screen,
            WrMixBlendMode::Overlay => MixBlendMode::Overlay,
            WrMixBlendMode::Darken => MixBlendMode::Darken,
            WrMixBlendMode::Lighten => MixBlendMode::Lighten,
            WrMixBlendMode::ColorDodge => MixBlendMode::ColorDodge,
            WrMixBlendMode::ColorBurn => MixBlendMode::ColorBurn,
            WrMixBlendMode::HardLight => MixBlendMode::HardLight,
            WrMixBlendMode::SoftLight => MixBlendMode::SoftLight,
            WrMixBlendMode::Difference => MixBlendMode::Difference,
            WrMixBlendMode::Exclusion => MixBlendMode::Exclusion,
            WrMixBlendMode::Hue => MixBlendMode::Hue,
            WrMixBlendMode::Saturation => MixBlendMode::Saturation,
            WrMixBlendMode::Color => MixBlendMode::Color,
            WrMixBlendMode::Luminosity => MixBlendMode::Luminosity,
        }
    }
}

#[no_mangle]
pub extern fn wr_dp_push_stacking_context(state:&mut WrState, bounds: WrRect, overflow: WrRect, mask: *const WrImageMask, opacity: f32, transform: &LayoutTransform, mix_blend_mode: WrMixBlendMode)
{
    assert!( unsafe { is_in_compositor_thread() });
    state.z_index += 1;

    let bounds = bounds.to_rect();
    let mut overflow = overflow.to_rect();
    let mix_blend_mode = mix_blend_mode.to_mix_blend_mode();
    //println!("stacking context: {:?} {:?} {:?} {:?} {:?}", state.pipeline_id, bounds, overflow, mask, transform);
    // convert from the C type to the Rust type
    let mask = unsafe { mask.as_ref().map(|&WrImageMask{image, ref rect,repeat}| ImageMask{image: image, rect: rect.to_rect(), repeat: repeat}) };

    let clip_region2 = state.frame_builder.dl_builder.new_clip_region(&overflow, vec![], None);
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&overflow, vec![], mask);

    let mut filters: Vec<FilterOp> = Vec::new();
    if opacity < 1.0 {
        filters.push(FilterOp::Opacity(PropertyBinding::Value(opacity)));
    }

    state.frame_builder.dl_builder.push_stacking_context(webrender_traits::ScrollPolicy::Scrollable,
                                  bounds,
                                  clip_region2,
                                  state.z_index,
                                  PropertyBinding::Value(*transform),
                                  LayoutTransform::identity(),
                                  mix_blend_mode,
                                  filters);
    state.frame_builder.dl_builder.push_scroll_layer(clip_region, bounds.size, ServoScrollRootId(1));

}

#[no_mangle]
pub extern fn wr_dp_pop_stacking_context(state: &mut WrState)
{
    assert!( unsafe { is_in_compositor_thread() });
    state.frame_builder.dl_builder.pop_scroll_layer();
    state.frame_builder.dl_builder.pop_stacking_context();
    //println!("pop_stacking {:?}", state.pipeline_id);
}

#[no_mangle]
pub extern fn wr_dp_push_scroll_layer(state: &mut WrState, bounds: WrRect, overflow: WrRect, mask: Option<&WrImageMask>)
{
    let bounds = bounds.to_rect();
    let overflow = overflow.to_rect();
    let mask = mask.map(|&WrImageMask{image, ref rect,repeat}| ImageMask{image: image, rect: rect.to_rect(), repeat: repeat});
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&overflow, vec![], mask);
    state.frame_builder.dl_builder.push_scroll_layer(clip_region, bounds.size, ServoScrollRootId(1));
}

#[no_mangle]
pub extern fn wr_dp_pop_scroll_layer(state: &mut WrState)
{
    assert!( unsafe { is_in_compositor_thread() });
    state.frame_builder.dl_builder.pop_scroll_layer();
}

#[no_mangle]
pub extern fn wr_api_set_root_pipeline(api: &mut RenderApi, pipeline_id: PipelineId) {
    api.set_root_pipeline(pipeline_id);
    api.generate_frame(None);
}

#[no_mangle]
pub extern fn wr_api_add_image(api: &mut RenderApi, descriptor: &WrImageDescriptor, bytes: * const u8, size: usize) -> ImageKey {
    assert!( unsafe { is_in_compositor_thread() });
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };
    return api.add_image(
        ImageDescriptor {
            width: descriptor.width,
            height: descriptor.height,
            stride: if descriptor.stride != 0 { Some(descriptor.stride) } else { None },
            format: descriptor.format,
            is_opaque: descriptor.is_opaque,
        },
        ImageData::new(bytes)
    );
}

#[no_mangle]
pub extern fn wr_api_add_external_image_texture(api: &mut RenderApi, width: u32, height: u32, format: ImageFormat, external_image_id: u64) -> ImageKey {
    assert!( unsafe { is_in_compositor_thread() });
    unimplemented!(); // TODO
    //api.add_image(ImageDescriptor{width:width, height:height, stride:None, format: format, is_opaque: false}, ImageData::External(ExternalImageId(external_image_id)))
}

#[no_mangle]
pub extern fn wr_api_update_image(api: &mut RenderApi, key: ImageKey, descriptor: &WrImageDescriptor, bytes: * const u8, size: usize) {
    assert!( unsafe { is_in_compositor_thread() });
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };

    api.update_image(
        key,
        ImageDescriptor {
            width: descriptor.width,
            height: descriptor.height,
            stride: if descriptor.stride != 0 { Some(descriptor.stride) } else { None },
            format: descriptor.format,
            is_opaque: descriptor.is_opaque,
        },
        bytes
    );
}
#[no_mangle]
pub extern fn wr_api_delete_image(api: &mut RenderApi, key: ImageKey) {
    assert!( unsafe { is_in_compositor_thread() });
    api.delete_image(key)
}

#[no_mangle]
pub extern fn wr_api_send_external_event(api: &mut RenderApi, evt: usize) {
    assert!(unsafe { is_in_compositor_thread() });

    api.send_external_event(ExternalEvent::from_raw(evt));
}


#[no_mangle]
pub extern fn wr_dp_push_rect(state: &mut WrState, rect: WrRect, clip: WrRect, color: WrColor) {
    assert!( unsafe { is_in_compositor_thread() });
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(), Vec::new(), None);

    state.frame_builder.dl_builder.push_rect(
                                    rect.to_rect(),
                                    clip_region,
                                    color.to_color());
}

#[no_mangle]
pub extern fn wr_dp_push_border(state: &mut WrState, rect: WrRect, clip: WrRect,
                                top: WrBorderSide, right: WrBorderSide, bottom: WrBorderSide, left: WrBorderSide,
                                radius: WrBorderRadius) {
    assert!( unsafe { is_in_compositor_thread() });
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(), Vec::new(), None);
    state.frame_builder.dl_builder.push_border(
                                    rect.to_rect(),
                                    clip_region,
                                    left.to_border_side(),
                                    top.to_border_side(),
                                    right.to_border_side(),
                                    bottom.to_border_side(),
                                    radius.to_border_radius());
}

#[no_mangle]
pub extern fn wr_dp_push_iframe(state: &mut WrState, rect: WrRect, clip: WrRect, pipeline_id: PipelineId) {
    assert!( unsafe { is_in_compositor_thread() });

    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(),
                                                                     Vec::new(),
                                                                     None);
    state.frame_builder.dl_builder.push_iframe(rect.to_rect(),
                                               clip_region,
                                               pipeline_id);
}

#[repr(C)]
pub struct WrColor
{
    r: f32,
    g: f32,
    b: f32,
    a: f32
}

impl WrColor
{
    pub fn to_color(&self) -> ColorF
    {
        ColorF::new(self.r, self.g, self.b, self.a)
    }
}

#[repr(C)]
pub struct WrBorderRadius {
    pub top_left: WrLayoutSize,
    pub top_right: WrLayoutSize,
    pub bottom_left: WrLayoutSize,
    pub bottom_right: WrLayoutSize,
}

impl WrBorderRadius
{
    pub fn to_border_radius(&self) -> BorderRadius
    {
        BorderRadius { top_left: self.top_left.to_layout_size(),
                       top_right: self.top_right.to_layout_size(),
                       bottom_left: self.bottom_left.to_layout_size(),
                       bottom_right: self.bottom_right.to_layout_size() }
    }
}

#[repr(C)]
pub struct WrBorderSide
{
    width: f32,
    color: WrColor,
    style: BorderStyle
}

impl WrBorderSide
{
    pub fn to_border_side(&self) -> BorderSide
    {
        BorderSide { width: self.width, color: self.color.to_color(), style: self.style }
    }
}

#[repr(C)]
pub struct WrLayoutSize
{
    width: f32,
    height: f32
}

impl WrLayoutSize
{
    pub fn to_layout_size(&self) -> LayoutSize
    {
        LayoutSize::new(self.width, self.height)
    }
}

#[repr(C)]
pub struct WrRect
{
    x: f32,
    y: f32,
    width: f32,
    height: f32
}

impl WrRect
{
    pub fn to_rect(&self) -> LayoutRect
    {
        LayoutRect::new(LayoutPoint::new(self.x, self.y), LayoutSize::new(self.width, self.height))
    }
}

#[repr(C)]
pub struct WrImageMask
{
    image: ImageKey,
    rect: WrRect,
    repeat: bool
}

impl WrImageMask
{
    pub fn to_image_mask(&self) -> ImageMask
    {
        ImageMask { image: self.image, rect: self.rect.to_rect(), repeat: self.repeat }
    }
}

#[no_mangle]
pub extern fn wr_dp_push_image(state:&mut WrState, bounds: WrRect, clip : WrRect, mask: *const WrImageMask, filter: ImageRendering, key: ImageKey) {
    assert!( unsafe { is_in_compositor_thread() });

    let bounds = bounds.to_rect();
    let clip = clip.to_rect();

    //println!("push_image bounds {:?} clip {:?}", bounds, clip);
    // convert from the C type to the Rust type, mapping NULL to None
    let mask = unsafe { mask.as_ref().map(|m| m.to_image_mask()) };
    let image_rendering = filter;

    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip, Vec::new(), mask);
    state.frame_builder.dl_builder.push_image(
        bounds,
        clip_region,
        bounds.size,
        bounds.size,
        image_rendering,
        key
    );
}

#[no_mangle]
pub extern fn wr_api_add_raw_font(api: &mut RenderApi,
                                  font_buffer: *mut u8,
                                  buffer_size: usize) -> FontKey
{
    assert!( unsafe { is_in_compositor_thread() });

    let font_slice = unsafe {
        slice::from_raw_parts(font_buffer, buffer_size as usize)
    };
    let mut font_vector = Vec::new();
    font_vector.extend_from_slice(font_slice);

    return api.add_raw_font(font_vector);
}

#[no_mangle]
pub extern fn wr_dp_push_text(state: &mut WrState,
                              bounds: WrRect,
                              clip: WrRect,
                              color: WrColor,
                              font_key: FontKey,
                              glyphs: *mut GlyphInstance,
                              glyph_count: u32,
                              glyph_size: f32)
{
    assert!( unsafe { is_in_compositor_thread() });

    let glyph_slice = unsafe {
        slice::from_raw_parts(glyphs, glyph_count as usize)
    };
    let mut glyph_vector = Vec::new();
    glyph_vector.extend_from_slice(&glyph_slice);

    let colorf = ColorF::new(color.r, color.g, color.b, color.a);

    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(), Vec::new(), None);

    let glyph_options = None; // TODO
    state.frame_builder.dl_builder.push_text(bounds.to_rect(),
                                             clip_region,
                                             glyph_vector,
                                             font_key,
                                             colorf,
                                             Au::from_f32_px(glyph_size),
                                             Au::from_px(0),
                                             glyph_options);
}


use fnv::FnvHasher;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::hash::BuildHasherDefault;
use std::{mem, slice};
use std::os::raw::{c_void, c_char};
use gleam::gl;
use webrender_traits::{BorderSide, BorderStyle, BorderRadius};
use webrender_traits::{PipelineId, ClipRegion};
use webrender_traits::{Epoch, ColorF};
use webrender_traits::{ImageData, ImageFormat, ImageKey, ImageMask, ImageRendering, RendererKind};
use webrender_traits::{ExternalImageId};
use webrender_traits::{DeviceUintSize};
use webrender_traits::{LayoutPoint, LayoutRect, LayoutSize, LayoutTransform};
use webrender::renderer::{Renderer, RendererOptions};
use webrender::renderer::{ExternalImage, ExternalImageHandler, ExternalImageSource};
use std::sync::{Arc, Mutex, Condvar};
extern crate webrender_traits;

fn pipeline_id_to_u64(id: PipelineId) -> u64 { (id.0 as u64) << 32 + id.1 as u64 }
fn u64_to_pipeline_id(id: u64) -> PipelineId { PipelineId((id >> 32) as u32, id as u32) }

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
pub unsafe extern fn wr_api_delete(api: *mut webrender_traits::RenderApi) {
    Box::from_raw(api);
}

#[no_mangle]
pub extern fn wr_window_new(window_id: u64,
                            enable_profiler: bool,
                            out_api: &mut *mut webrender_traits::RenderApi,
                            out_renderer: &mut *mut Renderer) {
    assert!(unsafe { is_in_compositor_thread() });

    let opts = RendererOptions {
        device_pixel_ratio: 1.0,
        resource_override_path: None,
        enable_aa: false,
        enable_subpixel_aa: false,
        enable_msaa: false,
        enable_profiler: enable_profiler,
        enable_recording: false,
        enable_scrollbars: false,
        precache_shaders: false,
        renderer_kind: RendererKind::Native,
        debug: false,
        clear_framebuffer: true,
        clear_empty_tiles: false,
        clear_color: ColorF::new(1.0, 1.0, 1.0, 1.0),
    };

    let (renderer, sender) = Renderer::new(opts);
    renderer.set_render_notifier(Box::new(CppNotifier { window_id: window_id }));

    *out_api = Box::into_raw(Box::new(sender.create_api()));
    *out_renderer = Box::into_raw(Box::new(renderer));
}

// Call MakeCurrent before this.
#[no_mangle]
pub extern fn wr_gl_init(gl_context: *mut c_void) {
    assert!(unsafe { is_in_render_thread() });

    gl::load_with(|symbol| get_proc_address(gl_context, symbol));
    gl::clear_color(0.3, 0.0, 0.0, 1.0);

    let version = unsafe {
        let data = CStr::from_ptr(gl::GetString(gl::VERSION) as *const _).to_bytes().to_vec();
        String::from_utf8(data).unwrap()
    };

    println!("WebRender - OpenGL version new {}", version);
}

#[no_mangle]
pub extern fn wr_state_new(width: u32, height: u32, pipeline: u64) -> *mut WrState {
    assert!(unsafe { is_in_compositor_thread() });
    let pipeline_id = u64_to_pipeline_id(pipeline);

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

struct CppNotifier {
    window_id: u64,
}

unsafe impl Send for CppNotifier {}

extern {
    fn wr_notifier_new_frame_ready(window_id: u64);
    fn wr_notifier_new_scroll_frame_ready(window_id: u64, composite_needed: bool);
    fn wr_notifier_pipeline_size_changed(window_id: u64, pipeline: u64, new_width: f32, new_height: f32);
    // TODO: Waiting for PR #688
    fn wr_notifier_external_event(window_id: u64, raw_event: usize);
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

    fn pipeline_size_changed(&mut self,
                             pipeline_id: PipelineId,
                             new_size: Option<LayoutSize>) {
        let (w, h) = if let Some(size) = new_size {
            (size.width, size.height)
        } else {
            (0.0, 0.0)
        };
        unsafe {
            let id = pipeline_id_to_u64(pipeline_id);
            wr_notifier_pipeline_size_changed(self.window_id, id, w, h);
        }
    }
}

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

struct Notifier {
    render_notifier: Arc<(Mutex<bool>, Condvar)>,
}

impl webrender_traits::RenderNotifier for Notifier {
    fn new_frame_ready(&mut self) {
        assert!( unsafe { !is_in_compositor_thread() });
        let &(ref lock, ref cvar) = &*self.render_notifier;
        let mut finished = lock.lock().unwrap();
        *finished = true;
        cvar.notify_one();
    }
    fn new_scroll_frame_ready(&mut self, _: bool) {
    }

    fn pipeline_size_changed(&mut self,
                             _: PipelineId,
                             _: Option<LayoutSize>) {
    }
}

pub struct WrWindowState {
    renderer: Renderer,
    api: webrender_traits::RenderApi,
    root_pipeline_id: PipelineId,
    size: DeviceUintSize,
    render_notifier_lock: Arc<(Mutex<bool>, Condvar)>,
    pipeline_epoch_map: HashMap<PipelineId, Epoch, BuildHasherDefault<FnvHasher>>,
    pipeline_sync_list: Vec<PipelineId>,
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

type GetExternalImageCallback = fn(*mut c_void, ExternalImageId) -> WrExternalImageStruct;
type ReleaseExternalImageCallback = fn(*mut c_void, ExternalImageId);

#[repr(C)]
pub struct WrExternalImageHandler {
    external_image_obj: *mut c_void,
    get_func: GetExternalImageCallback,
    release_func: ReleaseExternalImageCallback,
}

impl ExternalImageHandler for WrExternalImageHandler {
    fn get(&mut self, id: ExternalImageId) -> ExternalImage {
        let image = (self.get_func)(self.external_image_obj, id);

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

    fn release(&mut self, id: ExternalImageId) {
        (self.release_func)(self.external_image_obj, id);
    }
}

#[no_mangle]
pub extern fn wr_init_window(root_pipeline_id: u64,
                             glcontext_ptr: *mut c_void,
                             enable_profiler: bool,
                             external_image_handler: *mut WrExternalImageHandler) -> *mut WrWindowState {
    assert!( unsafe { is_in_compositor_thread() });
    gl::load_with(|symbol| get_proc_address(glcontext_ptr, symbol));
    gl::clear_color(0.3, 0.0, 0.0, 1.0);

    let version = unsafe {
        let data = CStr::from_ptr(gl::GetString(gl::VERSION) as *const _).to_bytes().to_vec();
        String::from_utf8(data).unwrap()
    };

    println!("OpenGL version new {}", version);

    let opts = RendererOptions {
        device_pixel_ratio: 1.0,
        resource_override_path: None,
        enable_aa: false,
        enable_subpixel_aa: false,
        enable_msaa: false,
        enable_profiler: enable_profiler,
        enable_recording: false,
        enable_scrollbars: false,
        precache_shaders: false,
        renderer_kind: RendererKind::Native,
        debug: false,
        clear_framebuffer: true,
        clear_empty_tiles: false,
        clear_color: ColorF::new(1.0, 1.0, 1.0, 1.0),
    };

    let (mut renderer, sender) = Renderer::new(opts);
    let api = sender.create_api();

    let notification_lock = Arc::new((Mutex::new(false), Condvar::new()));
    let notification_lock_clone = notification_lock.clone();
    let notifier = Box::new(Notifier{render_notifier: notification_lock});
    renderer.set_render_notifier(notifier);

    if !external_image_handler.is_null() {
        renderer.set_external_image_handler(Box::new(
            unsafe {
                WrExternalImageHandler {
                    external_image_obj: (*external_image_handler).external_image_obj,
                    get_func: (*external_image_handler).get_func,
                    release_func: (*external_image_handler).release_func,
                }
            }));
    }

    let pipeline_id = u64_to_pipeline_id(root_pipeline_id);
    api.set_root_pipeline(pipeline_id);

    let state = Box::new(WrWindowState {
        renderer: renderer,
        api: api,
        root_pipeline_id: pipeline_id,
        size: DeviceUintSize::new(0, 0),
        render_notifier_lock: notification_lock_clone,
        pipeline_epoch_map: HashMap::with_hasher(Default::default()),
        pipeline_sync_list: Vec::new(),
    });
    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_create(window: &mut WrWindowState, width: u32, height: u32, layers_id: u64) -> *mut WrState {
    assert!( unsafe { is_in_compositor_thread() });
    let pipeline_id = u64_to_pipeline_id(layers_id);

    let builder = WebRenderFrameBuilder::new(pipeline_id);

    let state = Box::new(WrState {
        size: (width, height),
        pipeline_id: pipeline_id,
        z_index: 0,
        frame_builder: builder,
    });

    if pipeline_id == window.root_pipeline_id {
        window.size = DeviceUintSize::new(width, height);
    }

    window.pipeline_epoch_map.insert(pipeline_id, Epoch(0));
    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_dp_begin(window: &mut WrWindowState, state: &mut WrState, width: u32, height: u32) {
    assert!( unsafe { is_in_compositor_thread() });
    state.size = (width, height);
    state.frame_builder.dl_builder.list.clear();
    state.z_index = 0;

    if state.pipeline_id == window.root_pipeline_id {
        window.size = DeviceUintSize::new(width, height);
    }

    let bounds = LayoutRect::new(LayoutPoint::new(0.0, 0.0), LayoutSize::new(width as f32, height as f32));

    state.frame_builder.dl_builder.push_stacking_context(
        webrender_traits::ScrollPolicy::Scrollable,
        bounds,
        ClipRegion::simple(&bounds),
        0,
        &LayoutTransform::identity(),
        &LayoutTransform::identity(),
        webrender_traits::MixBlendMode::Normal,
        Vec::new(),
    );
}

#[no_mangle]
pub extern fn wr_dp_push_stacking_context(state:&mut WrState, bounds: WrRect, overflow: WrRect, transform: &LayoutTransform)
{
    assert!( unsafe { is_in_compositor_thread() });
    state.z_index += 1;

    let bounds = bounds.to_rect();
    let overflow = overflow.to_rect();

    state.frame_builder.dl_builder.push_stacking_context(webrender_traits::ScrollPolicy::Scrollable,
                                  bounds,
                                  ClipRegion::simple(&overflow),
                                  state.z_index,
                                  transform,
                                  &LayoutTransform::identity(),
                                  webrender_traits::MixBlendMode::Normal,
                                  Vec::new());

}

#[no_mangle]
pub extern fn wr_dp_pop_stacking_context(state: &mut WrState)
{
    assert!( unsafe { is_in_compositor_thread() });
    // 
    state.frame_builder.dl_builder.pop_stacking_context()
}

fn wait_for_epoch(window: &mut WrWindowState) {
    let &(ref lock, ref cvar) = &*window.render_notifier_lock;
    let mut finished = lock.lock().unwrap();

    window.pipeline_sync_list.push(window.root_pipeline_id);

    'outer: for pipeline_id in window.pipeline_sync_list.iter() {
        let epoch = window.pipeline_epoch_map.get(pipeline_id);
        if epoch.is_none() {
            // We could only push a pipeline_id for iframe without setting its root_display_list data.
            continue;
        }

        if epoch.unwrap().0 == 0 {
            // This pipeline_id is not set the display_list yet, so skip the waiting.
            continue;
        }

        loop {
            // Update all epochs.
            window.renderer.update();

            if let Some(rendered_epoch) = window.renderer.current_epoch(*pipeline_id) {
                if *(epoch.unwrap()) == rendered_epoch {
                    continue 'outer;
                }
            }

            // If the epoch is not matched, starts to wait for next frame updating.
            while !*finished {
                finished = cvar.wait(finished).unwrap();
            }
            // For the next sync one
            *finished = false;
        }
    }
    window.pipeline_sync_list.clear();
}

#[no_mangle]
pub fn wr_composite_window(window: &mut WrWindowState) {
    assert!(unsafe { is_in_render_thread() });

    gl::clear(gl::COLOR_BUFFER_BIT);

    wait_for_epoch(window);
    window.renderer.render(window.size);
}

#[no_mangle]
pub extern fn wr_dp_end(window: &mut WrWindowState,
                        state: &mut WrState) {
    assert!( unsafe { is_in_compositor_thread() });
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);
    let pipeline_id = state.pipeline_id;
    let (width, height) = state.size;

    if let Some(epoch) = window.pipeline_epoch_map.get_mut(&pipeline_id) {
        (*epoch).0 += 1;

        state.frame_builder.dl_builder.pop_stacking_context();

        let fb = mem::replace(&mut state.frame_builder, WebRenderFrameBuilder::new(pipeline_id));

        //let (dl_builder, aux_builder) = fb.dl_builder.finalize();
        window.api.set_root_display_list(Some(root_background_color),
                                         *epoch,
                                         LayoutSize::new(width as f32, height as f32),
                                         fb.dl_builder);

        return;
    }

    panic!("Could not find epoch for pipeline_id:({},{})", pipeline_id.0, pipeline_id.1);
}

#[no_mangle]
pub extern fn wr_add_image(window: &mut WrWindowState, width: u32, height: u32, stride: u32, format: ImageFormat, bytes: * const u8, size: usize) -> ImageKey {
    assert!( unsafe { is_in_compositor_thread() });
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };
    let stride_option = match stride {
        0 => None,
        _ => Some(stride),
    };

    window.api.add_image(width, height, stride_option, format, ImageData::new(bytes))
}

#[no_mangle]
pub extern fn wr_add_external_image_texture(window: &mut WrWindowState, width: u32, height: u32, format: ImageFormat, external_image_id: u64) -> ImageKey {
    assert!( unsafe { is_in_compositor_thread() });
    window.api.add_image(width, height, None, format, ImageData::External(ExternalImageId(external_image_id)))
}

#[no_mangle]
pub extern fn wr_update_image(window: &mut WrWindowState, key: ImageKey, width: u32, height: u32, format: ImageFormat, bytes: * const u8, size: usize) {
    assert!( unsafe { is_in_compositor_thread() });
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };
    window.api.update_image(key, width, height, format, bytes);
}

#[no_mangle]
pub extern fn wr_delete_image(window: &mut WrWindowState, key: ImageKey) {
    assert!( unsafe { is_in_compositor_thread() });
    window.api.delete_image(key)
}

#[no_mangle]
pub extern fn wr_dp_push_rect(state: &mut WrState, rect: WrRect, clip: WrRect, r: f32, g: f32, b: f32, a: f32) {
    assert!( unsafe { is_in_compositor_thread() });
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(), Vec::new(), None);

    state.frame_builder.dl_builder.push_rect(
                                    rect.to_rect(),
                                    clip_region,
                                    ColorF::new(r, g, b, a));
}

#[no_mangle]
pub extern fn wr_dp_push_border(state: &mut WrState, rect: WrRect, clip: WrRect,
                                top: WrBorderSide, right: WrBorderSide, bottom: WrBorderSide, left: WrBorderSide,
                                top_left_radius: WrLayoutSize, top_right_radius: WrLayoutSize,
                                bottom_left_radius: WrLayoutSize, bottom_right_radius: WrLayoutSize) {
    assert!( unsafe { is_in_compositor_thread() });
    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(), Vec::new(), None);
    let radius = BorderRadius { top_left: top_left_radius.to_layout_size(),
                                top_right: top_right_radius.to_layout_size(),
                                bottom_left: bottom_left_radius.to_layout_size(),
                                bottom_right: bottom_right_radius.to_layout_size() };
    state.frame_builder.dl_builder.push_border(
                                    rect.to_rect(),
                                    clip_region,
                                    left.to_border_side(),
                                    top.to_border_side(),
                                    right.to_border_side(),
                                    bottom.to_border_side(),
                                    radius);
}

#[no_mangle]
pub extern fn wr_dp_push_iframe(window: &mut WrWindowState, state: &mut WrState, rect: WrRect, clip: WrRect, layers_id: u64) {
    assert!( unsafe { is_in_compositor_thread() });

    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip.to_rect(),
                                                                     Vec::new(),
                                                                     None);
    let pipeline_id = PipelineId((layers_id >> 32) as u32, layers_id as u32);
    window.pipeline_sync_list.push(pipeline_id);
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

#[repr(C)]
pub struct WrImageMask
{
    image: ImageKey,
    rect: WrRect,
    repeat: bool
}

impl WrRect
{
    pub fn to_rect(&self) -> LayoutRect
    {
        LayoutRect::new(LayoutPoint::new(self.x, self.y), LayoutSize::new(self.width, self.height))
    }
}

#[no_mangle]
pub extern fn wr_dp_push_image(state:&mut WrState, bounds: WrRect, clip : WrRect, mask: *const WrImageMask, key: ImageKey) {
    assert!( unsafe { is_in_compositor_thread() });

    let bounds = bounds.to_rect();
    let clip = clip.to_rect();

    // convert from the C type to the Rust type
    let mask = unsafe { mask.as_ref().map(|&WrImageMask{image, ref rect,repeat}| ImageMask{image: image, rect: rect.to_rect(), repeat: repeat}) };

    let clip_region = state.frame_builder.dl_builder.new_clip_region(&clip, Vec::new(), mask);
    state.frame_builder.dl_builder.push_image(
        bounds,
        clip_region,
        bounds.size,
        bounds.size,
        ImageRendering::Auto,
        key
    );
}

#[no_mangle]
pub extern fn wr_destroy(window: &mut WrWindowState, state:*mut WrState) {
    assert!( unsafe { is_in_compositor_thread() });

    unsafe {
        window.pipeline_epoch_map.remove(&((*state).pipeline_id));
        Box::from_raw(state);
    }
}

#[no_mangle]
pub extern fn wr_readback_into_buffer(window: &mut WrWindowState, width: u32, height: u32,
                                      dst_buffer: *mut u8, buffer_size: usize) {
    assert!( unsafe { is_in_compositor_thread() });
    wr_composite_window(window);
    gl::flush();

    unsafe {
        let mut slice = slice::from_raw_parts_mut(dst_buffer, buffer_size);
        gl::read_pixels_into_buffer(0, 0,
                                    width as gl::GLsizei,
                                    height as gl::GLsizei,
                                    gl::BGRA,
                                    gl::UNSIGNED_BYTE,
                                    slice);
    }
}

#[no_mangle]
pub extern fn wr_profiler_set_enabled(window: &mut WrWindowState, enabled: bool)
{
    assert!( unsafe { is_in_compositor_thread() });
    window.renderer.set_profiler_enabled(enabled);
}

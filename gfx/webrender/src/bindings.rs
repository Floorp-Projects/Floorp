use std::path::PathBuf;
use webrender_traits::{PipelineId, AuxiliaryListsBuilder, StackingContextId, DisplayListId};
use renderer::{Renderer, RendererOptions};
extern crate webrender_traits;

use euclid::{Size2D, Point2D, Rect, Matrix4D};
use gleam::gl;
use std::ffi::CStr;
use webrender_traits::{ServoScrollRootId};
use webrender_traits::{Epoch, ColorF};
use webrender_traits::{ImageFormat, ImageKey, ImageMask, ImageRendering, RendererKind};
use std::mem;
use std::slice;
use std::os::raw::c_uchar;

#[cfg(target_os = "linux")]
mod linux {
    use std::mem;
    use std::os::raw::{c_void, c_char, c_int};
    use std::ffi::CString;

    //pub const RTLD_LAZY: c_int = 0x001;
    pub const RTLD_NOW: c_int = 0x002;

    #[link="dl"]
    extern {
        fn dlopen(filename: *const c_char, flag: c_int) -> *mut c_void;
        //fn dlerror() -> *mut c_char;
        fn dlsym(handle: *mut c_void, symbol: *const c_char) -> *mut c_void;
        fn dlclose(handle: *mut c_void) -> c_int;
    }

    pub struct Library {
        handle: *mut c_void,
        load_fun: extern "system" fn(*const u8) -> *const c_void,
    }

    impl Drop for Library {
        fn drop(&mut self) {
            unsafe { dlclose(self.handle) };
        }
    }

    impl Library {
        pub fn new() -> Library {
            let mut libglx = unsafe { dlopen(b"libGL.so.1\0".as_ptr() as *const _, RTLD_NOW) };
            if libglx.is_null() {
                libglx = unsafe { dlopen(b"libGL.so\0".as_ptr() as *const _, RTLD_NOW) };
            }
            let fun = unsafe { dlsym(libglx, b"glXGetProcAddress\0".as_ptr() as *const _) };
            Library {
                handle: libglx,
                load_fun: unsafe { mem::transmute(fun) },
            }
        }
        pub fn query(&self, name: &str) -> *const c_void {
            let string = CString::new(name).unwrap();
            let address = (self.load_fun)(string.as_ptr() as *const _);
            address as *const _
        }
    }
}

#[cfg(target_os="macos")]
mod macos {
    use std::str::FromStr;
    use std::os::raw::c_void;
    use core_foundation::base::TCFType;
    use core_foundation::string::CFString;
    use core_foundation::bundle::{CFBundleRef, CFBundleGetBundleWithIdentifier, CFBundleGetFunctionPointerForName};

    pub struct Library(CFBundleRef);

    impl Library {
        pub fn new() -> Library {
            let framework_name: CFString = FromStr::from_str("com.apple.opengl").unwrap();
            let framework = unsafe {
                CFBundleGetBundleWithIdentifier(framework_name.as_concrete_TypeRef())
            };
            Library(framework)
        }
        pub fn query(&self, name: &str) -> *const c_void {
            let symbol_name: CFString = FromStr::from_str(name).unwrap();
            let symbol = unsafe {
                CFBundleGetFunctionPointerForName(self.0, symbol_name.as_concrete_TypeRef())
            };

            if symbol.is_null() {
                println!("Could not find symbol for {:?}", symbol_name);
            }
            symbol as *const _
        }
    }
}

#[cfg(target_os="windows")]
mod win {
    use winapi;
    use kernel32;
    use std::ffi::CString;
    use std::os::raw::c_void;
    use std::mem::transmute;

    #[allow(non_camel_case_types)] // Because this is what the actual func name by MSFT is named
    type wglGetProcAddress = extern "system" fn (lpszProc : winapi::winnt::LPCSTR) -> winapi::minwindef::PROC;

    // This is a tuple struct
    pub struct Library {
        ogl_lib: winapi::HMODULE,
        ogl_ext_lib: wglGetProcAddress,
    }

    // TODO: Maybe switch this out with crate libloading for a safer alternative
    impl Library {
        pub fn new() -> Library {
            let system_ogl_lib = unsafe {
                kernel32::LoadLibraryA(b"opengl32.dll\0".as_ptr() as *const _)
            };

            if system_ogl_lib.is_null() {
                panic!("Could not load opengl32 library");
            }

            let wgl_lookup_func_name = CString::new("wglGetProcAddress").unwrap();
            let wgl_lookup_func = unsafe {
                kernel32::GetProcAddress(system_ogl_lib, wgl_lookup_func_name.as_ptr())
            };

            if wgl_lookup_func.is_null() {
                panic!("Could not load wglGetProcAddress from Opengl32.dll");
            }

            Library {
                ogl_lib: system_ogl_lib,
                ogl_ext_lib: unsafe {
                    transmute::<*const c_void, wglGetProcAddress>(wgl_lookup_func)
                },
            }
        }
        pub fn query(&self, name: &str) -> *const c_void {
            let symbol_name = CString::new(name).unwrap();
            let mut symbol = unsafe {
                // Try opengl32.dll first
                kernel32::GetProcAddress(self.ogl_lib, symbol_name.as_ptr())
            };

            if symbol.is_null() {
                // Then try through wglGetProcAddress, which is what Gecko does
                symbol = (self.ogl_ext_lib)(symbol_name.as_ptr());
            }

            // For now panic, not sure we should be though or if we can recover
            if symbol.is_null() {
                panic!("Could not find symbol {:?} in Opengl32.dll or wglGetProcAddress", symbol_name);
            }

            symbol as *const _
        }
    }
}

#[cfg(target_os = "linux")]
use self::linux::Library as GlLibrary;
#[cfg(target_os = "macos")]
use self::macos::Library as GlLibrary;
#[cfg(target_os = "windows")]
use self::win::Library as GlLibrary;

pub struct WebRenderFrameBuilder {
    pub stacking_contexts: Vec<(StackingContextId, webrender_traits::StackingContext)>,
    pub display_lists: Vec<(DisplayListId, webrender_traits::BuiltDisplayList)>,
    pub auxiliary_lists_builder: AuxiliaryListsBuilder,
    pub root_pipeline_id: PipelineId,
}

impl WebRenderFrameBuilder {
    pub fn new(root_pipeline_id: PipelineId) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            stacking_contexts: vec![],
            display_lists: vec![],
            auxiliary_lists_builder: AuxiliaryListsBuilder::new(),
            root_pipeline_id: root_pipeline_id,
        }
    }

    pub fn add_stacking_context(&mut self,
                                api: &mut webrender_traits::RenderApi,
                                pipeline_id: PipelineId,
                                stacking_context: webrender_traits::StackingContext)
                                -> StackingContextId {
        assert!(pipeline_id == self.root_pipeline_id);
        let id = api.next_stacking_context_id();
        self.stacking_contexts.push((id, stacking_context));
        id
    }

    pub fn add_display_list(&mut self,
                            api: &mut webrender_traits::RenderApi,
                            display_list: webrender_traits::BuiltDisplayList,
                            stacking_context: &mut webrender_traits::StackingContext)
                            -> DisplayListId {
        let id = api.next_display_list_id();
        stacking_context.display_lists.push(id);
        self.display_lists.push((id, display_list));
        id
    }
}

struct Notifier {
}

impl webrender_traits::RenderNotifier for Notifier {
    fn new_frame_ready(&mut self) {
    }
    fn new_scroll_frame_ready(&mut self, _: bool) {
    }

    fn pipeline_size_changed(&mut self,
                             _: PipelineId,
                             _: Option<Size2D<f32>>) {
    }
}

pub struct WrWindowState {
    renderer: Renderer,
    api: webrender_traits::RenderApi,
    _gl_library: GlLibrary,
    root_pipeline_id: PipelineId,
    size: Size2D<u32>,
}

pub struct WrState {
    size: (u32, u32),
    pipeline_id: PipelineId,
    z_index: i32,
    frame_builder: WebRenderFrameBuilder,
    dl_builder: Vec<webrender_traits::DisplayListBuilder>,
}

#[no_mangle]
pub extern fn wr_init_window(root_pipeline_id: u64) -> *mut WrWindowState {
    // hack to find the directory for the shaders
    let res_path = concat!(env!("CARGO_MANIFEST_DIR"),"/res");

    let library = GlLibrary::new();
    gl::load_with(|symbol| library.query(symbol));
    gl::clear_color(0.3, 0.0, 0.0, 1.0);

    let version = unsafe {
        let data = CStr::from_ptr(gl::GetString(gl::VERSION) as *const _).to_bytes().to_vec();
        String::from_utf8(data).unwrap()
    };

    println!("OpenGL version new {}", version);
    println!("Shader resource path: {}", res_path);

    let opts = RendererOptions {
        device_pixel_ratio: 1.0,
        resource_path: PathBuf::from(res_path),
        enable_aa: false,
        enable_subpixel_aa: false,
        enable_msaa: false,
        enable_profiler: true,
        enable_recording: false,
        enable_scrollbars: false,
        precache_shaders: false,
        renderer_kind: RendererKind::Native,
        debug: false,
    };

    let (renderer, sender) = Renderer::new(opts);
    let api = sender.create_api();

    let notifier = Box::new(Notifier{});
    renderer.set_render_notifier(notifier);

    let pipeline_id = PipelineId((root_pipeline_id >> 32) as u32, root_pipeline_id as u32);
    api.set_root_pipeline(pipeline_id);

    let state = Box::new(WrWindowState {
        renderer: renderer,
        api: api,
        _gl_library: library,
        root_pipeline_id: pipeline_id,
        size: Size2D::new(0, 0),
    });
    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_create(window: &mut WrWindowState, width: u32, height: u32, layers_id: u64) -> *mut WrState {
    let pipeline_id = PipelineId((layers_id >> 32) as u32, layers_id as u32);

    let builder = WebRenderFrameBuilder::new(pipeline_id);

    let state = Box::new(WrState {
        size: (width, height),
        pipeline_id: pipeline_id,
        z_index: 0,
        frame_builder: builder,
        dl_builder: Vec::new(),
    });

    if pipeline_id == window.root_pipeline_id {
        window.size = Size2D::new(width, height);
    }

    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_dp_begin(window: &mut WrWindowState, state: &mut WrState, width: u32, height: u32) {
    state.size = (width, height);
    state.dl_builder.clear();
    state.z_index = 0;
    state.dl_builder.push(webrender_traits::DisplayListBuilder::new());

    if state.pipeline_id == window.root_pipeline_id {
        window.size = Size2D::new(width, height);
    }
}

#[no_mangle]
pub extern fn wr_push_dl_builder(state:&mut WrState)
{
    state.dl_builder.push(webrender_traits::DisplayListBuilder::new());
}

#[no_mangle]
pub extern fn wr_pop_dl_builder(window: &mut WrWindowState, state: &mut WrState, bounds: WrRect, overflow: WrRect, transform: &Matrix4D<f32>, scroll_id: u64)
{
    // 
    state.z_index += 1;

    let pipeline_id = state.frame_builder.root_pipeline_id;
    let scroll_layer_id = if scroll_id == 0 {
        None
    } else {
        Some(webrender_traits::ScrollLayerId::new(pipeline_id,
                                                  scroll_id as usize, // WR issue 489
                                                  ServoScrollRootId(0)))
    };

    let mut sc =
        webrender_traits::StackingContext::new(scroll_layer_id,
                                               webrender_traits::ScrollPolicy::Scrollable,
                                               bounds.to_rect(),
                                               overflow.to_rect(),
                                               state.z_index,
                                               transform,
                                               &Matrix4D::identity(),
                                               false,
                                               webrender_traits::MixBlendMode::Normal,
                                               Vec::new(),
                                               &mut state.frame_builder.auxiliary_lists_builder);
    let dl = state.dl_builder.pop().unwrap();
    state.frame_builder.add_display_list(&mut window.api, dl.finalize(), &mut sc);
    let stacking_context_id = state.frame_builder.add_stacking_context(&mut window.api, pipeline_id, sc);

    state.dl_builder.last_mut().unwrap().push_stacking_context(stacking_context_id);

}

#[no_mangle]
pub extern fn wr_dp_end(window: &mut WrWindowState, state: &mut WrState) {
    let epoch = Epoch(0);
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);
    let pipeline_id = state.pipeline_id;
    let (width, height) = state.size;
    let bounds = Rect::new(Point2D::new(0.0, 0.0), Size2D::new(width as f32, height as f32));

    let mut sc =
        webrender_traits::StackingContext::new(Some(webrender_traits::ScrollLayerId::new(
                                                        pipeline_id, 0, ServoScrollRootId(0))),
                                               webrender_traits::ScrollPolicy::Scrollable,
                                               bounds,
                                               bounds,
                                               0,
                                               &Matrix4D::identity(),
                                               &Matrix4D::identity(),
                                               true,
                                               webrender_traits::MixBlendMode::Normal,
                                               Vec::new(),
                                               &mut state.frame_builder.auxiliary_lists_builder);

    assert!(state.dl_builder.len() == 1);
    let dl = state.dl_builder.pop().unwrap();
    state.frame_builder.add_display_list(&mut window.api, dl.finalize(), &mut sc);
    let sc_id = state.frame_builder.add_stacking_context(&mut window.api, pipeline_id, sc);

    let fb = mem::replace(&mut state.frame_builder, WebRenderFrameBuilder::new(pipeline_id));

    window.api.set_root_stacking_context(sc_id,
                                  root_background_color,
                                  epoch,
                                  pipeline_id,
                                  Size2D::new(width as f32, height as f32),
                                  fb.stacking_contexts,
                                  fb.display_lists,
                                  fb.auxiliary_lists_builder
                                               .finalize());

    gl::clear(gl::COLOR_BUFFER_BIT);
    window.renderer.update();

    window.renderer.render(window.size);
}

#[no_mangle]
pub extern fn wr_composite(window: &mut WrWindowState) {
    window.api.generate_frame();

    window.renderer.update();
    window.renderer.render(window.size);
}

#[no_mangle]
pub extern fn wr_add_image(window: &mut WrWindowState, width: u32, height: u32, stride: u32, format: ImageFormat, bytes: * const u8, size: usize) -> ImageKey {
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };
    let stride_option = match stride {
        0 => None,
        _ => Some(stride),
    };
    window.api.add_image(width, height, stride_option, format, bytes)
}

#[no_mangle]
pub extern fn wr_update_image(window: &mut WrWindowState, key: ImageKey, width: u32, height: u32, format: ImageFormat, bytes: * const u8, size: usize) {
    let bytes = unsafe { slice::from_raw_parts(bytes, size).to_owned() };
    window.api.update_image(key, width, height, format, bytes);
}

#[no_mangle]
pub extern fn wr_delete_image(window: &mut WrWindowState, key: ImageKey) {
    window.api.delete_image(key)
}

#[no_mangle]
pub extern fn wr_dp_push_rect(state:&mut WrState, rect: WrRect, clip: WrRect, r: f32, g: f32, b: f32, a: f32) {
    if state.dl_builder.len() == 0 {
      return;
    }
    //let (width, height) = state.size;
    let clip_region = webrender_traits::ClipRegion::new(&clip.to_rect(),
                                                        Vec::new(),
                                                        None,
                                                        &mut state.frame_builder.auxiliary_lists_builder);
    state.dl_builder.last_mut().unwrap().push_rect(rect.to_rect(),
                               clip_region,
                               ColorF::new(r, g, b, a));
}

#[no_mangle]
pub extern fn wr_dp_push_iframe(state: &mut WrState, rect: WrRect, clip: WrRect, layers_id: u64) {
    if state.dl_builder.len() == 0 {
        return;
    }

    let clip_region = webrender_traits::ClipRegion::new(&clip.to_rect(),
                                                        Vec::new(),
                                                        None,
                                                        &mut state.frame_builder.auxiliary_lists_builder);
    let pipeline_id = PipelineId((layers_id >> 32) as u32, layers_id as u32);
    state.dl_builder.last_mut().unwrap().push_iframe(rect.to_rect(),
                                clip_region, pipeline_id);
}

#[no_mangle]
pub extern fn wr_set_async_scroll(window: &mut WrWindowState, state: &mut WrState, scroll_id: u64, x: f32, y: f32) {
    let scroll_layer_id = webrender_traits::ScrollLayerId::new(
        state.frame_builder.root_pipeline_id,
        scroll_id as usize,
        ServoScrollRootId(0));
    window.api.set_scroll_offset(scroll_layer_id, Point2D::new(x, y));
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
    pub fn to_rect(&self) -> Rect<f32>
    {
        Rect::new(Point2D::new(self.x, self.y), Size2D::new(self.width, self.height))
    }
}

#[no_mangle]
pub extern fn wr_dp_push_image(state:&mut WrState, bounds: WrRect, clip : WrRect, mask: *const WrImageMask, key: ImageKey) {
    if state.dl_builder.len() == 0 {
      return;
    }
    //let (width, height) = state.size;
    let bounds = bounds.to_rect();
    let clip = clip.to_rect();

    // convert from the C type to the Rust type
    let mask = unsafe { mask.as_ref().map(|&WrImageMask{image, ref rect,repeat}| ImageMask{image: image, rect: rect.to_rect(), repeat: repeat}) };

    let clip_region = webrender_traits::ClipRegion::new(&clip,
                                                        Vec::new(),
                                                        mask,
                                                        &mut state.frame_builder.auxiliary_lists_builder);
    let rect = bounds;
    state.dl_builder.last_mut().unwrap().push_image(rect,
                               clip_region,
                               rect.size,
                               rect.size,
                               ImageRendering::Auto,
                               key);
}

#[no_mangle]
pub extern fn wr_destroy(state:*mut WrState) {
  unsafe {
    Box::from_raw(state);
  }
}

#[no_mangle]
// read the function definition to make sure we free this memory correctly.
pub extern fn wr_readback_buffer(width: u32, height: u32, out_length: *mut u32, out_capacity: *mut u32) -> *const c_uchar {
    gl::flush();
    let mut pixels = gl::read_pixels(0, 0,
                                 width as gl::GLsizei,
                                 height as gl::GLsizei,
                                 gl::BGRA,
                                 gl::UNSIGNED_BYTE);
    let pointer = pixels.as_mut_ptr();
    unsafe {
        *out_length = pixels.len() as u32;
        *out_capacity = pixels.capacity() as u32;
        mem::forget(pixels); // Ensure rust doesn't clean this up.
    }
    return pointer;
}

#[no_mangle]
pub extern fn wr_free_buffer(vec_ptr: *mut c_uchar, length: u32, capacity: u32)
{
    // note that vec_ptr loses its const here because we're doing unsafe things.
    unsafe {
        let rebuilt = Vec::from_raw_parts(vec_ptr, length as usize, capacity as usize);
    }
}
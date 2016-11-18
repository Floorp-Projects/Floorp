use std::path::PathBuf;
use std::ffi::CStr;
use std::{mem, slice};
use std::os::raw::c_uchar;
use gleam::gl;
use euclid::{Size2D, Point2D, Rect, Matrix4D};
use webrender_traits::{PipelineId, AuxiliaryListsBuilder};
use webrender_traits::{ServoScrollRootId};
use webrender_traits::{Epoch, ColorF};
use webrender_traits::{ImageData, ImageFormat, ImageKey, ImageMask, ImageRendering, RendererKind};
use webrender::renderer::{Renderer, RendererOptions};
extern crate webrender_traits;

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
    pub auxiliary_lists_builder: AuxiliaryListsBuilder,
    pub root_pipeline_id: PipelineId,
    pub root_dl_builder: webrender_traits::DisplayListBuilder,
    pub dl_builder: Vec<webrender_traits::DisplayListBuilder>,
}

impl WebRenderFrameBuilder {
    pub fn new(root_pipeline_id: PipelineId) -> WebRenderFrameBuilder {
        WebRenderFrameBuilder {
            auxiliary_lists_builder: AuxiliaryListsBuilder::new(),
            root_pipeline_id: root_pipeline_id,
            root_dl_builder: webrender_traits::DisplayListBuilder::new(),
            dl_builder: vec![],
        }
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
}

#[no_mangle]
pub extern fn wr_init_window(root_pipeline_id: u64) -> *mut WrWindowState {
    let library = GlLibrary::new();
    gl::load_with(|symbol| library.query(symbol));
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
        enable_profiler: false,
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
    });

    if pipeline_id == window.root_pipeline_id {
        window.size = Size2D::new(width, height);
    }

    Box::into_raw(state)
}

#[no_mangle]
pub extern fn wr_dp_begin(window: &mut WrWindowState, state: &mut WrState, width: u32, height: u32) {
    state.size = (width, height);
    state.frame_builder.root_dl_builder.list.clear();
    state.frame_builder.dl_builder.clear();
    state.z_index = 0;

    if state.pipeline_id == window.root_pipeline_id {
        window.size = Size2D::new(width, height);
    }

    let bounds = Rect::new(Point2D::new(0.0, 0.0), Size2D::new(width as f32, height as f32));

    let root_stacking_context =
        webrender_traits::StackingContext::new(Some(webrender_traits::ScrollLayerId::new(
                                                    state.pipeline_id, 0, ServoScrollRootId(0))),
                                               webrender_traits::ScrollPolicy::Scrollable,
                                               bounds,
                                               bounds,
                                               0,
                                               &Matrix4D::identity(),
                                               &Matrix4D::identity(),
                                               webrender_traits::MixBlendMode::Normal,
                                               Vec::new(),
                                               &mut state.frame_builder.auxiliary_lists_builder);

    state.frame_builder.root_dl_builder.push_stacking_context(root_stacking_context);
}

#[no_mangle]
pub extern fn wr_push_dl_builder(state:&mut WrState)
{
    state.frame_builder.dl_builder.push(webrender_traits::DisplayListBuilder::new());
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

    let sc =
        webrender_traits::StackingContext::new(scroll_layer_id,
                                               webrender_traits::ScrollPolicy::Scrollable,
                                               bounds.to_rect(),
                                               overflow.to_rect(),
                                               state.z_index,
                                               transform,
                                               &Matrix4D::identity(),
                                               webrender_traits::MixBlendMode::Normal,
                                               Vec::new(),
                                               &mut state.frame_builder.auxiliary_lists_builder);

    state.frame_builder.root_dl_builder.push_stacking_context(sc);
    assert!(state.frame_builder.root_dl_builder.list.len() != 0);

    let mut display_list = state.frame_builder.dl_builder.pop().unwrap();
    state.frame_builder.root_dl_builder.list.append(&mut display_list.list);

    state.frame_builder.root_dl_builder.pop_stacking_context();
}

#[no_mangle]
pub extern fn wr_dp_end(window: &mut WrWindowState, state: &mut WrState) {
    let epoch = Epoch(0);
    let root_background_color = ColorF::new(0.3, 0.0, 0.0, 1.0);
    let pipeline_id = state.pipeline_id;
    let (width, height) = state.size;

    // Should be the root one
    state.frame_builder.root_dl_builder.pop_stacking_context();

    let fb = mem::replace(&mut state.frame_builder, WebRenderFrameBuilder::new(pipeline_id));

    window.api.set_root_display_list(root_background_color,
                                     epoch,
                                     pipeline_id,
                                     Size2D::new(width as f32, height as f32),
                                     fb.root_dl_builder.finalize(),
                                     fb.auxiliary_lists_builder.finalize()
                                     );

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

    window.api.add_image(width, height, stride_option, format, ImageData::new(bytes))
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
    if state.frame_builder.dl_builder.is_empty() {
        return;
    }
    //let (width, height) = state.size;
    let clip_region = webrender_traits::ClipRegion::new(&clip.to_rect(),
                                                        Vec::new(),
                                                        None,
                                                        &mut state.frame_builder.auxiliary_lists_builder);

    state.frame_builder.dl_builder.last_mut().unwrap().push_rect(
                                    rect.to_rect(),
                                    clip_region,
                                    ColorF::new(r, g, b, a)
                                    );
}

#[no_mangle]
pub extern fn wr_dp_push_iframe(state: &mut WrState, rect: WrRect, clip: WrRect, layers_id: u64) {
    if state.frame_builder.dl_builder.is_empty() {
        return;
    }

    let clip_region = webrender_traits::ClipRegion::new(&clip.to_rect(),
                                                        Vec::new(),
                                                        None,
                                                        &mut state.frame_builder.auxiliary_lists_builder);
    let pipeline_id = PipelineId((layers_id >> 32) as u32, layers_id as u32);
    state.frame_builder.dl_builder.last_mut().unwrap().push_iframe(
                                                        rect.to_rect(),
                                                        clip_region,
                                                        pipeline_id
                                                        );
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
    if state.frame_builder.dl_builder.is_empty() {
        return;
    }
    let bounds = bounds.to_rect();
    let clip = clip.to_rect();

    // convert from the C type to the Rust type
    let mask = unsafe { mask.as_ref().map(|&WrImageMask{image, ref rect,repeat}| ImageMask{image: image, rect: rect.to_rect(), repeat: repeat}) };

    let clip_region = webrender_traits::ClipRegion::new(&clip,
                                                        Vec::new(),
                                                        mask,
                                                        &mut state.frame_builder.auxiliary_lists_builder);
    let rect = bounds;
    state.frame_builder.dl_builder.last_mut().unwrap().push_image(rect,
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

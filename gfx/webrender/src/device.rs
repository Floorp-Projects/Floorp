/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::shader_source;
use api::{ColorF, ImageFormat};
use api::{DeviceIntPoint, DeviceIntRect, DeviceUintRect, DeviceUintSize};
use api::TextureTarget;
#[cfg(any(feature = "debug_renderer", feature="capture"))]
use api::ImageDescriptor;
use euclid::Transform3D;
use gleam::gl;
use internal_types::{FastHashMap, RenderTargetInfo};
use log::Level;
use smallvec::SmallVec;
use std::cell::RefCell;
use std::fs::File;
use std::io::Read;
use std::marker::PhantomData;
use std::mem;
use std::ops::Add;
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;
use std::slice;
use std::thread;

#[derive(Debug, Copy, Clone, PartialEq, Ord, Eq, PartialOrd)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameId(usize);

impl FrameId {
    pub fn new(value: usize) -> Self {
        FrameId(value)
    }
}

impl Add<usize> for FrameId {
    type Output = FrameId;

    fn add(self, other: usize) -> FrameId {
        FrameId(self.0 + other)
    }
}

const GL_FORMAT_BGRA_GL: gl::GLuint = gl::BGRA;

const GL_FORMAT_BGRA_GLES: gl::GLuint = gl::BGRA_EXT;

const SHADER_VERSION_GL: &str = "#version 150\n";
const SHADER_VERSION_GLES: &str = "#version 300 es\n";

const SHADER_KIND_VERTEX: &str = "#define WR_VERTEX_SHADER\n";
const SHADER_KIND_FRAGMENT: &str = "#define WR_FRAGMENT_SHADER\n";
const SHADER_IMPORT: &str = "#include ";

pub struct TextureSlot(pub usize);

// In some places we need to temporarily bind a texture to any slot.
const DEFAULT_TEXTURE: TextureSlot = TextureSlot(0);

#[repr(u32)]
pub enum DepthFunction {
    #[cfg(feature = "debug_renderer")]
    Less = gl::LESS,
    LessEqual = gl::LEQUAL,
}

#[derive(Copy, Clone, Debug, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub enum TextureFilter {
    Nearest,
    Linear,
    Trilinear,
}

#[derive(Debug)]
pub enum VertexAttributeKind {
    F32,
    #[cfg(feature = "debug_renderer")]
    U8Norm,
    U16Norm,
    I32,
    U16,
}

#[derive(Debug)]
pub struct VertexAttribute {
    pub name: &'static str,
    pub count: u32,
    pub kind: VertexAttributeKind,
}

#[derive(Debug)]
pub struct VertexDescriptor {
    pub vertex_attributes: &'static [VertexAttribute],
    pub instance_attributes: &'static [VertexAttribute],
}

enum FBOTarget {
    Read,
    Draw,
}

/// Method of uploading texel data from CPU to GPU.
#[derive(Debug, Clone)]
pub enum UploadMethod {
    /// Just call `glTexSubImage` directly with the CPU data pointer
    Immediate,
    /// Accumulate the changes in PBO first before transferring to a texture.
    PixelBuffer(VertexUsageHint),
}

/// Plain old data that can be used to initialize a texture.
pub unsafe trait Texel: Copy {}
unsafe impl Texel for u8 {}
unsafe impl Texel for f32 {}

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum ReadPixelsFormat {
    Standard(ImageFormat),
    Rgba8,
}

pub fn get_gl_target(target: TextureTarget) -> gl::GLuint {
    match target {
        TextureTarget::Default => gl::TEXTURE_2D,
        TextureTarget::Array => gl::TEXTURE_2D_ARRAY,
        TextureTarget::Rect => gl::TEXTURE_RECTANGLE,
        TextureTarget::External => gl::TEXTURE_EXTERNAL_OES,
    }
}

pub fn get_gl_format_bgra(gl: &gl::Gl) -> gl::GLuint {
    match gl.get_type() {
        gl::GlType::Gl => GL_FORMAT_BGRA_GL,
        gl::GlType::Gles => GL_FORMAT_BGRA_GLES,
    }
}

fn get_shader_version(gl: &gl::Gl) -> &'static str {
    match gl.get_type() {
        gl::GlType::Gl => SHADER_VERSION_GL,
        gl::GlType::Gles => SHADER_VERSION_GLES,
    }
}

// Get a shader string by name, from the built in resources or
// an override path, if supplied.
fn get_shader_source(shader_name: &str, base_path: &Option<PathBuf>) -> Option<String> {
    if let Some(ref base) = *base_path {
        let shader_path = base.join(&format!("{}.glsl", shader_name));
        if shader_path.exists() {
            let mut source = String::new();
            File::open(&shader_path)
                .unwrap()
                .read_to_string(&mut source)
                .unwrap();
            return Some(source);
        }
    }

    shader_source::SHADERS
        .get(shader_name)
        .map(|s| s.to_string())
}

// Parse a shader string for imports. Imports are recursively processed, and
// prepended to the list of outputs.
fn parse_shader_source(source: String, base_path: &Option<PathBuf>, output: &mut String) {
    for line in source.lines() {
        if line.starts_with(SHADER_IMPORT) {
            let imports = line[SHADER_IMPORT.len() ..].split(',');

            // For each import, get the source, and recurse.
            for import in imports {
                if let Some(include) = get_shader_source(import, base_path) {
                    parse_shader_source(include, base_path, output);
                }
            }
        } else {
            output.push_str(line);
            output.push_str("\n");
        }
    }
}

pub fn build_shader_strings(
    gl_version_string: &str,
    features: &str,
    base_filename: &str,
    override_path: &Option<PathBuf>,
) -> (String, String) {
    // Construct a list of strings to be passed to the shader compiler.
    let mut vs_source = String::new();
    let mut fs_source = String::new();

    // GLSL requires that the version number comes first.
    vs_source.push_str(gl_version_string);
    fs_source.push_str(gl_version_string);

    // Insert the shader name to make debugging easier.
    let name_string = format!("// {}\n", base_filename);
    vs_source.push_str(&name_string);
    fs_source.push_str(&name_string);

    // Define a constant depending on whether we are compiling VS or FS.
    vs_source.push_str(SHADER_KIND_VERTEX);
    fs_source.push_str(SHADER_KIND_FRAGMENT);

    // Add any defines that were passed by the caller.
    vs_source.push_str(features);
    fs_source.push_str(features);

    // Parse the main .glsl file, including any imports
    // and append them to the list of sources.
    let mut shared_result = String::new();
    if let Some(shared_source) = get_shader_source(base_filename, override_path) {
        parse_shader_source(shared_source, override_path, &mut shared_result);
    }

    vs_source.push_str(&shared_result);
    fs_source.push_str(&shared_result);

    (vs_source, fs_source)
}

pub trait FileWatcherHandler: Send {
    fn file_changed(&self, path: PathBuf);
}

impl VertexAttributeKind {
    fn size_in_bytes(&self) -> u32 {
        match *self {
            VertexAttributeKind::F32 => 4,
            #[cfg(feature = "debug_renderer")]
            VertexAttributeKind::U8Norm => 1,
            VertexAttributeKind::U16Norm => 2,
            VertexAttributeKind::I32 => 4,
            VertexAttributeKind::U16 => 2,
        }
    }
}

impl VertexAttribute {
    fn size_in_bytes(&self) -> u32 {
        self.count * self.kind.size_in_bytes()
    }

    fn bind_to_vao(
        &self,
        attr_index: gl::GLuint,
        divisor: gl::GLuint,
        stride: gl::GLint,
        offset: gl::GLuint,
        gl: &gl::Gl,
    ) {
        gl.enable_vertex_attrib_array(attr_index);
        gl.vertex_attrib_divisor(attr_index, divisor);

        match self.kind {
            VertexAttributeKind::F32 => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::FLOAT,
                    false,
                    stride,
                    offset,
                );
            }
            #[cfg(feature = "debug_renderer")]
            VertexAttributeKind::U8Norm => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_BYTE,
                    true,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::U16Norm => {
                gl.vertex_attrib_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_SHORT,
                    true,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::I32 => {
                gl.vertex_attrib_i_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::INT,
                    stride,
                    offset,
                );
            }
            VertexAttributeKind::U16 => {
                gl.vertex_attrib_i_pointer(
                    attr_index,
                    self.count as gl::GLint,
                    gl::UNSIGNED_SHORT,
                    stride,
                    offset,
                );
            }
        }
    }
}

impl VertexDescriptor {
    fn instance_stride(&self) -> u32 {
        self.instance_attributes
            .iter()
            .map(|attr| attr.size_in_bytes())
            .sum()
    }

    fn bind_attributes(
        attributes: &[VertexAttribute],
        start_index: usize,
        divisor: u32,
        gl: &gl::Gl,
        vbo: VBOId,
    ) {
        vbo.bind(gl);

        let stride: u32 = attributes
            .iter()
            .map(|attr| attr.size_in_bytes())
            .sum();

        let mut offset = 0;
        for (i, attr) in attributes.iter().enumerate() {
            let attr_index = (start_index + i) as gl::GLuint;
            attr.bind_to_vao(attr_index, divisor, stride as _, offset, gl);
            offset += attr.size_in_bytes();
        }
    }

    fn bind(&self, gl: &gl::Gl, main: VBOId, instance: VBOId) {
        Self::bind_attributes(self.vertex_attributes, 0, 0, gl, main);

        if !self.instance_attributes.is_empty() {
            Self::bind_attributes(
                self.instance_attributes,
                self.vertex_attributes.len(),
                1, gl, instance,
            );
        }
    }
}

impl VBOId {
    fn bind(&self, gl: &gl::Gl) {
        gl.bind_buffer(gl::ARRAY_BUFFER, self.0);
    }
}

impl IBOId {
    fn bind(&self, gl: &gl::Gl) {
        gl.bind_buffer(gl::ELEMENT_ARRAY_BUFFER, self.0);
    }
}

impl FBOId {
    fn bind(&self, gl: &gl::Gl, target: FBOTarget) {
        let target = match target {
            FBOTarget::Read => gl::READ_FRAMEBUFFER,
            FBOTarget::Draw => gl::DRAW_FRAMEBUFFER,
        };
        gl.bind_framebuffer(target, self.0);
    }
}

pub struct Stream<'a> {
    attributes: &'a [VertexAttribute],
    vbo: VBOId,
}

pub struct VBO<V> {
    id: gl::GLuint,
    target: gl::GLenum,
    allocated_count: usize,
    marker: PhantomData<V>,
}

impl<V> VBO<V> {
    pub fn allocated_count(&self) -> usize {
        self.allocated_count
    }

    pub fn stream_with<'a>(&self, attributes: &'a [VertexAttribute]) -> Stream<'a> {
        debug_assert_eq!(
            mem::size_of::<V>(),
            attributes.iter().map(|a| a.size_in_bytes() as usize).sum::<usize>()
        );
        Stream {
            attributes,
            vbo: VBOId(self.id),
        }
    }
}

impl<T> Drop for VBO<T> {
    fn drop(&mut self) {
        debug_assert!(thread::panicking() || self.id == 0);
    }
}

#[cfg_attr(feature = "replay", derive(Clone))]
pub struct ExternalTexture {
    id: gl::GLuint,
    target: gl::GLuint,
}

impl ExternalTexture {
    pub fn new(id: u32, target: TextureTarget) -> Self {
        ExternalTexture {
            id,
            target: get_gl_target(target),
        }
    }

    #[cfg(feature = "replay")]
    pub fn internal_id(&self) -> gl::GLuint {
        self.id
    }
}

pub struct Texture {
    id: gl::GLuint,
    target: gl::GLuint,
    layer_count: i32,
    format: ImageFormat,
    width: u32,
    height: u32,
    filter: TextureFilter,
    render_target: Option<RenderTargetInfo>,
    fbo_ids: Vec<FBOId>,
    depth_rb: Option<RBOId>,
    last_frame_used: FrameId,
}

impl Texture {
    pub fn get_dimensions(&self) -> DeviceUintSize {
        DeviceUintSize::new(self.width, self.height)
    }

    pub fn get_render_target_layer_count(&self) -> usize {
        self.fbo_ids.len()
    }

    pub fn get_layer_count(&self) -> i32 {
        self.layer_count
    }

    pub fn get_format(&self) -> ImageFormat {
        self.format
    }

    #[cfg(any(feature = "debug_renderer", feature = "capture"))]
    pub fn get_filter(&self) -> TextureFilter {
        self.filter
    }

    #[cfg(any(feature = "debug_renderer", feature = "capture"))]
    pub fn get_render_target(&self) -> Option<RenderTargetInfo> {
        self.render_target.clone()
    }

    pub fn has_depth(&self) -> bool {
        self.depth_rb.is_some()
    }

    pub fn get_rt_info(&self) -> Option<&RenderTargetInfo> {
        self.render_target.as_ref()
    }

    pub fn used_in_frame(&self, frame_id: FrameId) -> bool {
        self.last_frame_used == frame_id
    }

    #[cfg(feature = "replay")]
    pub fn into_external(mut self) -> ExternalTexture {
        let ext = ExternalTexture {
            id: self.id,
            target: self.target,
        };
        self.id = 0; // don't complain, moved out
        ext
    }
}

impl Drop for Texture {
    fn drop(&mut self) {
        debug_assert!(thread::panicking() || self.id == 0);
    }
}

pub struct Program {
    id: gl::GLuint,
    u_transform: gl::GLint,
    u_device_pixel_ratio: gl::GLint,
    u_mode: gl::GLint,
}

impl Drop for Program {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct CustomVAO {
    id: gl::GLuint,
}

impl Drop for CustomVAO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct VAO {
    id: gl::GLuint,
    ibo_id: IBOId,
    main_vbo_id: VBOId,
    instance_vbo_id: VBOId,
    instance_stride: usize,
    owns_vertices_and_indices: bool,
}

impl Drop for VAO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

pub struct PBO {
    id: gl::GLuint,
}

impl Drop for PBO {
    fn drop(&mut self) {
        debug_assert!(
            thread::panicking() || self.id == 0,
            "renderer::deinit not called"
        );
    }
}

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct FBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct RBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct VBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
struct IBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug)]
pub struct ProgramSources {
    renderer_name: String,
    vs_source: String,
    fs_source: String,
}

impl ProgramSources {
    fn new(renderer_name: String, vs_source: String, fs_source: String) -> Self {
        ProgramSources {
            renderer_name,
            vs_source,
            fs_source,
        }
    }
}

pub struct ProgramBinary {
    binary: Vec<u8>,
    format: gl::GLenum,
}

impl ProgramBinary {
    fn new(binary: Vec<u8>, format: gl::GLenum) -> Self {
        ProgramBinary {
            binary,
            format
        }
    }
}

pub struct ProgramCache {
    pub binaries: RefCell<FastHashMap<ProgramSources, ProgramBinary>>,
}

impl ProgramCache {
    pub fn new() -> Rc<Self> {
        Rc::new(
            ProgramCache {
                binaries: RefCell::new(FastHashMap::default()),
            }
        )
    }
}

#[derive(Debug, Copy, Clone)]
pub enum VertexUsageHint {
    Static,
    Dynamic,
    Stream,
}

impl VertexUsageHint {
    fn to_gl(&self) -> gl::GLuint {
        match *self {
            VertexUsageHint::Static => gl::STATIC_DRAW,
            VertexUsageHint::Dynamic => gl::DYNAMIC_DRAW,
            VertexUsageHint::Stream => gl::STREAM_DRAW,
        }
    }
}

#[derive(Copy, Clone, Debug)]
pub struct UniformLocation(gl::GLint);

impl UniformLocation {
    pub const INVALID: Self = UniformLocation(-1);
}

#[cfg(feature = "debug_renderer")]
pub struct Capabilities {
    pub supports_multisampling: bool,
}

#[derive(Clone, Debug)]
pub enum ShaderError {
    Compilation(String, String), // name, error message
    Link(String, String),        // name, error message
}

pub struct Device {
    gl: Rc<gl::Gl>,
    // device state
    bound_textures: [gl::GLuint; 16],
    bound_program: gl::GLuint,
    bound_vao: gl::GLuint,
    bound_read_fbo: FBOId,
    bound_draw_fbo: FBOId,
    program_mode_id: UniformLocation,
    default_read_fbo: gl::GLuint,
    default_draw_fbo: gl::GLuint,

    device_pixel_ratio: f32,
    upload_method: UploadMethod,

    // HW or API capabilities
    #[cfg(feature = "debug_renderer")]
    capabilities: Capabilities,

    // debug
    inside_frame: bool,

    // resources
    resource_override_path: Option<PathBuf>,

    max_texture_size: u32,
    renderer_name: String,
    cached_programs: Option<Rc<ProgramCache>>,

    // Frame counter. This is used to map between CPU
    // frames and GPU frames.
    frame_id: FrameId,

    // GL extensions
    extensions: Vec<String>,
}

impl Device {
    pub fn new(
        gl: Rc<gl::Gl>,
        resource_override_path: Option<PathBuf>,
        upload_method: UploadMethod,
        _file_changed_handler: Box<FileWatcherHandler>,
        cached_programs: Option<Rc<ProgramCache>>,
    ) -> Device {
        let max_texture_size = gl.get_integer_v(gl::MAX_TEXTURE_SIZE) as u32;
        let renderer_name = gl.get_string(gl::RENDERER);

        let mut extensions = Vec::new();
        let extension_count = gl.get_integer_v(gl::NUM_EXTENSIONS) as gl::GLuint;
        for i in 0 .. extension_count {
            extensions.push(gl.get_string_i(gl::EXTENSIONS, i));
        }

        Device {
            gl,
            resource_override_path,
            // This is initialized to 1 by default, but it is reset
            // at the beginning of each frame in `Renderer::bind_frame_data`.
            device_pixel_ratio: 1.0,
            upload_method,
            inside_frame: false,

            #[cfg(feature = "debug_renderer")]
            capabilities: Capabilities {
                supports_multisampling: false, //TODO
            },

            bound_textures: [0; 16],
            bound_program: 0,
            bound_vao: 0,
            bound_read_fbo: FBOId(0),
            bound_draw_fbo: FBOId(0),
            program_mode_id: UniformLocation::INVALID,
            default_read_fbo: 0,
            default_draw_fbo: 0,

            max_texture_size,
            renderer_name,
            cached_programs,
            frame_id: FrameId(0),
            extensions,
        }
    }

    pub fn gl(&self) -> &gl::Gl {
        &*self.gl
    }

    pub fn rc_gl(&self) -> &Rc<gl::Gl> {
        &self.gl
    }

    pub fn set_device_pixel_ratio(&mut self, ratio: f32) {
        self.device_pixel_ratio = ratio;
    }

    pub fn update_program_cache(&mut self, cached_programs: Rc<ProgramCache>) {
        self.cached_programs = Some(cached_programs);
    }

    pub fn max_texture_size(&self) -> u32 {
        self.max_texture_size
    }

    #[cfg(feature = "debug_renderer")]
    pub fn get_capabilities(&self) -> &Capabilities {
        &self.capabilities
    }

    pub fn reset_state(&mut self) {
        self.bound_textures = [0; 16];
        self.bound_vao = 0;
        self.bound_read_fbo = FBOId(0);
        self.bound_draw_fbo = FBOId(0);
    }

    #[cfg(debug_assertions)]
    fn print_shader_errors(source: &str, log: &str) {
        // hacky way to extract the offending lines
        if !log.starts_with("0:") {
            return;
        }
        let end_pos = match log[2..].chars().position(|c| !c.is_digit(10)) {
            Some(pos) => 2 + pos,
            None => return,
        };
        let base_line_number = match log[2 .. end_pos].parse::<usize>() {
            Ok(number) if number >= 2 => number - 2,
            _ => return,
        };
        for (line, prefix) in source.lines().skip(base_line_number).zip(&["|",">","|"]) {
            error!("{}\t{}", prefix, line);
        }
    }

    pub fn compile_shader(
        gl: &gl::Gl,
        name: &str,
        shader_type: gl::GLenum,
        source: &String,
    ) -> Result<gl::GLuint, ShaderError> {
        debug!("compile {}", name);
        let id = gl.create_shader(shader_type);
        gl.shader_source(id, &[source.as_bytes()]);
        gl.compile_shader(id);
        let log = gl.get_shader_info_log(id);
        if gl.get_shader_iv(id, gl::COMPILE_STATUS) == (0 as gl::GLint) {
            error!("Failed to compile shader: {}\n{}", name, log);
            #[cfg(debug_assertions)]
            Self::print_shader_errors(source, &log);
            Err(ShaderError::Compilation(name.to_string(), log))
        } else {
            if !log.is_empty() {
                warn!("Warnings detected on shader: {}\n{}", name, log);
            }
            Ok(id)
        }
    }

    pub fn begin_frame(&mut self) -> FrameId {
        debug_assert!(!self.inside_frame);
        self.inside_frame = true;

        // Retrieve the currently set FBO.
        let default_read_fbo = self.gl.get_integer_v(gl::READ_FRAMEBUFFER_BINDING);
        self.default_read_fbo = default_read_fbo as gl::GLuint;
        let default_draw_fbo = self.gl.get_integer_v(gl::DRAW_FRAMEBUFFER_BINDING);
        self.default_draw_fbo = default_draw_fbo as gl::GLuint;

        // Texture state
        for i in 0 .. self.bound_textures.len() {
            self.bound_textures[i] = 0;
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        // Shader state
        self.bound_program = 0;
        self.program_mode_id = UniformLocation::INVALID;
        self.gl.use_program(0);

        // Vertex state
        self.bound_vao = 0;
        self.gl.bind_vertex_array(0);

        // FBO state
        self.bound_read_fbo = FBOId(self.default_read_fbo);
        self.bound_draw_fbo = FBOId(self.default_draw_fbo);

        // Pixel op state
        self.gl.pixel_store_i(gl::UNPACK_ALIGNMENT, 1);
        self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);

        // Default is sampler 0, always
        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id
    }

    fn bind_texture_impl(&mut self, slot: TextureSlot, id: gl::GLuint, target: gl::GLenum) {
        debug_assert!(self.inside_frame);

        if self.bound_textures[slot.0] != id {
            self.bound_textures[slot.0] = id;
            self.gl.active_texture(gl::TEXTURE0 + slot.0 as gl::GLuint);
            self.gl.bind_texture(target, id);
            self.gl.active_texture(gl::TEXTURE0);
        }
    }

    pub fn bind_texture<S>(&mut self, sampler: S, texture: &Texture)
    where
        S: Into<TextureSlot>,
    {
        self.bind_texture_impl(sampler.into(), texture.id, texture.target);
    }

    pub fn bind_external_texture<S>(&mut self, sampler: S, external_texture: &ExternalTexture)
    where
        S: Into<TextureSlot>,
    {
        self.bind_texture_impl(sampler.into(), external_texture.id, external_texture.target);
    }

    pub fn bind_read_target_impl(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_read_fbo != fbo_id {
            self.bound_read_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Read);
        }
    }

    pub fn bind_read_target(&mut self, texture_and_layer: Option<(&Texture, i32)>) {
        let fbo_id = texture_and_layer.map_or(FBOId(self.default_read_fbo), |texture_and_layer| {
            texture_and_layer.0.fbo_ids[texture_and_layer.1 as usize]
        });

        self.bind_read_target_impl(fbo_id)
    }

    fn bind_draw_target_impl(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }
    }

    pub fn bind_draw_target(
        &mut self,
        texture_and_layer: Option<(&Texture, i32)>,
        dimensions: Option<DeviceUintSize>,
    ) {
        let fbo_id = texture_and_layer.map_or(FBOId(self.default_draw_fbo), |texture_and_layer| {
            texture_and_layer.0.fbo_ids[texture_and_layer.1 as usize]
        });

        self.bind_draw_target_impl(fbo_id);

        if let Some(dimensions) = dimensions {
            self.gl.viewport(
                0,
                0,
                dimensions.width as _,
                dimensions.height as _,
            );
        }
    }

    pub fn create_fbo_for_external_texture(&mut self, texture_id: u32) -> FBOId {
        let fbo = FBOId(self.gl.gen_framebuffers(1)[0]);
        fbo.bind(self.gl(), FBOTarget::Draw);
        self.gl.framebuffer_texture_2d(
            gl::DRAW_FRAMEBUFFER,
            gl::COLOR_ATTACHMENT0,
            gl::TEXTURE_2D,
            texture_id,
            0,
        );
        self.bound_draw_fbo.bind(self.gl(), FBOTarget::Draw);
        fbo
    }

    pub fn delete_fbo(&mut self, fbo: FBOId) {
        self.gl.delete_framebuffers(&[fbo.0]);
    }

    pub fn bind_external_draw_target(&mut self, fbo_id: FBOId) {
        debug_assert!(self.inside_frame);

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }
    }

    pub fn bind_program(&mut self, program: &Program) {
        debug_assert!(self.inside_frame);

        if self.bound_program != program.id {
            self.gl.use_program(program.id);
            self.bound_program = program.id;
            self.program_mode_id = UniformLocation(program.u_mode);
        }
    }

    pub fn create_texture(
        &mut self,
        target: TextureTarget,
        format: ImageFormat,
    ) -> Texture {
        Texture {
            id: self.gl.gen_textures(1)[0],
            target: get_gl_target(target),
            width: 0,
            height: 0,
            layer_count: 0,
            format,
            filter: TextureFilter::Nearest,
            render_target: None,
            fbo_ids: vec![],
            depth_rb: None,
            last_frame_used: self.frame_id,
        }
    }

    fn set_texture_parameters(&mut self, target: gl::GLuint, filter: TextureFilter) {
        let mag_filter = match filter {
            TextureFilter::Nearest => gl::NEAREST,
            TextureFilter::Linear | TextureFilter::Trilinear => gl::LINEAR,
        };

        let min_filter = match filter {
            TextureFilter::Nearest => gl::NEAREST,
            TextureFilter::Linear => gl::LINEAR,
            TextureFilter::Trilinear => gl::LINEAR_MIPMAP_LINEAR,
        };

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MAG_FILTER, mag_filter as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MIN_FILTER, min_filter as gl::GLint);

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
    }

    /// Resizes a texture with enabled render target views,
    /// preserves the data by blitting the old texture contents over.
    pub fn resize_renderable_texture(
        &mut self,
        texture: &mut Texture,
        new_size: DeviceUintSize,
    ) {
        debug_assert!(self.inside_frame);

        let old_size = texture.get_dimensions();
        let old_fbos = mem::replace(&mut texture.fbo_ids, Vec::new());
        let old_texture_id = mem::replace(&mut texture.id, self.gl.gen_textures(1)[0]);

        texture.width = new_size.width;
        texture.height = new_size.height;
        let rt_info = texture.render_target
            .clone()
            .expect("Only renderable textures are expected for resize here");

        self.bind_texture(DEFAULT_TEXTURE, texture);
        self.set_texture_parameters(texture.target, texture.filter);
        self.update_target_storage::<u8>(texture, &rt_info, true, None);

        let rect = DeviceIntRect::new(DeviceIntPoint::zero(), old_size.to_i32());
        for (read_fbo, &draw_fbo) in old_fbos.into_iter().zip(&texture.fbo_ids) {
            self.bind_read_target_impl(read_fbo);
            self.bind_draw_target_impl(draw_fbo);
            self.blit_render_target(rect, rect);
            self.delete_fbo(read_fbo);
        }
        self.gl.delete_textures(&[old_texture_id]);
        self.bind_read_target(None);
    }

    pub fn init_texture<T: Texel>(
        &mut self,
        texture: &mut Texture,
        mut width: u32,
        mut height: u32,
        filter: TextureFilter,
        render_target: Option<RenderTargetInfo>,
        layer_count: i32,
        pixels: Option<&[T]>,
    ) {
        debug_assert!(self.inside_frame);

        if width > self.max_texture_size || height > self.max_texture_size {
            error!("Attempting to allocate a texture of size {}x{} above the limit, trimming", width, height);
            width = width.min(self.max_texture_size);
            height = height.min(self.max_texture_size);
        }

        let is_resized = texture.width != width || texture.height != height;

        texture.width = width;
        texture.height = height;
        texture.filter = filter;
        texture.layer_count = layer_count;
        texture.render_target = render_target;
        texture.last_frame_used = self.frame_id;

        self.bind_texture(DEFAULT_TEXTURE, texture);
        self.set_texture_parameters(texture.target, filter);

        match render_target {
            Some(info) => {
                self.update_target_storage(texture, &info, is_resized, pixels);
            }
            None => {
                self.update_texture_storage(texture, pixels);
            }
        }
    }

    /// Updates the render target storage for the texture, creating FBOs as required.
    fn update_target_storage<T: Texel>(
        &mut self,
        texture: &mut Texture,
        rt_info: &RenderTargetInfo,
        is_resized: bool,
        pixels: Option<&[T]>,
    ) {
        assert!(texture.layer_count > 0 || texture.width + texture.height == 0);

        let needed_layer_count = texture.layer_count - texture.fbo_ids.len() as i32;
        let allocate_color = needed_layer_count != 0 || is_resized || pixels.is_some();

        if allocate_color {
            let desc = gl_describe_format(self.gl(), texture.format);
            match texture.target {
                gl::TEXTURE_2D_ARRAY => {
                    self.gl.tex_image_3d(
                        texture.target,
                        0,
                        desc.internal,
                        texture.width as _,
                        texture.height as _,
                        texture.layer_count,
                        0,
                        desc.external,
                        desc.pixel_type,
                        pixels.map(texels_to_u8_slice),
                    )
                }
                _ => {
                    assert_eq!(texture.layer_count, 1);
                    self.gl.tex_image_2d(
                        texture.target,
                        0,
                        desc.internal,
                        texture.width as _,
                        texture.height as _,
                        0,
                        desc.external,
                        desc.pixel_type,
                        pixels.map(texels_to_u8_slice),
                    )
                }
            }
        }

        if needed_layer_count > 0 {
            // Create more framebuffers to fill the gap
            let new_fbos = self.gl.gen_framebuffers(needed_layer_count);
            texture
                .fbo_ids
                .extend(new_fbos.into_iter().map(FBOId));
        } else if needed_layer_count < 0 {
            // Remove extra framebuffers
            for old in texture.fbo_ids.drain(texture.layer_count as usize ..) {
                self.gl.delete_framebuffers(&[old.0]);
            }
        }

        let (mut depth_rb, allocate_depth) = match texture.depth_rb {
            Some(rbo) => (rbo.0, is_resized || !rt_info.has_depth),
            None if rt_info.has_depth => {
                let renderbuffer_ids = self.gl.gen_renderbuffers(1);
                let depth_rb = renderbuffer_ids[0];
                texture.depth_rb = Some(RBOId(depth_rb));
                (depth_rb, true)
            },
            None => (0, false),
        };

        if allocate_depth {
            if rt_info.has_depth {
                self.gl.bind_renderbuffer(gl::RENDERBUFFER, depth_rb);
                self.gl.renderbuffer_storage(
                    gl::RENDERBUFFER,
                    gl::DEPTH_COMPONENT24,
                    texture.width as _,
                    texture.height as _,
                );
            } else {
                self.gl.delete_renderbuffers(&[depth_rb]);
                depth_rb = 0;
                texture.depth_rb = None;
            }
        }

        if allocate_color || allocate_depth {
            let original_bound_fbo = self.bound_draw_fbo;
            for (fbo_index, &fbo_id) in texture.fbo_ids.iter().enumerate() {
                self.bind_external_draw_target(fbo_id);
                match texture.target {
                    gl::TEXTURE_2D_ARRAY => {
                        self.gl.framebuffer_texture_layer(
                            gl::DRAW_FRAMEBUFFER,
                            gl::COLOR_ATTACHMENT0,
                            texture.id,
                            0,
                            fbo_index as _,
                        )
                    }
                    _ => {
                        assert_eq!(fbo_index, 0);
                        self.gl.framebuffer_texture_2d(
                            gl::DRAW_FRAMEBUFFER,
                            gl::COLOR_ATTACHMENT0,
                            texture.target,
                            texture.id,
                            0,
                        )
                    }
                }

                self.gl.framebuffer_renderbuffer(
                    gl::DRAW_FRAMEBUFFER,
                    gl::DEPTH_ATTACHMENT,
                    gl::RENDERBUFFER,
                    depth_rb,
                );
            }
            self.bind_external_draw_target(original_bound_fbo);
        }
    }

    fn update_texture_storage<T: Texel>(&mut self, texture: &Texture, pixels: Option<&[T]>) {
        let desc = gl_describe_format(self.gl(), texture.format);
        match texture.target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_image_3d(
                    gl::TEXTURE_2D_ARRAY,
                    0,
                    desc.internal,
                    texture.width as _,
                    texture.height as _,
                    texture.layer_count,
                    0,
                    desc.external,
                    desc.pixel_type,
                    pixels.map(texels_to_u8_slice),
                );
            }
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => {
                self.gl.tex_image_2d(
                    texture.target,
                    0,
                    desc.internal,
                    texture.width as _,
                    texture.height as _,
                    0,
                    desc.external,
                    desc.pixel_type,
                    pixels.map(texels_to_u8_slice),
                );
            }
            _ => panic!("BUG: Unexpected texture target!"),
        }
    }

    pub fn blit_render_target(&mut self, src_rect: DeviceIntRect, dest_rect: DeviceIntRect) {
        debug_assert!(self.inside_frame);

        self.gl.blit_framebuffer(
            src_rect.origin.x,
            src_rect.origin.y,
            src_rect.origin.x + src_rect.size.width,
            src_rect.origin.y + src_rect.size.height,
            dest_rect.origin.x,
            dest_rect.origin.y,
            dest_rect.origin.x + dest_rect.size.width,
            dest_rect.origin.y + dest_rect.size.height,
            gl::COLOR_BUFFER_BIT,
            gl::LINEAR,
        );
    }

    fn free_texture_storage_impl(&mut self, target: gl::GLenum, desc: FormatDesc) {
        match target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_image_3d(
                    gl::TEXTURE_2D_ARRAY,
                    0,
                    desc.internal,
                    0,
                    0,
                    0,
                    0,
                    desc.external,
                    desc.pixel_type,
                    None,
                );
            }
            _ => {
                self.gl.tex_image_2d(
                    target,
                    0,
                    desc.internal,
                    0,
                    0,
                    0,
                    desc.external,
                    desc.pixel_type,
                    None,
                );
            }
        }
    }

    pub fn free_texture_storage(&mut self, texture: &mut Texture) {
        debug_assert!(self.inside_frame);

        if texture.width + texture.height == 0 {
            return;
        }

        self.bind_texture(DEFAULT_TEXTURE, texture);
        let desc = gl_describe_format(self.gl(), texture.format);

        self.free_texture_storage_impl(texture.target, desc);

        if let Some(RBOId(depth_rb)) = texture.depth_rb.take() {
            self.gl.delete_renderbuffers(&[depth_rb]);
        }

        if !texture.fbo_ids.is_empty() {
            let fbo_ids: Vec<_> = texture
                .fbo_ids
                .drain(..)
                .map(|FBOId(fbo_id)| fbo_id)
                .collect();
            self.gl.delete_framebuffers(&fbo_ids[..]);
        }

        texture.width = 0;
        texture.height = 0;
        texture.layer_count = 0;
    }

    pub fn delete_texture(&mut self, mut texture: Texture) {
        self.free_texture_storage(&mut texture);
        self.gl.delete_textures(&[texture.id]);

        for bound_texture in &mut self.bound_textures {
            if *bound_texture == texture.id {
                *bound_texture = 0
            }
        }

        texture.id = 0;
    }

    #[cfg(feature = "replay")]
    pub fn delete_external_texture(&mut self, mut external: ExternalTexture) {
        self.bind_external_texture(DEFAULT_TEXTURE, &external);
        //Note: the format descriptor here doesn't really matter
        self.free_texture_storage_impl(external.target, FormatDesc {
            internal: gl::R8 as _,
            external: gl::RED,
            pixel_type: gl::UNSIGNED_BYTE,
        });
        self.gl.delete_textures(&[external.id]);
        external.id = 0;
    }

    pub fn delete_program(&mut self, mut program: Program) {
        self.gl.delete_program(program.id);
        program.id = 0;
    }

    pub fn create_program(
        &mut self,
        base_filename: &str,
        features: &str,
        descriptor: &VertexDescriptor,
    ) -> Result<Program, ShaderError> {
        debug_assert!(self.inside_frame);

        let gl_version_string = get_shader_version(&*self.gl);

        let (vs_source, fs_source) = build_shader_strings(
            gl_version_string,
            features,
            base_filename,
            &self.resource_override_path,
        );

        let sources = ProgramSources::new(self.renderer_name.clone(), vs_source, fs_source);

        // Create program
        let pid = self.gl.create_program();

        let mut loaded = false;

        if let Some(ref cached_programs) = self.cached_programs {
            if let Some(binary) = cached_programs.binaries.borrow().get(&sources)
            {
                self.gl.program_binary(pid, binary.format, &binary.binary);

                if self.gl.get_program_iv(pid, gl::LINK_STATUS) == (0 as gl::GLint) {
                    let error_log = self.gl.get_program_info_log(pid);
                    error!(
                      "Failed to load a program object with a program binary: {} renderer {}\n{}",
                      base_filename,
                      self.renderer_name,
                      error_log
                    );
                } else {
                    loaded = true;
                }
            }
        }

        if loaded == false {
            // Compile the vertex shader
            let vs_id =
                match Device::compile_shader(&*self.gl, base_filename, gl::VERTEX_SHADER, &sources.vs_source) {
                    Ok(vs_id) => vs_id,
                    Err(err) => return Err(err),
                };

            // Compiler the fragment shader
            let fs_id =
                match Device::compile_shader(&*self.gl, base_filename, gl::FRAGMENT_SHADER, &sources.fs_source) {
                    Ok(fs_id) => fs_id,
                    Err(err) => {
                        self.gl.delete_shader(vs_id);
                        return Err(err);
                    }
                };

            // Attach shaders
            self.gl.attach_shader(pid, vs_id);
            self.gl.attach_shader(pid, fs_id);

            // Bind vertex attributes
            for (i, attr) in descriptor
                .vertex_attributes
                .iter()
                .chain(descriptor.instance_attributes.iter())
                .enumerate()
            {
                self.gl
                    .bind_attrib_location(pid, i as gl::GLuint, attr.name);
            }

            if self.cached_programs.is_some() {
                self.gl.program_parameter_i(pid, gl::PROGRAM_BINARY_RETRIEVABLE_HINT, gl::TRUE as gl::GLint);
            }

            // Link!
            self.gl.link_program(pid);

            // GL recommends detaching and deleting shaders once the link
            // is complete (whether successful or not). This allows the driver
            // to free any memory associated with the parsing and compilation.
            self.gl.detach_shader(pid, vs_id);
            self.gl.detach_shader(pid, fs_id);
            self.gl.delete_shader(vs_id);
            self.gl.delete_shader(fs_id);

            if self.gl.get_program_iv(pid, gl::LINK_STATUS) == (0 as gl::GLint) {
                let error_log = self.gl.get_program_info_log(pid);
                error!(
                    "Failed to link shader program: {}\n{}",
                    base_filename,
                    error_log
                );
                self.gl.delete_program(pid);
                return Err(ShaderError::Link(base_filename.to_string(), error_log));
            }
        }

        if let Some(ref cached_programs) = self.cached_programs {
            if !cached_programs.binaries.borrow().contains_key(&sources) {
                let (buffer, format) = self.gl.get_program_binary(pid);
                if buffer.len() > 0 {
                  cached_programs.binaries.borrow_mut().insert(sources, ProgramBinary::new(buffer, format));
                }
            }
        }

        let u_transform = self.gl.get_uniform_location(pid, "uTransform");
        let u_device_pixel_ratio = self.gl.get_uniform_location(pid, "uDevicePixelRatio");
        let u_mode = self.gl.get_uniform_location(pid, "uMode");

        let program = Program {
            id: pid,
            u_transform,
            u_device_pixel_ratio,
            u_mode,
        };

        self.bind_program(&program);

        Ok(program)
    }

    pub fn bind_shader_samplers<S>(&mut self, program: &Program, bindings: &[(&'static str, S)])
    where
        S: Into<TextureSlot> + Copy,
    {
        for binding in bindings {
            let u_location = self.gl.get_uniform_location(program.id, binding.0);
            if u_location != -1 {
                self.bind_program(program);
                self.gl
                    .uniform_1i(u_location, binding.1.into().0 as gl::GLint);
            }
        }
    }

    #[cfg(feature = "debug_renderer")]
    pub fn get_uniform_location(&self, program: &Program, name: &str) -> UniformLocation {
        UniformLocation(self.gl.get_uniform_location(program.id, name))
    }

    pub fn set_uniforms(
        &self,
        program: &Program,
        transform: &Transform3D<f32>,
    ) {
        debug_assert!(self.inside_frame);
        self.gl
            .uniform_matrix_4fv(program.u_transform, false, &transform.to_row_major_array());
        self.gl
            .uniform_1f(program.u_device_pixel_ratio, self.device_pixel_ratio);
    }

    pub fn switch_mode(&self, mode: i32) {
        debug_assert!(self.inside_frame);
        self.gl.uniform_1i(self.program_mode_id.0, mode);
    }

    pub fn create_pbo(&mut self) -> PBO {
        let id = self.gl.gen_buffers(1)[0];
        PBO { id }
    }

    pub fn delete_pbo(&mut self, mut pbo: PBO) {
        self.gl.delete_buffers(&[pbo.id]);
        pbo.id = 0;
    }

    pub fn upload_texture<'a, T>(
        &'a mut self,
        texture: &'a Texture,
        pbo: &PBO,
        upload_count: usize,
    ) -> TextureUploader<'a, T> {
        debug_assert!(self.inside_frame);
        self.bind_texture(DEFAULT_TEXTURE, texture);

        let buffer = match self.upload_method {
            UploadMethod::Immediate => None,
            UploadMethod::PixelBuffer(hint) => {
                let upload_size = upload_count * mem::size_of::<T>();
                self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, pbo.id);
                if upload_size != 0 {
                    self.gl.buffer_data_untyped(
                        gl::PIXEL_UNPACK_BUFFER,
                        upload_size as _,
                        ptr::null(),
                        hint.to_gl(),
                    );
                }
                Some(PixelBuffer::new(hint.to_gl(), upload_size))
            },
        };

        TextureUploader {
            target: UploadTarget {
                gl: &*self.gl,
                texture,
            },
            buffer,
            marker: PhantomData,
        }
    }

    #[cfg(any(feature = "debug_renderer", feature = "capture"))]
    pub fn read_pixels(&mut self, img_desc: &ImageDescriptor) -> Vec<u8> {
        let desc = gl_describe_format(self.gl(), img_desc.format);
        self.gl.read_pixels(
            0, 0,
            img_desc.width as i32,
            img_desc.height as i32,
            desc.external,
            desc.pixel_type,
        )
    }

    /// Read rectangle of pixels into the specified output slice.
    pub fn read_pixels_into(
        &mut self,
        rect: DeviceUintRect,
        format: ReadPixelsFormat,
        output: &mut [u8],
    ) {
        let (bytes_per_pixel, desc) = match format {
            ReadPixelsFormat::Standard(imf) => {
                (imf.bytes_per_pixel(), gl_describe_format(self.gl(), imf))
            }
            ReadPixelsFormat::Rgba8 => {
                (4, FormatDesc {
                    external: gl::RGBA,
                    internal: gl::RGBA8 as _,
                    pixel_type: gl::UNSIGNED_BYTE,
                })
            }
        };
        let size_in_bytes = (bytes_per_pixel * rect.size.width * rect.size.height) as usize;
        assert_eq!(output.len(), size_in_bytes);

        self.gl.flush();
        self.gl.read_pixels_into_buffer(
            rect.origin.x as _,
            rect.origin.y as _,
            rect.size.width as _,
            rect.size.height as _,
            desc.external,
            desc.pixel_type,
            output,
        );
    }

    /// Get texels of a texture into the specified output slice.
    #[cfg(feature = "debug_renderer")]
    pub fn get_tex_image_into(
        &mut self,
        texture: &Texture,
        format: ImageFormat,
        output: &mut [u8],
    ) {
        self.bind_texture(DEFAULT_TEXTURE, texture);
        let desc = gl_describe_format(self.gl(), format);
        self.gl.get_tex_image_into_buffer(
            texture.target,
            0,
            desc.external,
            desc.pixel_type,
            output,
        );
    }

    /// Attaches the provided texture to the current Read FBO binding.
    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    fn attach_read_texture_raw(
        &mut self, texture_id: gl::GLuint, target: gl::GLuint, layer_id: i32
    ) {
        match target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.framebuffer_texture_layer(
                    gl::READ_FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    texture_id,
                    0,
                    layer_id,
                )
            }
            _ => {
                assert_eq!(layer_id, 0);
                self.gl.framebuffer_texture_2d(
                    gl::READ_FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    target,
                    texture_id,
                    0,
                )
            }
        }
    }

    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    pub fn attach_read_texture_external(
        &mut self, texture_id: gl::GLuint, target: TextureTarget, layer_id: i32
    ) {
        self.attach_read_texture_raw(texture_id, get_gl_target(target), layer_id)
    }

    #[cfg(any(feature = "debug_renderer", feature="capture"))]
    pub fn attach_read_texture(&mut self, texture: &Texture, layer_id: i32) {
        self.attach_read_texture_raw(texture.id, texture.target, layer_id)
    }

    fn bind_vao_impl(&mut self, id: gl::GLuint) {
        debug_assert!(self.inside_frame);

        if self.bound_vao != id {
            self.bound_vao = id;
            self.gl.bind_vertex_array(id);
        }
    }

    pub fn bind_vao(&mut self, vao: &VAO) {
        self.bind_vao_impl(vao.id)
    }

    pub fn bind_custom_vao(&mut self, vao: &CustomVAO) {
        self.bind_vao_impl(vao.id)
    }

    fn create_vao_with_vbos(
        &mut self,
        descriptor: &VertexDescriptor,
        main_vbo_id: VBOId,
        instance_vbo_id: VBOId,
        ibo_id: IBOId,
        owns_vertices_and_indices: bool,
    ) -> VAO {
        debug_assert!(self.inside_frame);

        let instance_stride = descriptor.instance_stride() as usize;
        let vao_id = self.gl.gen_vertex_arrays(1)[0];

        self.gl.bind_vertex_array(vao_id);

        descriptor.bind(self.gl(), main_vbo_id, instance_vbo_id);
        ibo_id.bind(self.gl()); // force it to be a part of VAO

        self.gl.bind_vertex_array(0);

        VAO {
            id: vao_id,
            ibo_id,
            main_vbo_id,
            instance_vbo_id,
            instance_stride,
            owns_vertices_and_indices,
        }
    }

    pub fn create_custom_vao(
        &mut self,
        streams: &[Stream],
    ) -> CustomVAO {
        debug_assert!(self.inside_frame);

        let vao_id = self.gl.gen_vertex_arrays(1)[0];
        self.gl.bind_vertex_array(vao_id);

        let mut attrib_index = 0;
        for stream in streams {
            VertexDescriptor::bind_attributes(
                stream.attributes,
                attrib_index,
                0,
                self.gl(),
                stream.vbo,
            );
            attrib_index += stream.attributes.len();
        }

        self.gl.bind_vertex_array(0);

        CustomVAO {
            id: vao_id,
        }
    }

    pub fn delete_custom_vao(&mut self, mut vao: CustomVAO) {
        self.gl.delete_vertex_arrays(&[vao.id]);
        vao.id = 0;
    }

    pub fn create_vbo<T>(&mut self) -> VBO<T> {
        let ids = self.gl.gen_buffers(1);
        VBO {
            id: ids[0],
            target: gl::ARRAY_BUFFER,
            allocated_count: 0,
            marker: PhantomData,
        }
    }

    pub fn delete_vbo<T>(&mut self, mut vbo: VBO<T>) {
        self.gl.delete_buffers(&[vbo.id]);
        vbo.id = 0;
    }

    pub fn create_vao(&mut self, descriptor: &VertexDescriptor) -> VAO {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(3);
        let ibo_id = IBOId(buffer_ids[0]);
        let main_vbo_id = VBOId(buffer_ids[1]);
        let intance_vbo_id = VBOId(buffer_ids[2]);

        self.create_vao_with_vbos(descriptor, main_vbo_id, intance_vbo_id, ibo_id, true)
    }

    pub fn delete_vao(&mut self, mut vao: VAO) {
        self.gl.delete_vertex_arrays(&[vao.id]);
        vao.id = 0;

        if vao.owns_vertices_and_indices {
            self.gl.delete_buffers(&[vao.ibo_id.0]);
            self.gl.delete_buffers(&[vao.main_vbo_id.0]);
        }

        self.gl.delete_buffers(&[vao.instance_vbo_id.0])
    }

    pub fn allocate_vbo<V>(
        &mut self,
        vbo: &mut VBO<V>,
        count: usize,
        usage_hint: VertexUsageHint,
    ) {
        debug_assert!(self.inside_frame);
        vbo.allocated_count = count;

        self.gl.bind_buffer(vbo.target, vbo.id);
        self.gl.buffer_data_untyped(
            vbo.target,
            (count * mem::size_of::<V>()) as _,
            ptr::null(),
            usage_hint.to_gl(),
        );
    }

    pub fn fill_vbo<V>(
        &mut self,
        vbo: &VBO<V>,
        data: &[V],
        offset: usize,
    ) {
        debug_assert!(self.inside_frame);
        assert!(offset + data.len() <= vbo.allocated_count);
        let stride = mem::size_of::<V>();

        self.gl.bind_buffer(vbo.target, vbo.id);
        self.gl.buffer_sub_data_untyped(
            vbo.target,
            (offset * stride) as _,
            (data.len() * stride) as _,
            data.as_ptr() as _,
        );
    }

    fn update_vbo_data<V>(
        &mut self,
        vbo: VBOId,
        vertices: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert!(self.inside_frame);

        vbo.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, vertices, usage_hint.to_gl());
    }

    pub fn create_vao_with_new_instances(
        &mut self,
        descriptor: &VertexDescriptor,
        base_vao: &VAO,
    ) -> VAO {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(1);
        let intance_vbo_id = VBOId(buffer_ids[0]);

        self.create_vao_with_vbos(
            descriptor,
            base_vao.main_vbo_id,
            intance_vbo_id,
            base_vao.ibo_id,
            false,
        )
    }

    pub fn update_vao_main_vertices<V>(
        &mut self,
        vao: &VAO,
        vertices: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert_eq!(self.bound_vao, vao.id);
        self.update_vbo_data(vao.main_vbo_id, vertices, usage_hint)
    }

    pub fn update_vao_instances<V>(
        &mut self,
        vao: &VAO,
        instances: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert_eq!(self.bound_vao, vao.id);
        debug_assert_eq!(vao.instance_stride as usize, mem::size_of::<V>());

        self.update_vbo_data(vao.instance_vbo_id, instances, usage_hint)
    }

    pub fn update_vao_indices<I>(&mut self, vao: &VAO, indices: &[I], usage_hint: VertexUsageHint) {
        debug_assert!(self.inside_frame);
        debug_assert_eq!(self.bound_vao, vao.id);

        vao.ibo_id.bind(self.gl());
        gl::buffer_data(
            self.gl(),
            gl::ELEMENT_ARRAY_BUFFER,
            indices,
            usage_hint.to_gl(),
        );
    }

    pub fn draw_triangles_u16(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_SHORT,
            first_vertex as u32 * 2,
        );
    }

    #[cfg(feature = "debug_renderer")]
    pub fn draw_triangles_u32(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_INT,
            first_vertex as u32 * 4,
        );
    }

    pub fn draw_nonindexed_points(&mut self, first_vertex: i32, vertex_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_arrays(gl::POINTS, first_vertex, vertex_count);
    }

    #[cfg(feature = "debug_renderer")]
    pub fn draw_nonindexed_lines(&mut self, first_vertex: i32, vertex_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_arrays(gl::LINES, first_vertex, vertex_count);
    }

    pub fn draw_indexed_triangles_instanced_u16(&mut self, index_count: i32, instance_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements_instanced(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_SHORT,
            0,
            instance_count,
        );
    }

    pub fn end_frame(&mut self) {
        self.bind_draw_target(None, None);
        self.bind_read_target(None);

        debug_assert!(self.inside_frame);
        self.inside_frame = false;

        self.gl.bind_texture(gl::TEXTURE_2D, 0);
        self.gl.use_program(0);

        for i in 0 .. self.bound_textures.len() {
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id.0 += 1;
    }

    pub fn clear_target(
        &self,
        color: Option<[f32; 4]>,
        depth: Option<f32>,
        rect: Option<DeviceIntRect>,
    ) {
        let mut clear_bits = 0;

        if let Some(color) = color {
            self.gl.clear_color(color[0], color[1], color[2], color[3]);
            clear_bits |= gl::COLOR_BUFFER_BIT;
        }

        if let Some(depth) = depth {
            debug_assert_ne!(self.gl.get_boolean_v(gl::DEPTH_WRITEMASK), 0);
            self.gl.clear_depth(depth as f64);
            clear_bits |= gl::DEPTH_BUFFER_BIT;
        }

        if clear_bits != 0 {
            match rect {
                Some(rect) => {
                    self.gl.enable(gl::SCISSOR_TEST);
                    self.gl.scissor(
                        rect.origin.x,
                        rect.origin.y,
                        rect.size.width,
                        rect.size.height,
                    );
                    self.gl.clear(clear_bits);
                    self.gl.disable(gl::SCISSOR_TEST);
                }
                None => {
                    self.gl.clear(clear_bits);
                }
            }
        }
    }

    pub fn enable_depth(&self) {
        self.gl.enable(gl::DEPTH_TEST);
    }

    pub fn disable_depth(&self) {
        self.gl.disable(gl::DEPTH_TEST);
    }

    pub fn set_depth_func(&self, depth_func: DepthFunction) {
        self.gl.depth_func(depth_func as gl::GLuint);
    }

    pub fn enable_depth_write(&self) {
        self.gl.depth_mask(true);
    }

    pub fn disable_depth_write(&self) {
        self.gl.depth_mask(false);
    }

    pub fn disable_stencil(&self) {
        self.gl.disable(gl::STENCIL_TEST);
    }

    pub fn set_scissor_rect(&self, rect: DeviceIntRect) {
        self.gl.scissor(
            rect.origin.x,
            rect.origin.y,
            rect.size.width,
            rect.size.height,
        );
    }

    pub fn enable_scissor(&self) {
        self.gl.enable(gl::SCISSOR_TEST);
    }

    pub fn disable_scissor(&self) {
        self.gl.disable(gl::SCISSOR_TEST);
    }

    pub fn set_blend(&self, enable: bool) {
        if enable {
            self.gl.enable(gl::BLEND);
        } else {
            self.gl.disable(gl::BLEND);
        }
    }

    pub fn set_blend_mode_alpha(&self) {
        self.gl.blend_func_separate(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA,
                                    gl::ONE, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_premultiplied_alpha(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_premultiplied_dest_out(&self) {
        self.gl.blend_func(gl::ZERO, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_multiply(&self) {
        self.gl
            .blend_func_separate(gl::ZERO, gl::SRC_COLOR, gl::ZERO, gl::SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_max(&self) {
        self.gl
            .blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MAX, gl::FUNC_ADD);
    }
    #[cfg(feature = "debug_renderer")]
    pub fn set_blend_mode_min(&self) {
        self.gl
            .blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MIN, gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_pass0(&self) {
        self.gl.blend_func(gl::ZERO, gl::ONE_MINUS_SRC_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_pass1(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass0(&self) {
        self.gl.blend_func_separate(gl::ZERO, gl::ONE_MINUS_SRC_COLOR, gl::ZERO, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass1(&self) {
        self.gl.blend_func_separate(gl::ONE_MINUS_DST_ALPHA, gl::ONE, gl::ZERO, gl::ONE);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_with_bg_color_pass2(&self) {
        self.gl.blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_constant_text_color(&self, color: ColorF) {
        // color is an unpremultiplied color.
        self.gl.blend_color(color.r, color.g, color.b, 1.0);
        self.gl
            .blend_func(gl::CONSTANT_COLOR, gl::ONE_MINUS_SRC_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_dual_source(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE_MINUS_SRC1_COLOR);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn supports_extension(&self, extension: &str) -> bool {
        self.extensions.iter().any(|s| s == extension)
    }

    pub fn echo_driver_messages(&self) {
        for msg in self.gl.get_debug_messages() {
            let level = match msg.severity {
                gl::DEBUG_SEVERITY_HIGH => Level::Error,
                gl::DEBUG_SEVERITY_MEDIUM => Level::Warn,
                gl::DEBUG_SEVERITY_LOW => Level::Info,
                gl::DEBUG_SEVERITY_NOTIFICATION => Level::Debug,
                _ => Level::Trace,
            };
            let ty = match msg.ty {
                gl::DEBUG_TYPE_ERROR => "error",
                gl::DEBUG_TYPE_DEPRECATED_BEHAVIOR => "deprecated",
                gl::DEBUG_TYPE_UNDEFINED_BEHAVIOR => "undefined",
                gl::DEBUG_TYPE_PORTABILITY => "portability",
                gl::DEBUG_TYPE_PERFORMANCE => "perf",
                gl::DEBUG_TYPE_MARKER => "marker",
                gl::DEBUG_TYPE_PUSH_GROUP => "group push",
                gl::DEBUG_TYPE_POP_GROUP => "group pop",
                gl::DEBUG_TYPE_OTHER => "other",
                _ => "?",
            };
            log!(level, "({}) {}", ty, msg.message);
        }
    }
}

struct FormatDesc {
    internal: gl::GLint,
    external: gl::GLuint,
    pixel_type: gl::GLuint,
}

fn gl_describe_format(gl: &gl::Gl, format: ImageFormat) -> FormatDesc {
    match format {
        ImageFormat::R8 => FormatDesc {
            internal: gl::RED as _,
            external: gl::RED,
            pixel_type: gl::UNSIGNED_BYTE,
        },
        ImageFormat::BGRA8 => {
            let external = get_gl_format_bgra(gl);
            FormatDesc {
                internal: match gl.get_type() {
                    gl::GlType::Gl => gl::RGBA as _,
                    gl::GlType::Gles => external as _,
                },
                external,
                pixel_type: gl::UNSIGNED_BYTE,
            }
        },
        ImageFormat::RGBAF32 => FormatDesc {
            internal: gl::RGBA32F as _,
            external: gl::RGBA,
            pixel_type: gl::FLOAT,
        },
        ImageFormat::RG8 => FormatDesc {
            internal: gl::RG8 as _,
            external: gl::RG,
            pixel_type: gl::UNSIGNED_BYTE,
        },
    }
}

struct UploadChunk {
    rect: DeviceUintRect,
    layer_index: i32,
    stride: Option<u32>,
    offset: usize,
}

struct PixelBuffer {
    usage: gl::GLenum,
    size_allocated: usize,
    size_used: usize,
    // small vector avoids heap allocation for a single chunk
    chunks: SmallVec<[UploadChunk; 1]>,
}

impl PixelBuffer {
    fn new(
        usage: gl::GLenum,
        size_allocated: usize,
    ) -> Self {
        PixelBuffer {
            usage,
            size_allocated,
            size_used: 0,
            chunks: SmallVec::new(),
        }
    }
}

struct UploadTarget<'a> {
    gl: &'a gl::Gl,
    texture: &'a Texture,
}

pub struct TextureUploader<'a, T> {
    target: UploadTarget<'a>,
    buffer: Option<PixelBuffer>,
    marker: PhantomData<T>,
}

impl<'a, T> Drop for TextureUploader<'a, T> {
    fn drop(&mut self) {
        if let Some(buffer) = self.buffer.take() {
            for chunk in buffer.chunks {
                self.target.update_impl(chunk);
            }
            self.target.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);
        }
    }
}

impl<'a, T> TextureUploader<'a, T> {
    pub fn upload(
        &mut self,
        rect: DeviceUintRect,
        layer_index: i32,
        stride: Option<u32>,
        data: &[T],
    ) {
        match self.buffer {
            Some(ref mut buffer) => {
                let upload_size = mem::size_of::<T>() * data.len();
                if buffer.size_used + upload_size > buffer.size_allocated {
                    // flush
                    for chunk in buffer.chunks.drain() {
                        self.target.update_impl(chunk);
                    }
                    buffer.size_used = 0;
                }

                if upload_size > buffer.size_allocated {
                    gl::buffer_data(
                        self.target.gl,
                        gl::PIXEL_UNPACK_BUFFER,
                        data,
                        buffer.usage,
                    );
                    buffer.size_allocated = upload_size;
                } else {
                    gl::buffer_sub_data(
                        self.target.gl,
                        gl::PIXEL_UNPACK_BUFFER,
                        buffer.size_used as _,
                        data,
                    );
                }

                buffer.chunks.push(UploadChunk {
                    rect, layer_index, stride,
                    offset: buffer.size_used,
                });
                buffer.size_used += upload_size;
            }
            None => {
                self.target.update_impl(UploadChunk {
                    rect, layer_index, stride,
                    offset: data.as_ptr() as _,
                });
            }
        }
    }
}

impl<'a> UploadTarget<'a> {
    fn update_impl(&mut self, chunk: UploadChunk) {
        let (gl_format, bpp, data_type) = match self.texture.format {
            ImageFormat::R8 => (gl::RED, 1, gl::UNSIGNED_BYTE),
            ImageFormat::BGRA8 => (get_gl_format_bgra(self.gl), 4, gl::UNSIGNED_BYTE),
            ImageFormat::RG8 => (gl::RG, 2, gl::UNSIGNED_BYTE),
            ImageFormat::RGBAF32 => (gl::RGBA, 16, gl::FLOAT),
        };

        let row_length = match chunk.stride {
            Some(value) => value / bpp,
            None => self.texture.width,
        };

        if chunk.stride.is_some() {
            self.gl.pixel_store_i(
                gl::UNPACK_ROW_LENGTH,
                row_length as _,
            );
        }

        let pos = chunk.rect.origin;
        let size = chunk.rect.size;

        match self.texture.target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_sub_image_3d_pbo(
                    self.texture.target,
                    0,
                    pos.x as _,
                    pos.y as _,
                    chunk.layer_index,
                    size.width as _,
                    size.height as _,
                    1,
                    gl_format,
                    data_type,
                    chunk.offset,
                );
            }
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => {
                self.gl.tex_sub_image_2d_pbo(
                    self.texture.target,
                    0,
                    pos.x as _,
                    pos.y as _,
                    size.width as _,
                    size.height as _,
                    gl_format,
                    data_type,
                    chunk.offset,
                );
            }
            _ => panic!("BUG: Unexpected texture target!"),
        }

        // If using tri-linear filtering, build the mip-map chain for this texture.
        if self.texture.filter == TextureFilter::Trilinear {
            self.gl.generate_mipmap(self.texture.target);
        }

        // Reset row length to 0, otherwise the stride would apply to all texture uploads.
        if chunk.stride.is_some() {
            self.gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0 as _);
        }
    }
}

fn texels_to_u8_slice<T: Texel>(texels: &[T]) -> &[u8] {
    unsafe {
        slice::from_raw_parts(texels.as_ptr() as *const u8, texels.len() * mem::size_of::<T>())
    }
}

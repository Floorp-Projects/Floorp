/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use super::shader_source;
use api::{ColorF, ImageFormat};
use api::{DeviceIntRect, DeviceUintSize};
use euclid::Transform3D;
use gleam::gl;
use internal_types::{FastHashMap, RenderTargetInfo};
use std::cell::RefCell;
use std::fs::File;
use std::io::Read;
use std::iter::repeat;
use std::mem;
use std::ops::Add;
use std::path::PathBuf;
use std::ptr;
use std::rc::Rc;
use std::thread;

#[derive(Debug, Copy, Clone, PartialEq, Ord, Eq, PartialOrd)]
pub struct FrameId(usize);

impl FrameId {
    pub fn new(value: usize) -> FrameId {
        FrameId(value)
    }
}

impl Add<usize> for FrameId {
    type Output = FrameId;

    fn add(self, other: usize) -> FrameId {
        FrameId(self.0 + other)
    }
}

#[cfg(not(any(target_arch = "arm", target_arch = "aarch64")))]
const GL_FORMAT_A: gl::GLuint = gl::RED;

#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
const GL_FORMAT_A: gl::GLuint = gl::ALPHA;

const GL_FORMAT_BGRA_GL: gl::GLuint = gl::BGRA;

const GL_FORMAT_BGRA_GLES: gl::GLuint = gl::BGRA_EXT;

const SHADER_VERSION_GL: &str = "#version 150\n";
const SHADER_VERSION_GLES: &str = "#version 300 es\n";

const SHADER_KIND_VERTEX: &str = "#define WR_VERTEX_SHADER\n";
const SHADER_KIND_FRAGMENT: &str = "#define WR_FRAGMENT_SHADER\n";
const SHADER_IMPORT: &str = "#include ";
const SHADER_LINE_MARKER: &str = "#line 1\n";

pub struct TextureSlot(pub usize);

// In some places we need to temporarily bind a texture to any slot.
const DEFAULT_TEXTURE: TextureSlot = TextureSlot(0);

#[repr(u32)]
pub enum DepthFunction {
    Less = gl::LESS,
    LessEqual = gl::LEQUAL,
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum TextureTarget {
    Default,
    Array,
    Rect,
    External,
}

impl TextureTarget {
    pub fn to_gl_target(&self) -> gl::GLuint {
        match *self {
            TextureTarget::Default => gl::TEXTURE_2D,
            TextureTarget::Array => gl::TEXTURE_2D_ARRAY,
            TextureTarget::Rect => gl::TEXTURE_RECTANGLE,
            TextureTarget::External => gl::TEXTURE_EXTERNAL_OES,
        }
    }
}

#[derive(Copy, Clone, Debug, PartialEq)]
pub enum TextureFilter {
    Nearest,
    Linear,
}

#[derive(Debug)]
pub enum VertexAttributeKind {
    F32,
    U8Norm,
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
    output.push_str(SHADER_LINE_MARKER);

    for (line_num, line) in source.lines().enumerate() {
        if line.starts_with(SHADER_IMPORT) {
            let imports = line[SHADER_IMPORT.len() ..].split(",");

            // For each import, get the source, and recurse.
            for import in imports {
                if let Some(include) = get_shader_source(import, base_path) {
                    parse_shader_source(include, base_path, output);
                }
            }

            output.push_str(&format!("#line {}\n", line_num+1));
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

    // Append legacy (.vs and .fs) files if they exist.
    // TODO(gw): Once all shaders are ported to just use the
    //           .glsl file, we can remove this code.
    let vs_name = format!("{}.vs", base_filename);
    if let Some(old_vs_source) = get_shader_source(&vs_name, override_path) {
        vs_source.push_str(SHADER_LINE_MARKER);
        vs_source.push_str(&old_vs_source);
    }

    let fs_name = format!("{}.fs", base_filename);
    if let Some(old_fs_source) = get_shader_source(&fs_name, override_path) {
        fs_source.push_str(SHADER_LINE_MARKER);
        fs_source.push_str(&old_fs_source);
    }

    (vs_source, fs_source)
}

pub trait FileWatcherHandler: Send {
    fn file_changed(&self, path: PathBuf);
}

impl VertexAttributeKind {
    fn size_in_bytes(&self) -> u32 {
        match *self {
            VertexAttributeKind::F32 => 4,
            VertexAttributeKind::U8Norm => 1,
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

    fn bind(&self, gl: &gl::Gl, main: VBOId, instance: VBOId) {
        main.bind(gl);

        let vertex_stride: u32 = self.vertex_attributes
            .iter()
            .map(|attr| attr.size_in_bytes())
            .sum();
        let mut vertex_offset = 0;

        for (i, attr) in self.vertex_attributes.iter().enumerate() {
            let attr_index = i as gl::GLuint;
            attr.bind_to_vao(attr_index, 0, vertex_stride as gl::GLint, vertex_offset, gl);
            vertex_offset += attr.size_in_bytes();
        }

        if !self.instance_attributes.is_empty() {
            instance.bind(gl);
            let instance_stride = self.instance_stride();
            let mut instance_offset = 0;

            let base_attr = self.vertex_attributes.len() as u32;

            for (i, attr) in self.instance_attributes.iter().enumerate() {
                let attr_index = base_attr + i as u32;
                attr.bind_to_vao(
                    attr_index,
                    1,
                    instance_stride as gl::GLint,
                    instance_offset,
                    gl,
                );
                instance_offset += attr.size_in_bytes();
            }
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

pub struct ExternalTexture {
    id: gl::GLuint,
    target: gl::GLuint,
}

impl ExternalTexture {
    pub fn new(id: u32, target: TextureTarget) -> ExternalTexture {
        ExternalTexture {
            id,
            target: target.to_gl_target(),
        }
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

    pub fn get_bpp(&self) -> u32 {
        match self.format {
            ImageFormat::A8 => 1,
            ImageFormat::RGB8 => 3,
            ImageFormat::BGRA8 => 4,
            ImageFormat::RG8 => 2,
            ImageFormat::RGBAF32 => 16,
            ImageFormat::Invalid => unreachable!(),
        }
    }

    pub fn has_depth(&self) -> bool {
        self.depth_rb.is_some()
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
    pub fn invalid() -> UniformLocation {
        UniformLocation(-1)
    }
}

pub struct Capabilities {
    pub supports_multisampling: bool,
}

#[derive(Clone, Debug)]
pub enum ShaderError {
    Compilation(String, String), // name, error mssage
    Link(String, String),        // name, error message
}

pub struct Device {
    gl: Rc<gl::Gl>,
    // device state
    bound_textures: [gl::GLuint; 16],
    bound_program: gl::GLuint,
    bound_vao: gl::GLuint,
    bound_pbo: gl::GLuint,
    bound_read_fbo: FBOId,
    bound_draw_fbo: FBOId,
    default_read_fbo: gl::GLuint,
    default_draw_fbo: gl::GLuint,

    pub device_pixel_ratio: f32,

    // HW or API capabilties
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
}

impl Device {
    pub fn new(
        gl: Rc<gl::Gl>,
        resource_override_path: Option<PathBuf>,
        _file_changed_handler: Box<FileWatcherHandler>,
        cached_programs: Option<Rc<ProgramCache>>,
    ) -> Device {
        let max_texture_size = gl.get_integer_v(gl::MAX_TEXTURE_SIZE) as u32;
        let renderer_name = gl.get_string(gl::RENDERER);

        Device {
            gl,
            resource_override_path,
            // This is initialized to 1 by default, but it is set
            // every frame by the call to begin_frame().
            device_pixel_ratio: 1.0,
            inside_frame: false,

            capabilities: Capabilities {
                supports_multisampling: false, //TODO
            },

            bound_textures: [0; 16],
            bound_program: 0,
            bound_vao: 0,
            bound_pbo: 0,
            bound_read_fbo: FBOId(0),
            bound_draw_fbo: FBOId(0),
            default_read_fbo: 0,
            default_draw_fbo: 0,

            max_texture_size,
            renderer_name,
            cached_programs,
            frame_id: FrameId(0),
        }
    }

    pub fn gl(&self) -> &gl::Gl {
        &*self.gl
    }

    pub fn rc_gl(&self) -> &Rc<gl::Gl> {
        &self.gl
    }

    pub fn update_program_cache(&mut self, cached_programs: Rc<ProgramCache>) {
        self.cached_programs = Some(cached_programs);
    }

    pub fn max_texture_size(&self) -> u32 {
        self.max_texture_size
    }

    pub fn get_capabilities(&self) -> &Capabilities {
        &self.capabilities
    }

    pub fn reset_state(&mut self) {
        self.bound_textures = [0; 16];
        self.bound_vao = 0;
        self.bound_pbo = 0;
        self.bound_read_fbo = FBOId(0);
        self.bound_draw_fbo = FBOId(0);
    }

    pub fn compile_shader(
        gl: &gl::Gl,
        name: &str,
        shader_type: gl::GLenum,
        source: &String,
    ) -> Result<gl::GLuint, ShaderError> {
        debug!("compile {:?}", name);
        let id = gl.create_shader(shader_type);
        gl.shader_source(id, &[source.as_bytes()]);
        gl.compile_shader(id);
        let log = gl.get_shader_info_log(id);
        if gl.get_shader_iv(id, gl::COMPILE_STATUS) == (0 as gl::GLint) {
            println!("Failed to compile shader: {:?}\n{}", name, log);
            Err(ShaderError::Compilation(name.to_string(), log))
        } else {
            if !log.is_empty() {
                println!("Warnings detected on shader: {:?}\n{}", name, log);
            }
            Ok(id)
        }
    }

    pub fn begin_frame(&mut self) -> FrameId {
        debug_assert!(!self.inside_frame);
        self.inside_frame = true;

        // Retrive the currently set FBO.
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
        self.gl.use_program(0);

        // Vertex state
        self.bound_vao = 0;
        self.gl.bind_vertex_array(0);

        // FBO state
        self.bound_read_fbo = FBOId(self.default_read_fbo);
        self.bound_draw_fbo = FBOId(self.default_draw_fbo);

        // Pixel op state
        self.gl.pixel_store_i(gl::UNPACK_ALIGNMENT, 1);
        self.bound_pbo = 0;
        self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, 0);

        // Default is sampler 0, always
        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id
    }

    pub fn bind_texture<S>(&mut self, sampler: S, texture: &Texture)
    where
        S: Into<TextureSlot>,
    {
        debug_assert!(self.inside_frame);

        let sampler_index = sampler.into().0;
        if self.bound_textures[sampler_index] != texture.id {
            self.bound_textures[sampler_index] = texture.id;
            self.gl
                .active_texture(gl::TEXTURE0 + sampler_index as gl::GLuint);
            self.gl.bind_texture(texture.target, texture.id);
            self.gl.active_texture(gl::TEXTURE0);
        }
    }

    pub fn bind_external_texture<S>(&mut self, sampler: S, external_texture: &ExternalTexture)
    where
        S: Into<TextureSlot>,
    {
        debug_assert!(self.inside_frame);

        let sampler_index = sampler.into().0;
        if self.bound_textures[sampler_index] != external_texture.id {
            self.bound_textures[sampler_index] = external_texture.id;
            self.gl
                .active_texture(gl::TEXTURE0 + sampler_index as gl::GLuint);
            self.gl
                .bind_texture(external_texture.target, external_texture.id);
            self.gl.active_texture(gl::TEXTURE0);
        }
    }

    pub fn bind_read_target(&mut self, texture_and_layer: Option<(&Texture, i32)>) {
        debug_assert!(self.inside_frame);

        let fbo_id = texture_and_layer.map_or(FBOId(self.default_read_fbo), |texture_and_layer| {
            texture_and_layer.0.fbo_ids[texture_and_layer.1 as usize]
        });

        if self.bound_read_fbo != fbo_id {
            self.bound_read_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Read);
        }
    }

    pub fn bind_draw_target(
        &mut self,
        texture_and_layer: Option<(&Texture, i32)>,
        dimensions: Option<DeviceUintSize>,
    ) {
        debug_assert!(self.inside_frame);

        let fbo_id = texture_and_layer.map_or(FBOId(self.default_draw_fbo), |texture_and_layer| {
            texture_and_layer.0.fbo_ids[texture_and_layer.1 as usize]
        });

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }

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
        self.bind_external_draw_target(fbo);
        self.gl.framebuffer_texture_2d(
            gl::DRAW_FRAMEBUFFER,
            gl::COLOR_ATTACHMENT0,
            gl::TEXTURE_2D,
            texture_id,
            0,
        );
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
        }
    }

    pub fn create_texture(&mut self, target: TextureTarget) -> Texture {
        Texture {
            id: self.gl.gen_textures(1)[0],
            target: target.to_gl_target(),
            width: 0,
            height: 0,
            layer_count: 0,
            format: ImageFormat::Invalid,
            filter: TextureFilter::Nearest,
            render_target: None,
            fbo_ids: vec![],
            depth_rb: None,
        }
    }

    fn set_texture_parameters(&mut self, target: gl::GLuint, filter: TextureFilter) {
        let filter = match filter {
            TextureFilter::Nearest => gl::NEAREST,
            TextureFilter::Linear => gl::LINEAR,
        };

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MAG_FILTER, filter as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_MIN_FILTER, filter as gl::GLint);

        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
        self.gl
            .tex_parameter_i(target, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
    }

    pub fn init_texture(
        &mut self,
        texture: &mut Texture,
        width: u32,
        height: u32,
        format: ImageFormat,
        filter: TextureFilter,
        render_target: Option<RenderTargetInfo>,
        layer_count: i32,
        pixels: Option<&[u8]>,
    ) {
        debug_assert!(self.inside_frame);

        let resized = texture.width != width ||
            texture.height != height ||
            texture.format != format;

        texture.format = format;
        texture.width = width;
        texture.height = height;
        texture.filter = filter;
        texture.layer_count = layer_count;
        texture.render_target = render_target;

        self.bind_texture(DEFAULT_TEXTURE, texture);
        self.set_texture_parameters(texture.target, filter);

        match render_target {
            Some(info) => {
                assert!(pixels.is_none());
                self.update_texture_storage(texture, &info, resized);
            }
            None => {
                let (internal_format, gl_format) = gl_texture_formats_for_image_format(self.gl(), format);
                let type_ = gl_type_for_texture_format(format);

                let expanded_data: Vec<u8>;
                let actual_pixels = if pixels.is_some() && format == ImageFormat::A8 &&
                    cfg!(any(target_arch = "arm", target_arch = "aarch64"))
                {
                    expanded_data = pixels
                        .unwrap()
                        .iter()
                        .flat_map(|&byte| repeat(byte).take(4))
                        .collect();
                    Some(expanded_data.as_slice())
                } else {
                    pixels
                };

                match texture.target {
                    gl::TEXTURE_2D_ARRAY => {
                        self.gl.tex_image_3d(
                            gl::TEXTURE_2D_ARRAY,
                            0,
                            internal_format as gl::GLint,
                            width as gl::GLint,
                            height as gl::GLint,
                            layer_count,
                            0,
                            gl_format,
                            type_,
                            actual_pixels,
                        );
                    }
                    gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => {
                        self.gl.tex_image_2d(
                            texture.target,
                            0,
                            internal_format as gl::GLint,
                            width as gl::GLint,
                            height as gl::GLint,
                            0,
                            gl_format,
                            type_,
                            actual_pixels,
                        );
                    }
                    _ => panic!("BUG: Unexpected texture target!"),
                }
            }
        }
    }

    /// Updates the texture storage for the texture, creating FBOs as required.
    fn update_texture_storage(
        &mut self,
        texture: &mut Texture,
        rt_info: &RenderTargetInfo,
        is_resized: bool,
    ) {
        assert!(texture.layer_count > 0);
        assert_eq!(texture.target, gl::TEXTURE_2D_ARRAY);

        let needed_layer_count = texture.layer_count - texture.fbo_ids.len() as i32;
        let allocate_color = needed_layer_count != 0 || is_resized;

        if allocate_color {
            let (internal_format, gl_format) =
                gl_texture_formats_for_image_format(&*self.gl, texture.format);
            let type_ = gl_type_for_texture_format(texture.format);

            self.gl.tex_image_3d(
                texture.target,
                0,
                internal_format as gl::GLint,
                texture.width as gl::GLint,
                texture.height as gl::GLint,
                texture.layer_count,
                0,
                gl_format,
                type_,
                None,
            );
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
                    texture.width as gl::GLsizei,
                    texture.height as gl::GLsizei,
                );
            } else {
                self.gl.delete_renderbuffers(&[depth_rb]);
                depth_rb = 0;
                texture.depth_rb = None;
            }
        }

        if allocate_color || allocate_depth {
            for (fbo_index, &fbo_id) in texture.fbo_ids.iter().enumerate() {
                self.bind_external_draw_target(fbo_id);
                self.gl.framebuffer_texture_layer(
                    gl::DRAW_FRAMEBUFFER,
                    gl::COLOR_ATTACHMENT0,
                    texture.id,
                    0,
                    fbo_index as gl::GLint,
                );
                self.gl.framebuffer_renderbuffer(
                    gl::DRAW_FRAMEBUFFER,
                    gl::DEPTH_ATTACHMENT,
                    gl::RENDERBUFFER,
                    depth_rb,
                );
            }
            // restore the previous FBO
            let bound_fbo = self.bound_draw_fbo;
            self.bind_external_draw_target(bound_fbo);
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

    pub fn free_texture_storage(&mut self, texture: &mut Texture) {
        debug_assert!(self.inside_frame);
        debug_assert_eq!(self.bound_pbo, 0);

        if texture.format == ImageFormat::Invalid {
            return;
        }

        self.bind_texture(DEFAULT_TEXTURE, texture);

        let (internal_format, gl_format) =
            gl_texture_formats_for_image_format(&*self.gl, texture.format);
        let type_ = gl_type_for_texture_format(texture.format);

        match texture.target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_image_3d(
                    gl::TEXTURE_2D_ARRAY,
                    0,
                    internal_format as gl::GLint,
                    0,
                    0,
                    0,
                    0,
                    gl_format,
                    type_,
                    None,
                );
            }
            _ => {
                self.gl.tex_image_2d(
                    texture.target,
                    0,
                    internal_format,
                    0,
                    0,
                    0,
                    gl_format,
                    type_,
                    None,
                );
            }
        }

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

        texture.format = ImageFormat::Invalid;
        texture.width = 0;
        texture.height = 0;
        texture.layer_count = 0;
    }

    pub fn delete_texture(&mut self, mut texture: Texture) {
        self.free_texture_storage(&mut texture);
        self.gl.delete_textures(&[texture.id]);
        texture.id = 0;
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
                    println!(
                      "Failed to load a program object with a program binary: {:?} renderer {}\n{}",
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
                println!(
                    "Failed to link shader program: {:?}\n{}",
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

    pub fn get_uniform_location(&self, program: &Program, name: &str) -> UniformLocation {
        UniformLocation(self.gl.get_uniform_location(program.id, name))
    }

    pub fn set_uniform_2f(&self, uniform: UniformLocation, x: f32, y: f32) {
        debug_assert!(self.inside_frame);
        let UniformLocation(location) = uniform;
        self.gl.uniform_2f(location, x, y);
    }

    pub fn set_uniforms(
        &self,
        program: &Program,
        transform: &Transform3D<f32>,
        mode: i32,
    ) {
        debug_assert!(self.inside_frame);
        self.gl
            .uniform_matrix_4fv(program.u_transform, false, &transform.to_row_major_array());
        self.gl
            .uniform_1f(program.u_device_pixel_ratio, self.device_pixel_ratio);
        self.gl
            .uniform_1i(program.u_mode, mode);
    }

    pub fn create_pbo(&mut self) -> PBO {
        let id = self.gl.gen_buffers(1)[0];
        PBO { id }
    }

    pub fn delete_pbo(&mut self, mut pbo: PBO) {
        self.gl.delete_buffers(&[pbo.id]);
        pbo.id = 0;
    }

    pub fn bind_pbo(&mut self, pbo: Option<&PBO>) {
        debug_assert!(self.inside_frame);
        let pbo_id = pbo.map_or(0, |pbo| pbo.id);

        if self.bound_pbo != pbo_id {
            self.bound_pbo = pbo_id;

            self.gl.bind_buffer(gl::PIXEL_UNPACK_BUFFER, pbo_id);
        }
    }

    pub fn update_pbo_data<T>(&mut self, data: &[T]) {
        debug_assert!(self.inside_frame);
        debug_assert_ne!(self.bound_pbo, 0);

        gl::buffer_data(&*self.gl, gl::PIXEL_UNPACK_BUFFER, data, gl::STREAM_DRAW);
    }

    pub fn orphan_pbo(&mut self, new_size: usize) {
        debug_assert!(self.inside_frame);
        debug_assert_ne!(self.bound_pbo, 0);

        self.gl.buffer_data_untyped(
            gl::PIXEL_UNPACK_BUFFER,
            new_size as isize,
            ptr::null(),
            gl::STREAM_DRAW,
        );
    }

    pub fn update_texture_from_pbo(
        &mut self,
        texture: &Texture,
        x0: u32,
        y0: u32,
        width: u32,
        height: u32,
        layer_index: i32,
        stride: Option<u32>,
        offset: usize,
    ) {
        debug_assert!(self.inside_frame);

        let (gl_format, bpp, data_type) = match texture.format {
            ImageFormat::A8 => (GL_FORMAT_A, 1, gl::UNSIGNED_BYTE),
            ImageFormat::RGB8 => (gl::RGB, 3, gl::UNSIGNED_BYTE),
            ImageFormat::BGRA8 => (get_gl_format_bgra(self.gl()), 4, gl::UNSIGNED_BYTE),
            ImageFormat::RG8 => (gl::RG, 2, gl::UNSIGNED_BYTE),
            ImageFormat::RGBAF32 => (gl::RGBA, 16, gl::FLOAT),
            ImageFormat::Invalid => unreachable!(),
        };

        let row_length = match stride {
            Some(value) => value / bpp,
            None => width,
        };

        if let Some(..) = stride {
            self.gl
                .pixel_store_i(gl::UNPACK_ROW_LENGTH, row_length as gl::GLint);
        }

        self.bind_texture(DEFAULT_TEXTURE, texture);

        match texture.target {
            gl::TEXTURE_2D_ARRAY => {
                self.gl.tex_sub_image_3d_pbo(
                    texture.target,
                    0,
                    x0 as gl::GLint,
                    y0 as gl::GLint,
                    layer_index,
                    width as gl::GLint,
                    height as gl::GLint,
                    1,
                    gl_format,
                    data_type,
                    offset,
                );
            }
            gl::TEXTURE_2D | gl::TEXTURE_RECTANGLE | gl::TEXTURE_EXTERNAL_OES => {
                self.gl.tex_sub_image_2d_pbo(
                    texture.target,
                    0,
                    x0 as gl::GLint,
                    y0 as gl::GLint,
                    width as gl::GLint,
                    height as gl::GLint,
                    gl_format,
                    data_type,
                    offset,
                );
            }
            _ => panic!("BUG: Unexpected texture target!"),
        }

        // Reset row length to 0, otherwise the stride would apply to all texture uploads.
        if let Some(..) = stride {
            self.gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0 as gl::GLint);
        }
    }

    pub fn read_pixels(&mut self, width: i32, height: i32) -> Vec<u8> {
        self.gl.read_pixels(
            0, 0, 
            width as i32, height as i32,
            gl::RGBA,
            gl::UNSIGNED_BYTE
        )
    }

    pub fn bind_vao(&mut self, vao: &VAO) {
        debug_assert!(self.inside_frame);

        if self.bound_vao != vao.id {
            self.bound_vao = vao.id;
            self.gl.bind_vertex_array(vao.id);
        }
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

        let instance_stride = descriptor.instance_stride();
        let vao_id = self.gl.gen_vertex_arrays(1)[0];

        self.gl.bind_vertex_array(vao_id);

        descriptor.bind(self.gl(), main_vbo_id, instance_vbo_id);
        ibo_id.bind(self.gl()); // force it to be a part of VAO

        let vao = VAO {
            id: vao_id,
            ibo_id,
            main_vbo_id,
            instance_vbo_id,
            instance_stride: instance_stride as usize,
            owns_vertices_and_indices,
        };

        self.gl.bind_vertex_array(0);

        vao
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
        debug_assert!(self.inside_frame);
        debug_assert_eq!(self.bound_vao, vao.id);

        vao.main_vbo_id.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, vertices, usage_hint.to_gl());
    }

    pub fn update_vao_instances<V>(
        &mut self,
        vao: &VAO,
        instances: &[V],
        usage_hint: VertexUsageHint,
    ) {
        debug_assert!(self.inside_frame);
        debug_assert_eq!(self.bound_vao, vao.id);
        debug_assert_eq!(vao.instance_stride as usize, mem::size_of::<V>());

        vao.instance_vbo_id.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, instances, usage_hint.to_gl());
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

    pub fn draw_triangles_u32(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(
            gl::TRIANGLES,
            index_count,
            gl::UNSIGNED_INT,
            first_vertex as u32 * 4,
        );
    }

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
    pub fn set_blend_mode_min(&self) {
        self.gl
            .blend_func_separate(gl::ONE, gl::ONE, gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MIN, gl::FUNC_ADD);
    }
    pub fn set_blend_mode_subpixel_pass0(&self) {
        self.gl.blend_func(gl::ZERO, gl::ONE_MINUS_SRC_COLOR);
    }
    pub fn set_blend_mode_subpixel_pass1(&self) {
        self.gl.blend_func(gl::ONE, gl::ONE);
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
}

/// return (gl_internal_format, gl_format)
fn gl_texture_formats_for_image_format(
    gl: &gl::Gl,
    format: ImageFormat,
) -> (gl::GLint, gl::GLuint) {
    match format {
        ImageFormat::A8 => if cfg!(any(target_arch = "arm", target_arch = "aarch64")) {
            (get_gl_format_bgra(gl) as gl::GLint, get_gl_format_bgra(gl))
        } else {
            (GL_FORMAT_A as gl::GLint, GL_FORMAT_A)
        },
        ImageFormat::RGB8 => (gl::RGB as gl::GLint, gl::RGB),
        ImageFormat::BGRA8 => match gl.get_type() {
            gl::GlType::Gl => (gl::RGBA as gl::GLint, get_gl_format_bgra(gl)),
            gl::GlType::Gles => (get_gl_format_bgra(gl) as gl::GLint, get_gl_format_bgra(gl)),
        },
        ImageFormat::RGBAF32 => (gl::RGBA32F as gl::GLint, gl::RGBA),
        ImageFormat::RG8 => (gl::RG8 as gl::GLint, gl::RG),
        ImageFormat::Invalid => unreachable!(),
    }
}

fn gl_type_for_texture_format(format: ImageFormat) -> gl::GLuint {
    match format {
        ImageFormat::RGBAF32 => gl::FLOAT,
        _ => gl::UNSIGNED_BYTE,
    }
}

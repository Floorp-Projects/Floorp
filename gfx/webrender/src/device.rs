/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use euclid::Matrix4D;
use fnv::FnvHasher;
use gleam::gl;
use internal_types::{PackedVertex, RenderTargetMode, TextureSampler, DEFAULT_TEXTURE};
use internal_types::{BlurAttribute, ClearAttribute, ClipAttribute, VertexAttribute};
use internal_types::{DebugFontVertex, DebugColorVertex};
//use notify::{self, Watcher};
use super::shader_source;
use std::collections::HashMap;
use std::fs::File;
use std::hash::BuildHasherDefault;
use std::io::Read;
use std::iter::repeat;
use std::mem;
use std::path::PathBuf;
use std::rc::Rc;
//use std::sync::mpsc::{channel, Sender};
//use std::thread;
use webrender_traits::{ColorF, ImageFormat};
use webrender_traits::{DeviceIntPoint, DeviceIntRect, DeviceIntSize, DeviceUintSize};

#[derive(Debug, Copy, Clone)]
pub struct FrameId(usize);

#[cfg(not(any(target_arch = "arm", target_arch = "aarch64")))]
const GL_FORMAT_A: gl::GLuint = gl::RED;

#[cfg(any(target_arch = "arm", target_arch = "aarch64"))]
const GL_FORMAT_A: gl::GLuint = gl::ALPHA;

const GL_FORMAT_BGRA_GL: gl::GLuint = gl::BGRA;

const GL_FORMAT_BGRA_GLES: gl::GLuint = gl::BGRA_EXT;

const SHADER_VERSION_GL: &'static str = "#version 150\n";

const SHADER_VERSION_GLES: &'static str = "#version 300 es\n";

static SHADER_PREAMBLE: &'static str = "shared";

#[repr(u32)]
pub enum DepthFunction {
    Less = gl::LESS,
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

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
pub enum VertexFormat {
    Triangles,
    Rectangles,
    DebugFont,
    DebugColor,
    Clear,
    Blur,
    Clip,
}

enum FBOTarget {
    Read,
    Draw,
}

fn get_gl_format_bgra(gl: &gl::Gl) -> gl::GLuint {
    match gl.get_type() {
        gl::GlType::Gl => {
            GL_FORMAT_BGRA_GL
        }
        gl::GlType::Gles => {
            GL_FORMAT_BGRA_GLES
        }
    }
}

fn get_shader_version(gl: &gl::Gl) -> &'static str {
    match gl.get_type() {
        gl::GlType::Gl => {
            SHADER_VERSION_GL
        }
        gl::GlType::Gles => {
            SHADER_VERSION_GLES
        }
    }
}

fn get_optional_shader_source(shader_name: &str, base_path: &Option<PathBuf>) -> Option<String> {
    if let Some(ref base) = *base_path {
        let shader_path = base.join(&format!("{}.glsl", shader_name));
        if shader_path.exists() {
            let mut source = String::new();
            File::open(&shader_path).unwrap().read_to_string(&mut source).unwrap();
            return Some(source);
        }
    }

    shader_source::SHADERS.get(shader_name).and_then(|s| Some((*s).to_owned()))
}

fn get_shader_source(shader_name: &str, base_path: &Option<PathBuf>) -> String {
    get_optional_shader_source(shader_name, base_path)
        .expect(&format!("Couldn't get required shader: {}", shader_name))
}

pub trait FileWatcherHandler : Send {
    fn file_changed(&self, path: PathBuf);
}

impl VertexFormat {
    fn bind(&self, gl: &gl::Gl, main: VBOId, instance: VBOId, offset: gl::GLuint, instance_stride: gl::GLint) {
        main.bind(gl);

        match *self {
            VertexFormat::DebugFont => {
                gl.enable_vertex_attrib_array(VertexAttribute::Position as gl::GLuint);
                gl.enable_vertex_attrib_array(VertexAttribute::Color as gl::GLuint);
                gl.enable_vertex_attrib_array(VertexAttribute::ColorTexCoord as gl::GLuint);

                gl.vertex_attrib_divisor(VertexAttribute::Position as gl::GLuint, 0);
                gl.vertex_attrib_divisor(VertexAttribute::Color as gl::GLuint, 0);
                gl.vertex_attrib_divisor(VertexAttribute::ColorTexCoord as gl::GLuint, 0);

                let vertex_stride = mem::size_of::<DebugFontVertex>() as gl::GLuint;

                gl.vertex_attrib_pointer(VertexAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0 + vertex_stride * offset);
                gl.vertex_attrib_pointer(VertexAttribute::Color as gl::GLuint,
                                          4,
                                          gl::UNSIGNED_BYTE,
                                          true,
                                          vertex_stride as gl::GLint,
                                          8 + vertex_stride * offset);
                gl.vertex_attrib_pointer(VertexAttribute::ColorTexCoord as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          12 + vertex_stride * offset);
            }
            VertexFormat::DebugColor => {
                gl.enable_vertex_attrib_array(VertexAttribute::Position as gl::GLuint);
                gl.enable_vertex_attrib_array(VertexAttribute::Color as gl::GLuint);

                gl.vertex_attrib_divisor(VertexAttribute::Position as gl::GLuint, 0);
                gl.vertex_attrib_divisor(VertexAttribute::Color as gl::GLuint, 0);

                let vertex_stride = mem::size_of::<DebugColorVertex>() as gl::GLuint;

                gl.vertex_attrib_pointer(VertexAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0 + vertex_stride * offset);
                gl.vertex_attrib_pointer(VertexAttribute::Color as gl::GLuint,
                                          4,
                                          gl::UNSIGNED_BYTE,
                                          true,
                                          vertex_stride as gl::GLint,
                                          8 + vertex_stride * offset);
            }
            VertexFormat::Rectangles |
            VertexFormat::Triangles => {
                let vertex_stride = mem::size_of::<PackedVertex>() as gl::GLuint;
                gl.enable_vertex_attrib_array(VertexAttribute::Position as gl::GLuint);
                gl.vertex_attrib_divisor(VertexAttribute::Position as gl::GLuint, 0);

                gl.vertex_attrib_pointer(VertexAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0);

                instance.bind(gl);
                let mut offset = 0;

                for &attrib in [VertexAttribute::GlobalPrimId,
                                VertexAttribute::PrimitiveAddress,
                                VertexAttribute::TaskIndex,
                                VertexAttribute::ClipTaskIndex,
                                VertexAttribute::LayerIndex,
                                VertexAttribute::ElementIndex,
                                VertexAttribute::ZIndex,
                               ].into_iter() {
                    gl.enable_vertex_attrib_array(attrib as gl::GLuint);
                    gl.vertex_attrib_divisor(attrib as gl::GLuint, 1);
                    gl.vertex_attrib_i_pointer(attrib as gl::GLuint,
                                                1,
                                                gl::INT,
                                                instance_stride,
                                                offset);
                    offset += 4;
                }

                gl.enable_vertex_attrib_array(VertexAttribute::UserData as gl::GLuint);
                gl.vertex_attrib_divisor(VertexAttribute::UserData as gl::GLuint, 1);
                gl.vertex_attrib_i_pointer(VertexAttribute::UserData as gl::GLuint,
                                            2,
                                            gl::INT,
                                            instance_stride,
                                            offset);
            }
            VertexFormat::Clear => {
                let vertex_stride = mem::size_of::<PackedVertex>() as gl::GLuint;
                gl.enable_vertex_attrib_array(ClearAttribute::Position as gl::GLuint);
                gl.vertex_attrib_divisor(ClearAttribute::Position as gl::GLuint, 0);

                gl.vertex_attrib_pointer(ClearAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0);

                instance.bind(gl);

                gl.enable_vertex_attrib_array(ClearAttribute::Rectangle as gl::GLuint);
                gl.vertex_attrib_divisor(ClearAttribute::Rectangle as gl::GLuint, 1);
                gl.vertex_attrib_i_pointer(ClearAttribute::Rectangle as gl::GLuint,
                                            4,
                                            gl::INT,
                                            instance_stride,
                                            0);
            }
            VertexFormat::Blur => {
                let vertex_stride = mem::size_of::<PackedVertex>() as gl::GLuint;
                gl.enable_vertex_attrib_array(BlurAttribute::Position as gl::GLuint);
                gl.vertex_attrib_divisor(BlurAttribute::Position as gl::GLuint, 0);

                gl.vertex_attrib_pointer(BlurAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0);

                instance.bind(gl);

                for (i, &attrib) in [BlurAttribute::RenderTaskIndex,
                                     BlurAttribute::SourceTaskIndex,
                                     BlurAttribute::Direction,
                                    ].into_iter().enumerate() {
                    gl.enable_vertex_attrib_array(attrib as gl::GLuint);
                    gl.vertex_attrib_divisor(attrib as gl::GLuint, 1);
                    gl.vertex_attrib_i_pointer(attrib as gl::GLuint,
                                                1,
                                                gl::INT,
                                                instance_stride,
                                                (i * 4) as gl::GLuint);
                }
            }
            VertexFormat::Clip => {
                let vertex_stride = mem::size_of::<PackedVertex>() as gl::GLuint;
                gl.enable_vertex_attrib_array(ClipAttribute::Position as gl::GLuint);
                gl.vertex_attrib_divisor(ClipAttribute::Position as gl::GLuint, 0);

                gl.vertex_attrib_pointer(ClipAttribute::Position as gl::GLuint,
                                          2,
                                          gl::FLOAT,
                                          false,
                                          vertex_stride as gl::GLint,
                                          0);

                instance.bind(gl);

                for (i, &attrib) in [ClipAttribute::RenderTaskIndex,
                                     ClipAttribute::LayerIndex,
                                     ClipAttribute::DataIndex,
                                     ClipAttribute::SegmentIndex,
                                    ].into_iter().enumerate() {
                    gl.enable_vertex_attrib_array(attrib as gl::GLuint);
                    gl.vertex_attrib_divisor(attrib as gl::GLuint, 1);
                    gl.vertex_attrib_i_pointer(attrib as gl::GLuint,
                                                1,
                                                gl::INT,
                                                instance_stride,
                                                (i * 4) as gl::GLuint);
                }
            }
        }
    }
}

impl TextureId {
    pub fn bind(&self, gl: &gl::Gl) {
        gl.bind_texture(self.target, self.name);
    }

    pub fn new(name: gl::GLuint, texture_target: TextureTarget) -> TextureId {
        TextureId {
            name: name,
            target: texture_target.to_gl_target(),
        }
    }

    pub fn invalid() -> TextureId {
        TextureId {
            name: 0,
            target: gl::TEXTURE_2D,
        }
    }

    pub fn is_valid(&self) -> bool { *self != TextureId::invalid() }
}

impl ProgramId {
    fn bind(&self, gl: &gl::Gl) {
        gl.use_program(self.0);
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

impl UBOId {
    fn _bind(&self, gl: &gl::Gl) {
        gl.bind_buffer(gl::UNIFORM_BUFFER, self.0);
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

struct Texture {
    gl: Rc<gl::Gl>,
    id: gl::GLuint,
    format: ImageFormat,
    width: u32,
    height: u32,
    filter: TextureFilter,
    mode: RenderTargetMode,
    fbo_ids: Vec<FBOId>,
}

impl Drop for Texture {
    fn drop(&mut self) {
        if !self.fbo_ids.is_empty() {
            let fbo_ids: Vec<_> = self.fbo_ids.iter().map(|&FBOId(fbo_id)| fbo_id).collect();
            self.gl.delete_framebuffers(&fbo_ids[..]);
        }
        self.gl.delete_textures(&[self.id]);
    }
}

struct Program {
    gl: Rc<gl::Gl>,
    id: gl::GLuint,
    u_transform: gl::GLint,
    u_device_pixel_ratio: gl::GLint,
    name: String,
    vs_source: String,
    fs_source: String,
    prefix: Option<String>,
    vs_id: Option<gl::GLuint>,
    fs_id: Option<gl::GLuint>,
}

impl Program {
    fn attach_and_bind_shaders(&mut self,
                               vs_id: gl::GLuint,
                               fs_id: gl::GLuint,
                               vertex_format: VertexFormat) -> Result<(), ShaderError> {
        self.gl.attach_shader(self.id, vs_id);
        self.gl.attach_shader(self.id, fs_id);

        match vertex_format {
            VertexFormat::Triangles | VertexFormat::Rectangles |
            VertexFormat::DebugFont |  VertexFormat::DebugColor => {
                self.gl.bind_attrib_location(self.id, VertexAttribute::Position as gl::GLuint, "aPosition");
                self.gl.bind_attrib_location(self.id, VertexAttribute::Color as gl::GLuint, "aColor");
                self.gl.bind_attrib_location(self.id, VertexAttribute::ColorTexCoord as gl::GLuint, "aColorTexCoord");

                self.gl.bind_attrib_location(self.id, VertexAttribute::GlobalPrimId as gl::GLuint, "aGlobalPrimId");
                self.gl.bind_attrib_location(self.id, VertexAttribute::PrimitiveAddress as gl::GLuint, "aPrimitiveAddress");
                self.gl.bind_attrib_location(self.id, VertexAttribute::TaskIndex as gl::GLuint, "aTaskIndex");
                self.gl.bind_attrib_location(self.id, VertexAttribute::ClipTaskIndex as gl::GLuint, "aClipTaskIndex");
                self.gl.bind_attrib_location(self.id, VertexAttribute::LayerIndex as gl::GLuint, "aLayerIndex");
                self.gl.bind_attrib_location(self.id, VertexAttribute::ElementIndex as gl::GLuint, "aElementIndex");
                self.gl.bind_attrib_location(self.id, VertexAttribute::UserData as gl::GLuint, "aUserData");
                self.gl.bind_attrib_location(self.id, VertexAttribute::ZIndex as gl::GLuint, "aZIndex");
            }
            VertexFormat::Clear => {
                self.gl.bind_attrib_location(self.id, ClearAttribute::Position as gl::GLuint, "aPosition");
                self.gl.bind_attrib_location(self.id, ClearAttribute::Rectangle as gl::GLuint, "aClearRectangle");
            }
            VertexFormat::Blur => {
                self.gl.bind_attrib_location(self.id, BlurAttribute::Position as gl::GLuint, "aPosition");
                self.gl.bind_attrib_location(self.id, BlurAttribute::RenderTaskIndex as gl::GLuint, "aBlurRenderTaskIndex");
                self.gl.bind_attrib_location(self.id, BlurAttribute::SourceTaskIndex as gl::GLuint, "aBlurSourceTaskIndex");
                self.gl.bind_attrib_location(self.id, BlurAttribute::Direction as gl::GLuint, "aBlurDirection");
            }
            VertexFormat::Clip => {
                self.gl.bind_attrib_location(self.id, ClipAttribute::Position as gl::GLuint, "aPosition");
                self.gl.bind_attrib_location(self.id, ClipAttribute::RenderTaskIndex as gl::GLuint, "aClipRenderTaskIndex");
                self.gl.bind_attrib_location(self.id, ClipAttribute::LayerIndex as gl::GLuint, "aClipLayerIndex");
                self.gl.bind_attrib_location(self.id, ClipAttribute::DataIndex as gl::GLuint, "aClipDataIndex");
                self.gl.bind_attrib_location(self.id, ClipAttribute::SegmentIndex as gl::GLuint, "aClipSegmentIndex");
            }
        }

        self.gl.link_program(self.id);
        if self.gl.get_program_iv(self.id, gl::LINK_STATUS) == (0 as gl::GLint) {
            let error_log = self.gl.get_program_info_log(self.id);
            println!("Failed to link shader program: {}", error_log);
            self.gl.detach_shader(self.id, vs_id);
            self.gl.detach_shader(self.id, fs_id);
            return Err(ShaderError::Link(error_log));
        }

        Ok(())
    }
}

impl Drop for Program {
    fn drop(&mut self) {
        self.gl.delete_program(self.id);
    }
}

struct VAO {
    gl: Rc<gl::Gl>,
    id: gl::GLuint,
    ibo_id: IBOId,
    main_vbo_id: VBOId,
    instance_vbo_id: VBOId,
    instance_stride: gl::GLint,
    owns_indices: bool,
    owns_vertices: bool,
    owns_instances: bool,
}

impl Drop for VAO {
    fn drop(&mut self) {
        self.gl.delete_vertex_arrays(&[self.id]);

        if self.owns_indices {
            // todo(gw): maybe make these their own type with hashmap?
            self.gl.delete_buffers(&[self.ibo_id.0]);
        }
        if self.owns_vertices {
            self.gl.delete_buffers(&[self.main_vbo_id.0]);
        }
        if self.owns_instances {
            self.gl.delete_buffers(&[self.instance_vbo_id.0])
        }
    }
}

#[derive(PartialEq, Eq, Hash, PartialOrd, Ord, Debug, Copy, Clone)]
pub struct TextureId {
    name: gl::GLuint,
    target: gl::GLuint,
}

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct ProgramId(pub gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct VAOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct FBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct VBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
struct IBOId(gl::GLuint);

#[derive(PartialEq, Eq, Hash, Debug, Copy, Clone)]
pub struct UBOId(gl::GLuint);

const MAX_EVENTS_PER_FRAME: usize = 256;
const MAX_PROFILE_FRAMES: usize = 4;

pub trait NamedTag {
    fn get_label(&self) -> &str;
}

#[derive(Debug, Clone)]
pub struct GpuSample<T> {
    pub tag: T,
    pub time_ns: u64,
}

pub struct GpuFrameProfile<T> {
    gl: Rc<gl::Gl>,
    queries: Vec<gl::GLuint>,
    samples: Vec<GpuSample<T>>,
    next_query: usize,
    pending_query: gl::GLuint,
    frame_id: FrameId,
}

impl<T> GpuFrameProfile<T> {
    fn new(gl: Rc<gl::Gl>) -> GpuFrameProfile<T> {
        match gl.get_type() {
            gl::GlType::Gl => {
                let queries = gl.gen_queries(MAX_EVENTS_PER_FRAME as gl::GLint);
                GpuFrameProfile {
                    gl: gl,
                    queries: queries,
                    samples: Vec::new(),
                    next_query: 0,
                    pending_query: 0,
                    frame_id: FrameId(0),
                }
            }
            gl::GlType::Gles => {
                GpuFrameProfile {
                    gl: gl,
                    queries: Vec::new(),
                    samples: Vec::new(),
                    next_query: 0,
                    pending_query: 0,
                    frame_id: FrameId(0),
                }
            }
        }
    }

    fn begin_frame(&mut self, frame_id: FrameId) {
        self.frame_id = frame_id;
        self.next_query = 0;
        self.pending_query = 0;
        self.samples.clear();
    }

    fn end_frame(&mut self) {
        match self.gl.get_type() {
            gl::GlType::Gl => {
                if self.pending_query != 0 {
                    self.gl.end_query(gl::TIME_ELAPSED);
                }
            }
            gl::GlType::Gles => {},
        }
    }

    fn add_marker(&mut self, tag: T) -> GpuMarker
    where T: NamedTag {
        match self.gl.get_type() {
            gl::GlType::Gl => {
                self.add_marker_gl(tag)
            }
            gl::GlType::Gles => {
                self.add_marker_gles(tag)
            }
        }
    }

    fn add_marker_gl(&mut self, tag: T) -> GpuMarker
    where T: NamedTag {
        if self.pending_query != 0 {
            self.gl.end_query(gl::TIME_ELAPSED);
        }

        let marker = GpuMarker::new(&self.gl, tag.get_label());

        if self.next_query < MAX_EVENTS_PER_FRAME {
            self.pending_query = self.queries[self.next_query];
            self.gl.begin_query(gl::TIME_ELAPSED, self.pending_query);
            self.samples.push(GpuSample {
                tag: tag,
                time_ns: 0,
            });
        } else {
            self.pending_query = 0;
        }

        self.next_query += 1;
        marker
    }

    fn add_marker_gles(&mut self, tag: T) -> GpuMarker
    where T: NamedTag {
        let marker = GpuMarker::new(&self.gl, tag.get_label());
        self.samples.push(GpuSample {
            tag: tag,
            time_ns: 0,
        });
        marker
    }

    fn is_valid(&self) -> bool {
        self.next_query > 0 && self.next_query <= MAX_EVENTS_PER_FRAME
    }

    fn build_samples(&mut self) -> Vec<GpuSample<T>> {
        match self.gl.get_type() {
            gl::GlType::Gl => {
                self.build_samples_gl()
            }
            gl::GlType::Gles => {
                self.build_samples_gles()
            }
        }
    }

    fn build_samples_gl(&mut self) -> Vec<GpuSample<T>> {
        for (index, sample) in self.samples.iter_mut().enumerate() {
            sample.time_ns = self.gl.get_query_object_ui64v(self.queries[index], gl::QUERY_RESULT)
        }

        mem::replace(&mut self.samples, Vec::new())
    }

    fn build_samples_gles(&mut self) -> Vec<GpuSample<T>> {
        mem::replace(&mut self.samples, Vec::new())
    }
}

impl<T> Drop for GpuFrameProfile<T> {
    fn drop(&mut self) {
        match self.gl.get_type() {
            gl::GlType::Gl =>  {
                self.gl.delete_queries(&self.queries);
            }
            gl::GlType::Gles => {},
        }
    }
}

pub struct GpuProfiler<T> {
    frames: [GpuFrameProfile<T>; MAX_PROFILE_FRAMES],
    next_frame: usize,
}

impl<T> GpuProfiler<T> {
    pub fn new(gl: &Rc<gl::Gl>) -> GpuProfiler<T> {
        GpuProfiler {
            next_frame: 0,
            frames: [
                      GpuFrameProfile::new(Rc::clone(gl)),
                      GpuFrameProfile::new(Rc::clone(gl)),
                      GpuFrameProfile::new(Rc::clone(gl)),
                      GpuFrameProfile::new(Rc::clone(gl)),
                    ],
        }
    }

    pub fn build_samples(&mut self) -> Option<(FrameId, Vec<GpuSample<T>>)> {
        let frame = &mut self.frames[self.next_frame];
        if frame.is_valid() {
            Some((frame.frame_id, frame.build_samples()))
        } else {
            None
        }
    }

    pub fn begin_frame(&mut self, frame_id: FrameId) {
        let frame = &mut self.frames[self.next_frame];
        frame.begin_frame(frame_id);
    }

    pub fn end_frame(&mut self) {
        let frame = &mut self.frames[self.next_frame];
        frame.end_frame();
        self.next_frame = (self.next_frame + 1) % MAX_PROFILE_FRAMES;
    }

    pub fn add_marker(&mut self, tag: T) -> GpuMarker
    where T: NamedTag {
        self.frames[self.next_frame].add_marker(tag)
    }
}

#[must_use]
pub struct GpuMarker{
    gl: Rc<gl::Gl>,
}

impl GpuMarker {
    pub fn new(gl: &Rc<gl::Gl>, message: &str) -> GpuMarker {
        match gl.get_type() {
            gl::GlType::Gl =>  {
                gl.push_group_marker_ext(message);
                GpuMarker{
                    gl: Rc::clone(gl),
                }
            }
            gl::GlType::Gles => {
                GpuMarker{
                    gl: Rc::clone(gl),
                }
            }
        }
    }

    pub fn fire(gl: &gl::Gl, message: &str) {
        match gl.get_type() {
            gl::GlType::Gl =>  {
                gl.insert_event_marker_ext(message);
            }
            gl::GlType::Gles => {},
        }
    }
}

#[cfg(not(any(target_arch="arm", target_arch="aarch64")))]
impl Drop for GpuMarker {
    fn drop(&mut self) {
        match self.gl.get_type() {
            gl::GlType::Gl =>  {
                self.gl.pop_group_marker_ext();
            }
            gl::GlType::Gles => {},
        }
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

// TODO(gw): Fix up notify cargo deps and re-enable this!
/*
enum FileWatcherCmd {
    AddWatch(PathBuf),
    Exit,
}

struct FileWatcherThread {
    api_tx: Sender<FileWatcherCmd>,
}

impl FileWatcherThread {
    fn new(handler: Box<FileWatcherHandler>) -> FileWatcherThread {
        let (api_tx, api_rx) = channel();

        thread::spawn(move || {

            let (watch_tx, watch_rx) = channel();

            enum Request {
                Watcher(notify::Event),
                Command(FileWatcherCmd),
            }

            let mut file_watcher: notify::RecommendedWatcher = notify::Watcher::new(watch_tx).unwrap();

            loop {
                let request = {
                    let receiver_from_api = &api_rx;
                    let receiver_from_watcher = &watch_rx;
                    select! {
                        msg = receiver_from_api.recv() => Request::Command(msg.unwrap()),
                        msg = receiver_from_watcher.recv() => Request::Watcher(msg.unwrap())
                    }
                };

                match request {
                    Request::Watcher(event) => {
                        handler.file_changed(event.path.unwrap());
                    }
                    Request::Command(cmd) => {
                        match cmd {
                            FileWatcherCmd::AddWatch(path) => {
                                file_watcher.watch(path).ok();
                            }
                            FileWatcherCmd::Exit => {
                                break;
                            }
                        }
                    }
                }
            }
        });

        FileWatcherThread {
            api_tx: api_tx,
        }
    }

    fn exit(&self) {
        self.api_tx.send(FileWatcherCmd::Exit).ok();
    }

    fn add_watch(&self, path: PathBuf) {
        self.api_tx.send(FileWatcherCmd::AddWatch(path)).ok();
    }
}
*/

pub struct Capabilities {
    pub max_ubo_size: usize,
    pub supports_multisampling: bool,
}

#[derive(Clone, Debug)]
pub enum ShaderError {
    Compilation(String, String), // name, error mssage
    Link(String), // error message
}

pub struct Device {
    gl: Rc<gl::Gl>,
    // device state
    bound_textures: [TextureId; 16],
    bound_program: ProgramId,
    bound_vao: VAOId,
    bound_read_fbo: FBOId,
    bound_draw_fbo: FBOId,
    default_read_fbo: gl::GLuint,
    default_draw_fbo: gl::GLuint,
    device_pixel_ratio: f32,

    // HW or API capabilties
    capabilities: Capabilities,

    // debug
    inside_frame: bool,

    // resources
    resource_override_path: Option<PathBuf>,
    textures: HashMap<TextureId, Texture, BuildHasherDefault<FnvHasher>>,
    programs: HashMap<ProgramId, Program, BuildHasherDefault<FnvHasher>>,
    vaos: HashMap<VAOId, VAO, BuildHasherDefault<FnvHasher>>,

    // misc.
    shader_preamble: String,
    //file_watcher: FileWatcherThread,

    // Used on android only
    #[allow(dead_code)]
    next_vao_id: gl::GLuint,

    max_texture_size: u32,

    // Frame counter. This is used to map between CPU
    // frames and GPU frames.
    frame_id: FrameId,
}

impl Device {
    pub fn new(gl: Rc<gl::Gl>,
               resource_override_path: Option<PathBuf>,
               _file_changed_handler: Box<FileWatcherHandler>) -> Device {
        //let file_watcher = FileWatcherThread::new(file_changed_handler);

        let shader_preamble = get_shader_source(SHADER_PREAMBLE, &resource_override_path);
        //file_watcher.add_watch(resource_path);

        let max_ubo_size = gl.get_integer_v(gl::MAX_UNIFORM_BLOCK_SIZE) as usize;
        let max_texture_size = gl.get_integer_v(gl::MAX_TEXTURE_SIZE) as u32;

        Device {
            gl: gl,
            resource_override_path: resource_override_path,
            // This is initialized to 1 by default, but it is set
            // every frame by the call to begin_frame().
            device_pixel_ratio: 1.0,
            inside_frame: false,

            capabilities: Capabilities {
                max_ubo_size: max_ubo_size,
                supports_multisampling: false, //TODO
            },

            bound_textures: [ TextureId::invalid(); 16 ],
            bound_program: ProgramId(0),
            bound_vao: VAOId(0),
            bound_read_fbo: FBOId(0),
            bound_draw_fbo: FBOId(0),
            default_read_fbo: 0,
            default_draw_fbo: 0,

            textures: HashMap::default(),
            programs: HashMap::default(),
            vaos: HashMap::default(),

            shader_preamble: shader_preamble,

            next_vao_id: 1,
            //file_watcher: file_watcher,

            max_texture_size: max_texture_size,
            frame_id: FrameId(0),
        }
    }

    pub fn gl(&self) -> &gl::Gl {
        &*self.gl
    }

    pub fn rc_gl(&self) -> &Rc<gl::Gl> {
        &self.gl
    }

    pub fn max_texture_size(&self) -> u32 {
        self.max_texture_size
    }

    pub fn get_capabilities(&self) -> &Capabilities {
        &self.capabilities
    }

    pub fn compile_shader(gl: &gl::Gl,
                          name: &str,
                          source_str: &str,
                          shader_type: gl::GLenum,
                          shader_preamble: &[String])
                          -> Result<gl::GLuint, ShaderError> {
        debug!("compile {:?}", name);

        let mut s = String::new();
        s.push_str(get_shader_version(gl));
        for prefix in shader_preamble {
            s.push_str(prefix);
        }
        s.push_str(source_str);

        let id = gl.create_shader(shader_type);
        let mut source = Vec::new();
        source.extend_from_slice(s.as_bytes());
        gl.shader_source(id, &[&source[..]]);
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

    pub fn begin_frame(&mut self, device_pixel_ratio: f32) -> FrameId {
        debug_assert!(!self.inside_frame);
        self.inside_frame = true;
        self.device_pixel_ratio = device_pixel_ratio;

        // Retrive the currently set FBO.
        let default_read_fbo = self.gl.get_integer_v(gl::READ_FRAMEBUFFER_BINDING);
        self.default_read_fbo = default_read_fbo as gl::GLuint;
        let default_draw_fbo = self.gl.get_integer_v(gl::DRAW_FRAMEBUFFER_BINDING);
        self.default_draw_fbo = default_draw_fbo as gl::GLuint;

        // Texture state
        for i in 0..self.bound_textures.len() {
            self.bound_textures[i] = TextureId::invalid();
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        // Shader state
        self.bound_program = ProgramId(0);
        self.gl.use_program(0);

        // Vertex state
        self.bound_vao = VAOId(0);
        self.clear_vertex_array();

        // FBO state
        self.bound_read_fbo = FBOId(self.default_read_fbo);
        self.bound_draw_fbo = FBOId(self.default_draw_fbo);

        // Pixel op state
        self.gl.pixel_store_i(gl::UNPACK_ALIGNMENT, 1);

        // Default is sampler 0, always
        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id
    }

    pub fn bind_texture(&mut self,
                        sampler: TextureSampler,
                        texture_id: TextureId) {
        debug_assert!(self.inside_frame);

        let sampler_index = sampler as usize;
        if self.bound_textures[sampler_index] != texture_id {
            self.bound_textures[sampler_index] = texture_id;
            self.gl.active_texture(gl::TEXTURE0 + sampler_index as gl::GLuint);
            texture_id.bind(self.gl());
            self.gl.active_texture(gl::TEXTURE0);
        }
    }

    pub fn bind_read_target(&mut self, texture_id: Option<(TextureId, i32)>) {
        debug_assert!(self.inside_frame);

        let fbo_id = texture_id.map_or(FBOId(self.default_read_fbo), |texture_id| {
            self.textures.get(&texture_id.0).unwrap().fbo_ids[texture_id.1 as usize]
        });

        if self.bound_read_fbo != fbo_id {
            self.bound_read_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Read);
        }
    }

    pub fn bind_draw_target(&mut self,
                            texture_id: Option<(TextureId, i32)>,
                            dimensions: Option<DeviceUintSize>) {
        debug_assert!(self.inside_frame);

        let fbo_id = texture_id.map_or(FBOId(self.default_draw_fbo), |texture_id| {
            self.textures.get(&texture_id.0).unwrap().fbo_ids[texture_id.1 as usize]
        });

        if self.bound_draw_fbo != fbo_id {
            self.bound_draw_fbo = fbo_id;
            fbo_id.bind(self.gl(), FBOTarget::Draw);
        }

        if let Some(dimensions) = dimensions {
            self.gl.viewport(0, 0, dimensions.width as gl::GLint, dimensions.height as gl::GLint);
        }
    }

    pub fn bind_program(&mut self,
                        program_id: ProgramId,
                        projection: &Matrix4D<f32>) {
        debug_assert!(self.inside_frame);

        if self.bound_program != program_id {
            self.bound_program = program_id;
            program_id.bind(&*self.gl);
        }

        let program = self.programs.get(&program_id).unwrap();
        self.set_uniforms(program,
                          projection,
                          self.device_pixel_ratio);
    }

    pub fn create_texture_ids(&mut self,
                              count: i32,
                              target: TextureTarget) -> Vec<TextureId> {
        let id_list = self.gl.gen_textures(count);
        let mut texture_ids = Vec::new();

        for id in id_list {
            let texture_id = TextureId {
                name: id,
                target: target.to_gl_target(),
            };

            let texture = Texture {
                gl: Rc::clone(&self.gl),
                id: id,
                width: 0,
                height: 0,
                format: ImageFormat::Invalid,
                filter: TextureFilter::Nearest,
                mode: RenderTargetMode::None,
                fbo_ids: vec![],
            };

            debug_assert!(self.textures.contains_key(&texture_id) == false);
            self.textures.insert(texture_id, texture);

            texture_ids.push(texture_id);
        }

        texture_ids
    }

    pub fn get_texture_dimensions(&self, texture_id: TextureId) -> DeviceUintSize {
        let texture = &self.textures[&texture_id];
        DeviceUintSize::new(texture.width, texture.height)
    }

    fn set_texture_parameters(&mut self, target: gl::GLuint, filter: TextureFilter) {
        let filter = match filter {
            TextureFilter::Nearest => {
                gl::NEAREST
            }
            TextureFilter::Linear => {
                gl::LINEAR
            }
        };

        self.gl.tex_parameter_i(target, gl::TEXTURE_MAG_FILTER, filter as gl::GLint);
        self.gl.tex_parameter_i(target, gl::TEXTURE_MIN_FILTER, filter as gl::GLint);

        self.gl.tex_parameter_i(target, gl::TEXTURE_WRAP_S, gl::CLAMP_TO_EDGE as gl::GLint);
        self.gl.tex_parameter_i(target, gl::TEXTURE_WRAP_T, gl::CLAMP_TO_EDGE as gl::GLint);
    }

    fn upload_texture_image(&mut self,
                            target: gl::GLuint,
                            width: u32,
                            height: u32,
                            internal_format: u32,
                            format: u32,
                            type_: u32,
                            pixels: Option<&[u8]>) {
        self.gl.tex_image_2d(target,
                              0,
                              internal_format as gl::GLint,
                              width as gl::GLint, height as gl::GLint,
                              0,
                              format,
                              type_,
                              pixels);
    }

    pub fn init_texture(&mut self,
                        texture_id: TextureId,
                        width: u32,
                        height: u32,
                        format: ImageFormat,
                        filter: TextureFilter,
                        mode: RenderTargetMode,
                        pixels: Option<&[u8]>) {
        debug_assert!(self.inside_frame);

        {
            let texture = self.textures.get_mut(&texture_id).expect("Didn't find texture!");
            texture.format = format;
            texture.width = width;
            texture.height = height;
            texture.filter = filter;
            texture.mode = mode;
        }

        let (internal_format, gl_format) = gl_texture_formats_for_image_format(self.gl(), format);
        let type_ = gl_type_for_texture_format(format);

        match mode {
            RenderTargetMode::SimpleRenderTarget => {
                self.bind_texture(DEFAULT_TEXTURE, texture_id);
                self.set_texture_parameters(texture_id.target, filter);
                self.upload_texture_image(texture_id.target,
                                          width,
                                          height,
                                          internal_format as u32,
                                          gl_format,
                                          type_,
                                          None);
                self.create_fbo_for_texture_if_necessary(texture_id, None);
            }
            RenderTargetMode::LayerRenderTarget(layer_count) => {
                self.bind_texture(DEFAULT_TEXTURE, texture_id);
                self.set_texture_parameters(texture_id.target, filter);
                self.create_fbo_for_texture_if_necessary(texture_id, Some(layer_count));
            }
            RenderTargetMode::None => {
                self.bind_texture(DEFAULT_TEXTURE, texture_id);
                self.set_texture_parameters(texture_id.target, filter);
                let expanded_data: Vec<u8>;
                let actual_pixels = if pixels.is_some() &&
                                       format == ImageFormat::A8 &&
                                       cfg!(any(target_arch="arm", target_arch="aarch64")) {
                    expanded_data = pixels.unwrap().iter().flat_map(|&byte| repeat(byte).take(4)).collect();
                    Some(expanded_data.as_slice())
                } else {
                    pixels
                };
                self.upload_texture_image(texture_id.target,
                                          width,
                                          height,
                                          internal_format as u32,
                                          gl_format,
                                          type_,
                                          actual_pixels);
            }
        }
    }

    pub fn get_render_target_layer_count(&self, texture_id: TextureId) -> usize {
        self.textures[&texture_id].fbo_ids.len()
    }

    pub fn create_fbo_for_texture_if_necessary(&mut self,
                                               texture_id: TextureId,
                                               layer_count: Option<i32>) {
        let texture = self.textures.get_mut(&texture_id).unwrap();

        match layer_count {
            Some(layer_count) => {
                debug_assert!(layer_count > 0);

                // If we have enough layers allocated already, just use them.
                // TODO(gw): Probably worth removing some after a while if
                //           there is a surplus?
                let current_layer_count = texture.fbo_ids.len() as i32;
                if current_layer_count >= layer_count {
                    return;
                }

                let (internal_format, gl_format) = gl_texture_formats_for_image_format(&*self.gl, texture.format);
                let type_ = gl_type_for_texture_format(texture.format);

                self.gl.tex_image_3d(texture_id.target,
                                      0,
                                      internal_format as gl::GLint,
                                      texture.width as gl::GLint,
                                      texture.height as gl::GLint,
                                      layer_count,
                                      0,
                                      gl_format,
                                      type_,
                                      None);

                let needed_layer_count = layer_count - current_layer_count;
                let new_fbos = self.gl.gen_framebuffers(needed_layer_count);
                texture.fbo_ids.extend(new_fbos.into_iter().map(|id| FBOId(id)));

                for (fbo_index, fbo_id) in texture.fbo_ids.iter().enumerate() {
                    self.gl.bind_framebuffer(gl::FRAMEBUFFER, fbo_id.0);
                    self.gl.framebuffer_texture_layer(gl::FRAMEBUFFER,
                                                       gl::COLOR_ATTACHMENT0,
                                                       texture_id.name,
                                                       0,
                                                       fbo_index as gl::GLint);

                    // TODO(gw): Share depth render buffer between FBOs to
                    //           save memory!
                    // TODO(gw): Free these renderbuffers on exit!
                    let renderbuffer_ids = self.gl.gen_renderbuffers(1);
                    let depth_rb = renderbuffer_ids[0];
                    self.gl.bind_renderbuffer(gl::RENDERBUFFER, depth_rb);
                    self.gl.renderbuffer_storage(gl::RENDERBUFFER,
                                                  gl::DEPTH_COMPONENT24,
                                                  texture.width as gl::GLsizei,
                                                  texture.height as gl::GLsizei);
                    self.gl.framebuffer_renderbuffer(gl::FRAMEBUFFER,
                                                      gl::DEPTH_ATTACHMENT,
                                                      gl::RENDERBUFFER,
                                                      depth_rb);
                }
            }
            None => {
                debug_assert!(texture.fbo_ids.len() == 0 || texture.fbo_ids.len() == 1);
                if texture.fbo_ids.is_empty() {
                    let new_fbo = self.gl.gen_framebuffers(1)[0];

                    self.gl.bind_framebuffer(gl::FRAMEBUFFER, new_fbo);

                    self.gl.framebuffer_texture_2d(gl::FRAMEBUFFER,
                                                    gl::COLOR_ATTACHMENT0,
                                                    texture_id.target,
                                                    texture_id.name,
                                                    0);

                    texture.fbo_ids.push(FBOId(new_fbo));
                }
            }
        }

        // TODO(gw): Hack! Modify the code above to use the normal binding interfaces the device exposes.
        self.gl.bind_framebuffer(gl::READ_FRAMEBUFFER, self.bound_read_fbo.0);
        self.gl.bind_framebuffer(gl::DRAW_FRAMEBUFFER, self.bound_draw_fbo.0);
    }

    pub fn blit_render_target(&mut self,
                              src_texture: Option<(TextureId, i32)>,
                              src_rect: Option<DeviceIntRect>,
                              dest_rect: DeviceIntRect) {
        debug_assert!(self.inside_frame);

        let src_rect = src_rect.unwrap_or_else(|| {
            let texture = self.textures.get(&src_texture.unwrap().0).expect("unknown texture id!");
            DeviceIntRect::new(DeviceIntPoint::zero(),
                               DeviceIntSize::new(texture.width as gl::GLint,
                                                  texture.height as gl::GLint))
        });

        self.bind_read_target(src_texture);

        self.gl.blit_framebuffer(src_rect.origin.x,
                                  src_rect.origin.y,
                                  src_rect.origin.x + src_rect.size.width,
                                  src_rect.origin.y + src_rect.size.height,
                                  dest_rect.origin.x,
                                  dest_rect.origin.y,
                                  dest_rect.origin.x + dest_rect.size.width,
                                  dest_rect.origin.y + dest_rect.size.height,
                                  gl::COLOR_BUFFER_BIT,
                                  gl::LINEAR);
    }

    pub fn resize_texture(&mut self,
                          texture_id: TextureId,
                          new_width: u32,
                          new_height: u32,
                          format: ImageFormat,
                          filter: TextureFilter,
                          mode: RenderTargetMode) {
        debug_assert!(self.inside_frame);

        let old_size = self.get_texture_dimensions(texture_id);

        let temp_texture_id = self.create_texture_ids(1, TextureTarget::Default)[0];
        self.init_texture(temp_texture_id, old_size.width, old_size.height, format, filter, mode, None);
        self.create_fbo_for_texture_if_necessary(temp_texture_id, None);

        self.bind_read_target(Some((texture_id, 0)));
        self.bind_texture(DEFAULT_TEXTURE, temp_texture_id);

        self.gl.copy_tex_sub_image_2d(temp_texture_id.target,
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       old_size.width as i32,
                                       old_size.height as i32);

        self.deinit_texture(texture_id);
        self.init_texture(texture_id, new_width, new_height, format, filter, mode, None);
        self.create_fbo_for_texture_if_necessary(texture_id, None);
        self.bind_read_target(Some((temp_texture_id, 0)));
        self.bind_texture(DEFAULT_TEXTURE, texture_id);

        self.gl.copy_tex_sub_image_2d(texture_id.target,
                                       0,
                                       0,
                                       0,
                                       0,
                                       0,
                                       old_size.width as i32,
                                       old_size.height as i32);

        self.bind_read_target(None);
        self.deinit_texture(temp_texture_id);
    }

    pub fn deinit_texture(&mut self, texture_id: TextureId) {
        debug_assert!(self.inside_frame);

        self.bind_texture(DEFAULT_TEXTURE, texture_id);

        let texture = self.textures.get_mut(&texture_id).unwrap();
        let (internal_format, gl_format) = gl_texture_formats_for_image_format(&*self.gl, texture.format);
        let type_ = gl_type_for_texture_format(texture.format);

        self.gl.tex_image_2d(texture_id.target,
                              0,
                              internal_format,
                              0,
                              0,
                              0,
                              gl_format,
                              type_,
                              None);

        if !texture.fbo_ids.is_empty() {
            let fbo_ids: Vec<_> = texture.fbo_ids.iter().map(|&FBOId(fbo_id)| fbo_id).collect();
            self.gl.delete_framebuffers(&fbo_ids[..]);
        }

        texture.format = ImageFormat::Invalid;
        texture.width = 0;
        texture.height = 0;
        texture.fbo_ids.clear();
    }

    pub fn create_program(&mut self,
                          base_filename: &str,
                          include_filename: &str,
                          vertex_format: VertexFormat) -> Result<ProgramId, ShaderError> {
        self.create_program_with_prefix(base_filename, &[include_filename], None, vertex_format)
    }

    pub fn create_program_with_prefix(&mut self,
                                      base_filename: &str,
                                      include_filenames: &[&str],
                                      prefix: Option<String>,
                                      vertex_format: VertexFormat) -> Result<ProgramId, ShaderError> {
        debug_assert!(self.inside_frame);

        let pid = self.gl.create_program();

        let mut vs_name = String::from(base_filename);
        vs_name.push_str(".vs");
        let mut fs_name = String::from(base_filename);
        fs_name.push_str(".fs");

        let mut include = format!("// Base shader: {}\n", base_filename);
        for inc_filename in include_filenames {
            let src = get_shader_source(inc_filename, &self.resource_override_path);
            include.push_str(&src);
        }

        if let Some(shared_src) = get_optional_shader_source(base_filename, &self.resource_override_path) {
            include.push_str(&shared_src);
        }

        let program = Program {
            gl: Rc::clone(&self.gl),
            name: base_filename.to_owned(),
            id: pid,
            u_transform: -1,
            u_device_pixel_ratio: -1,
            vs_source: get_shader_source(&vs_name, &self.resource_override_path),
            fs_source: get_shader_source(&fs_name, &self.resource_override_path),
            prefix: prefix,
            vs_id: None,
            fs_id: None,
        };

        let program_id = ProgramId(pid);

        debug_assert!(self.programs.contains_key(&program_id) == false);
        self.programs.insert(program_id, program);

        try!{ self.load_program(program_id, include, vertex_format) };

        Ok(program_id)
    }

    fn load_program(&mut self,
                    program_id: ProgramId,
                    include: String,
                    vertex_format: VertexFormat) -> Result<(), ShaderError> {
        debug_assert!(self.inside_frame);

        let program = self.programs.get_mut(&program_id).unwrap();

        let mut vs_preamble = Vec::new();
        let mut fs_preamble = Vec::new();

        vs_preamble.push("#define WR_VERTEX_SHADER\n".to_owned());
        fs_preamble.push("#define WR_FRAGMENT_SHADER\n".to_owned());

        if let Some(ref prefix) = program.prefix {
            vs_preamble.push(prefix.clone());
            fs_preamble.push(prefix.clone());
        }

        vs_preamble.push(self.shader_preamble.to_owned());
        fs_preamble.push(self.shader_preamble.to_owned());

        vs_preamble.push(include.clone());
        fs_preamble.push(include);

        // todo(gw): store shader ids so they can be freed!
        let vs_id = try!{ Device::compile_shader(&*self.gl,
                                                 &program.name,
                                                 &program.vs_source,
                                                 gl::VERTEX_SHADER,
                                                 &vs_preamble) };
        let fs_id = try!{ Device::compile_shader(&*self.gl,
                                                 &program.name,
                                                 &program.fs_source,
                                                 gl::FRAGMENT_SHADER,
                                                 &fs_preamble) };

        if let Some(vs_id) = program.vs_id {
            self.gl.detach_shader(program.id, vs_id);
        }

        if let Some(fs_id) = program.fs_id {
            self.gl.detach_shader(program.id, fs_id);
        }

        if let Err(bind_error) = program.attach_and_bind_shaders(vs_id, fs_id, vertex_format) {
            if let (Some(vs_id), Some(fs_id)) = (program.vs_id, program.fs_id) {
                try! { program.attach_and_bind_shaders(vs_id, fs_id, vertex_format) };
            } else {
               return Err(bind_error);
            }
        } else {
            if let Some(vs_id) = program.vs_id {
                self.gl.delete_shader(vs_id);
            }

            if let Some(fs_id) = program.fs_id {
                self.gl.delete_shader(fs_id);
            }

            program.vs_id = Some(vs_id);
            program.fs_id = Some(fs_id);
        }

        program.u_transform = self.gl.get_uniform_location(program.id, "uTransform");
        program.u_device_pixel_ratio = self.gl.get_uniform_location(program.id, "uDevicePixelRatio");

        program_id.bind(&*self.gl);
        let u_color_0 = self.gl.get_uniform_location(program.id, "sColor0");
        if u_color_0 != -1 {
            self.gl.uniform_1i(u_color_0, TextureSampler::Color0 as i32);
        }
        let u_color1 = self.gl.get_uniform_location(program.id, "sColor1");
        if u_color1 != -1 {
            self.gl.uniform_1i(u_color1, TextureSampler::Color1 as i32);
        }
        let u_color_2 = self.gl.get_uniform_location(program.id, "sColor2");
        if u_color_2 != -1 {
            self.gl.uniform_1i(u_color_2, TextureSampler::Color2 as i32);
        }
        let u_noise = self.gl.get_uniform_location(program.id, "sDither");
        if u_noise != -1 {
            self.gl.uniform_1i(u_noise, TextureSampler::Dither as i32);
        }
        let u_cache_a8 = self.gl.get_uniform_location(program.id, "sCacheA8");
        if u_cache_a8 != -1 {
            self.gl.uniform_1i(u_cache_a8, TextureSampler::CacheA8 as i32);
        }
        let u_cache_rgba8 = self.gl.get_uniform_location(program.id, "sCacheRGBA8");
        if u_cache_rgba8 != -1 {
            self.gl.uniform_1i(u_cache_rgba8, TextureSampler::CacheRGBA8 as i32);
        }

        let u_layers = self.gl.get_uniform_location(program.id, "sLayers");
        if u_layers != -1 {
            self.gl.uniform_1i(u_layers, TextureSampler::Layers as i32);
        }

        let u_tasks = self.gl.get_uniform_location(program.id, "sRenderTasks");
        if u_tasks != -1 {
            self.gl.uniform_1i(u_tasks, TextureSampler::RenderTasks as i32);
        }

        let u_prim_geom = self.gl.get_uniform_location(program.id, "sPrimGeometry");
        if u_prim_geom != -1 {
            self.gl.uniform_1i(u_prim_geom, TextureSampler::Geometry as i32);
        }

        let u_data16 = self.gl.get_uniform_location(program.id, "sData16");
        if u_data16 != -1 {
            self.gl.uniform_1i(u_data16, TextureSampler::Data16 as i32);
        }

        let u_data32 = self.gl.get_uniform_location(program.id, "sData32");
        if u_data32 != -1 {
            self.gl.uniform_1i(u_data32, TextureSampler::Data32 as i32);
        }

        let u_data64 = self.gl.get_uniform_location(program.id, "sData64");
        if u_data64 != -1 {
            self.gl.uniform_1i(u_data64, TextureSampler::Data64 as i32);
        }

        let u_data128 = self.gl.get_uniform_location(program.id, "sData128");
        if u_data128 != -1 {
            self.gl.uniform_1i(u_data128, TextureSampler::Data128    as i32);
        }

        let u_resource_rects = self.gl.get_uniform_location(program.id, "sResourceRects");
        if u_resource_rects != -1 {
            self.gl.uniform_1i(u_resource_rects, TextureSampler::ResourceRects as i32);
        }

        let u_gradients = self.gl.get_uniform_location(program.id, "sGradients");
        if u_gradients != -1 {
            self.gl.uniform_1i(u_gradients, TextureSampler::Gradients as i32);
        }

        Ok(())
    }

/*
    pub fn refresh_shader(&mut self, path: PathBuf) {
        let mut vs_preamble_path = self.resource_path.clone();
        vs_preamble_path.push(VERTEX_SHADER_PREAMBLE);

        let mut fs_preamble_path = self.resource_path.clone();
        fs_preamble_path.push(FRAGMENT_SHADER_PREAMBLE);

        let mut refresh_all = false;

        if path == vs_preamble_path {
            let mut f = File::open(&vs_preamble_path).unwrap();
            self.vertex_shader_preamble = String::new();
            f.read_to_string(&mut self.vertex_shader_preamble).unwrap();
            refresh_all = true;
        }

        if path == fs_preamble_path {
            let mut f = File::open(&fs_preamble_path).unwrap();
            self.fragment_shader_preamble = String::new();
            f.read_to_string(&mut self.fragment_shader_preamble).unwrap();
            refresh_all = true;
        }

        let mut programs_to_update = Vec::new();

        for (program_id, program) in &mut self.programs {
            if refresh_all || program.vs_path == path || program.fs_path == path {
                programs_to_update.push(*program_id)
            }
        }

        for program_id in programs_to_update {
            self.load_program(program_id, false);
        }
    }*/

    pub fn get_uniform_location(&self, program_id: ProgramId, name: &str) -> UniformLocation {
        let ProgramId(program_id) = program_id;
        UniformLocation(self.gl.get_uniform_location(program_id, name))
    }

    pub fn set_uniform_2f(&self, uniform: UniformLocation, x: f32, y: f32) {
        debug_assert!(self.inside_frame);
        let UniformLocation(location) = uniform;
        self.gl.uniform_2f(location, x, y);
    }

    fn set_uniforms(&self,
                    program: &Program,
                    transform: &Matrix4D<f32>,
                    device_pixel_ratio: f32) {
        debug_assert!(self.inside_frame);
        self.gl.uniform_matrix_4fv(program.u_transform,
                               false,
                               &transform.to_row_major_array());
        self.gl.uniform_1f(program.u_device_pixel_ratio, device_pixel_ratio);
    }

    fn update_image_for_2d_texture(&mut self,
                                   target: gl::GLuint,
                                   x0: gl::GLint,
                                   y0: gl::GLint,
                                   width: gl::GLint,
                                   height: gl::GLint,
                                   format: gl::GLuint,
                                   data: &[u8]) {
        self.gl.tex_sub_image_2d(target,
                                  0,
                                  x0, y0,
                                  width, height,
                                  format,
                                  gl::UNSIGNED_BYTE,
                                  data);
    }

    pub fn update_texture(&mut self,
                          texture_id: TextureId,
                          x0: u32,
                          y0: u32,
                          width: u32,
                          height: u32,
                          stride: Option<u32>,
                          data: &[u8]) {
        debug_assert!(self.inside_frame);

        let mut expanded_data = Vec::new();

        let (gl_format, bpp, data) = match self.textures.get(&texture_id).unwrap().format {
            ImageFormat::A8 => {
                if cfg!(any(target_arch="arm", target_arch="aarch64")) {
                    for byte in data {
                        expanded_data.push(*byte);
                        expanded_data.push(*byte);
                        expanded_data.push(*byte);
                        expanded_data.push(*byte);
                    }
                    (get_gl_format_bgra(self.gl()), 4, expanded_data.as_slice())
                } else {
                    (GL_FORMAT_A, 1, data)
                }
            }
            ImageFormat::RGB8 => (gl::RGB, 3, data),
            ImageFormat::RGBA8 => (get_gl_format_bgra(self.gl()), 4, data),
            ImageFormat::RG8 => (gl::RG, 2, data),
            ImageFormat::Invalid | ImageFormat::RGBAF32 => unreachable!(),
        };

        let row_length = match stride {
            Some(value) => value / bpp,
            None => width,
        };

        // Take the stride into account for all rows, except the last one.
        let len = bpp * row_length * (height - 1)
                + width * bpp;

        assert!(data.len() as u32 >= len);
        let data = &data[0..len as usize];

        if let Some(..) = stride {
            self.gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, row_length as gl::GLint);
        }

        self.bind_texture(DEFAULT_TEXTURE, texture_id);
        self.update_image_for_2d_texture(texture_id.target,
                                         x0 as gl::GLint,
                                         y0 as gl::GLint,
                                         width as gl::GLint,
                                         height as gl::GLint,
                                         gl_format,
                                         data);

        // Reset row length to 0, otherwise the stride would apply to all texture uploads.
        if let Some(..) = stride {
            self.gl.pixel_store_i(gl::UNPACK_ROW_LENGTH, 0 as gl::GLint);
        }
    }

    fn clear_vertex_array(&mut self) {
        debug_assert!(self.inside_frame);
        self.gl.bind_vertex_array(0);
    }

    pub fn bind_vao(&mut self, vao_id: VAOId) {
        debug_assert!(self.inside_frame);

        if self.bound_vao != vao_id {
            self.bound_vao = vao_id;

            let VAOId(id) = vao_id;
            self.gl.bind_vertex_array(id);
        }
    }

    fn create_vao_with_vbos(&mut self,
                            format: VertexFormat,
                            main_vbo_id: VBOId,
                            instance_vbo_id: VBOId,
                            ibo_id: IBOId,
                            vertex_offset: gl::GLuint,
                            instance_stride: gl::GLint,
                            owns_vertices: bool,
                            owns_instances: bool,
                            owns_indices: bool)
                            -> VAOId {
        debug_assert!(self.inside_frame);

        let vao_ids = self.gl.gen_vertex_arrays(1);
        let vao_id = vao_ids[0];

        self.gl.bind_vertex_array(vao_id);

        format.bind(self.gl(), main_vbo_id, instance_vbo_id, vertex_offset, instance_stride);
        ibo_id.bind(self.gl()); // force it to be a part of VAO

        let vao = VAO {
            gl: Rc::clone(&self.gl),
            id: vao_id,
            ibo_id: ibo_id,
            main_vbo_id: main_vbo_id,
            instance_vbo_id: instance_vbo_id,
            instance_stride: instance_stride,
            owns_indices: owns_indices,
            owns_vertices: owns_vertices,
            owns_instances: owns_instances,
        };

        self.gl.bind_vertex_array(0);

        let vao_id = VAOId(vao_id);

        debug_assert!(!self.vaos.contains_key(&vao_id));
        self.vaos.insert(vao_id, vao);

        vao_id
    }

    pub fn create_vao(&mut self, format: VertexFormat, inst_stride: gl::GLint) -> VAOId {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(3);
        let ibo_id = IBOId(buffer_ids[0]);
        let main_vbo_id = VBOId(buffer_ids[1]);
        let intance_vbo_id = VBOId(buffer_ids[2]);

        self.create_vao_with_vbos(format, main_vbo_id, intance_vbo_id, ibo_id, 0, inst_stride, true, true, true)
    }

    pub fn create_vao_with_new_instances(&mut self, format: VertexFormat, inst_stride: gl::GLint,
                                         base_vao: VAOId) -> VAOId {
        debug_assert!(self.inside_frame);

        let buffer_ids = self.gl.gen_buffers(1);
        let intance_vbo_id = VBOId(buffer_ids[0]);
        let (main_vbo_id, ibo_id) = {
            let vao = self.vaos.get(&base_vao).unwrap();
            (vao.main_vbo_id, vao.ibo_id)
        };

        self.create_vao_with_vbos(format, main_vbo_id, intance_vbo_id, ibo_id, 0, inst_stride, false, true, false)
    }

    pub fn update_vao_main_vertices<V>(&mut self,
                                       vao_id: VAOId,
                                       vertices: &[V],
                                       usage_hint: VertexUsageHint) {
        debug_assert!(self.inside_frame);

        let vao = self.vaos.get(&vao_id).unwrap();
        debug_assert_eq!(self.bound_vao, vao_id);

        vao.main_vbo_id.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, vertices, usage_hint.to_gl());
    }

    pub fn update_vao_instances<V>(&mut self,
                                   vao_id: VAOId,
                                   instances: &[V],
                                   usage_hint: VertexUsageHint) {
        debug_assert!(self.inside_frame);

        let vao = self.vaos.get(&vao_id).unwrap();
        debug_assert_eq!(self.bound_vao, vao_id);
        debug_assert_eq!(vao.instance_stride as usize, mem::size_of::<V>());

        vao.instance_vbo_id.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ARRAY_BUFFER, instances, usage_hint.to_gl());
    }

    pub fn update_vao_indices<I>(&mut self,
                                 vao_id: VAOId,
                                 indices: &[I],
                                 usage_hint: VertexUsageHint) {
        debug_assert!(self.inside_frame);

        let vao = self.vaos.get(&vao_id).unwrap();
        debug_assert_eq!(self.bound_vao, vao_id);

        vao.ibo_id.bind(self.gl());
        gl::buffer_data(self.gl(), gl::ELEMENT_ARRAY_BUFFER, indices, usage_hint.to_gl());
    }

    pub fn draw_triangles_u16(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(gl::TRIANGLES,
                               index_count,
                               gl::UNSIGNED_SHORT,
                               first_vertex as u32 * 2);
    }

    pub fn draw_triangles_u32(&mut self, first_vertex: i32, index_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements(gl::TRIANGLES,
                               index_count,
                               gl::UNSIGNED_INT,
                               first_vertex as u32 * 4);
    }

    pub fn draw_nonindexed_lines(&mut self, first_vertex: i32, vertex_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_arrays(gl::LINES,
                             first_vertex,
                             vertex_count);
    }

    pub fn draw_indexed_triangles_instanced_u16(&mut self,
                                                index_count: i32,
                                                instance_count: i32) {
        debug_assert!(self.inside_frame);
        self.gl.draw_elements_instanced(gl::TRIANGLES, index_count, gl::UNSIGNED_SHORT, 0, instance_count);
    }

    pub fn end_frame(&mut self) {
        self.bind_draw_target(None, None);
        self.bind_read_target(None);

        debug_assert!(self.inside_frame);
        self.inside_frame = false;

        self.gl.bind_texture(gl::TEXTURE_2D, 0);
        self.gl.use_program(0);

        for i in 0..self.bound_textures.len() {
            self.gl.active_texture(gl::TEXTURE0 + i as gl::GLuint);
            self.gl.bind_texture(gl::TEXTURE_2D, 0);
        }

        self.gl.active_texture(gl::TEXTURE0);

        self.frame_id.0 += 1;
    }

    pub fn assign_ubo_binding(&self, program_id: ProgramId, name: &str, value: u32) -> u32 {
        let index = self.gl.get_uniform_block_index(program_id.0, name);
        self.gl.uniform_block_binding(program_id.0, index, value);
        index
    }

    pub fn create_ubo<T>(&self, data: &[T], binding: u32) -> UBOId {
        let ubo = self.gl.gen_buffers(1)[0];
        self.gl.bind_buffer(gl::UNIFORM_BUFFER, ubo);
        gl::buffer_data(self.gl(), gl::UNIFORM_BUFFER, data, gl::STATIC_DRAW);
        self.gl.bind_buffer_base(gl::UNIFORM_BUFFER, binding, ubo);
        UBOId(ubo)
    }

    pub fn reset_ubo(&self, binding: u32) {
        self.gl.bind_buffer(gl::UNIFORM_BUFFER, 0);
        self.gl.bind_buffer_base(gl::UNIFORM_BUFFER, binding, 0);
    }

    pub fn delete_buffer(&self, buffer: UBOId) {
        self.gl.delete_buffers(&[buffer.0]);
    }

    #[cfg(target_os = "android")]
    pub fn set_multisample(&self, enable: bool) {
    }

    #[cfg(not(target_os = "android"))]
    pub fn set_multisample(&self, enable: bool) {
        if self.capabilities.supports_multisampling {
            if enable {
                self.gl.enable(gl::MULTISAMPLE);
            } else {
                self.gl.disable(gl::MULTISAMPLE);
            }
        }
    }

    pub fn clear_target(&self,
                        color: Option<[f32; 4]>,
                        depth: Option<f32>) {
        let mut clear_bits = 0;

        if let Some(color) = color {
            self.gl.clear_color(color[0], color[1], color[2], color[3]);
            clear_bits |= gl::COLOR_BUFFER_BIT;
        }

        if let Some(depth) = depth {
            self.gl.clear_depth(depth as f64);
            clear_bits |= gl::DEPTH_BUFFER_BIT;
        }

        if clear_bits != 0 {
            self.gl.clear(clear_bits);
        }
    }

    pub fn clear_target_rect(&self,
                             color: Option<[f32; 4]>,
                             depth: Option<f32>,
                             rect: DeviceIntRect) {
        let mut clear_bits = 0;

        if let Some(color) = color {
            self.gl.clear_color(color[0], color[1], color[2], color[3]);
            clear_bits |= gl::COLOR_BUFFER_BIT;
        }

        if let Some(depth) = depth {
            self.gl.clear_depth(depth as f64);
            clear_bits |= gl::DEPTH_BUFFER_BIT;
        }

        if clear_bits != 0 {
            self.gl.enable(gl::SCISSOR_TEST);
            self.gl.scissor(rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
            self.gl.clear(clear_bits);
            self.gl.disable(gl::SCISSOR_TEST);
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

    pub fn set_blend_mode_alpha(&self) {
        self.gl.blend_func_separate(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA,
                                    gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }

    pub fn set_blend_mode_subpixel(&self, color: ColorF) {
        self.gl.blend_color(color.r, color.g, color.b, color.a);
        self.gl.blend_func(gl::CONSTANT_COLOR, gl::ONE_MINUS_SRC_COLOR);
    }

    pub fn set_blend_mode_multiply(&self) {
        self.gl.blend_func_separate(gl::ZERO, gl::SRC_COLOR,
                                     gl::ZERO, gl::SRC_ALPHA);
        self.gl.blend_equation(gl::FUNC_ADD);
    }
    pub fn set_blend_mode_max(&self) {
        self.gl.blend_func_separate(gl::ONE, gl::ONE,
                                     gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MAX, gl::FUNC_ADD);
    }
    pub fn set_blend_mode_min(&self) {
        self.gl.blend_func_separate(gl::ONE, gl::ONE,
                                     gl::ONE, gl::ONE);
        self.gl.blend_equation_separate(gl::MIN, gl::FUNC_ADD);
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        //self.file_watcher.exit();
    }
}

/// return (gl_internal_format, gl_format)
fn gl_texture_formats_for_image_format(gl: &gl::Gl, format: ImageFormat) -> (gl::GLint, gl::GLuint) {
    match format {
        ImageFormat::A8 => {
            if cfg!(any(target_arch="arm", target_arch="aarch64")) {
                (get_gl_format_bgra(gl) as gl::GLint, get_gl_format_bgra(gl))
            } else {
                (GL_FORMAT_A as gl::GLint, GL_FORMAT_A)
            }
        },
        ImageFormat::RGB8 => (gl::RGB as gl::GLint, gl::RGB),
        ImageFormat::RGBA8 => {
            match gl.get_type() {
                gl::GlType::Gl =>  {
                    (gl::RGBA as gl::GLint, get_gl_format_bgra(gl))
                }
                gl::GlType::Gles => {
                    (get_gl_format_bgra(gl) as gl::GLint, get_gl_format_bgra(gl))
                }
            }
        }
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


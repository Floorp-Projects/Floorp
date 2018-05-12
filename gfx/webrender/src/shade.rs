/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{
    YUV_COLOR_SPACES, YUV_FORMATS,
    YuvColorSpace, YuvFormat,
};
use batch::{BatchKey, BatchKind, BrushBatchKind, TransformBatchKind};
use device::{Device, Program, ShaderError};
use euclid::{Transform3D};
use glyph_rasterizer::GlyphFormat;
use renderer::{
    desc,
    MAX_VERTEX_TEXTURE_WIDTH,
    BlendMode, ImageBufferKind, RendererError, RendererOptions,
    TextureSampler, VertexArrayKind,
};
use util::TransformedRectKind;

use gleam::gl::GlType;
use time::precise_time_ns;


impl ImageBufferKind {
    pub(crate) fn get_feature_string(&self) -> &'static str {
        match *self {
            ImageBufferKind::Texture2D => "TEXTURE_2D",
            ImageBufferKind::Texture2DArray => "",
            ImageBufferKind::TextureRect => "TEXTURE_RECT",
            ImageBufferKind::TextureExternal => "TEXTURE_EXTERNAL",
        }
    }

    fn has_platform_support(&self, gl_type: &GlType) -> bool {
        match (*self, gl_type) {
            (ImageBufferKind::Texture2D, _) => true,
            (ImageBufferKind::Texture2DArray, _) => true,
            (ImageBufferKind::TextureRect, _) => true,
            (ImageBufferKind::TextureExternal, &GlType::Gles) => true,
            (ImageBufferKind::TextureExternal, &GlType::Gl) => false,
        }
    }
}

pub const IMAGE_BUFFER_KINDS: [ImageBufferKind; 4] = [
    ImageBufferKind::Texture2D,
    ImageBufferKind::TextureRect,
    ImageBufferKind::TextureExternal,
    ImageBufferKind::Texture2DArray,
];

const TRANSFORM_FEATURE: &str = "TRANSFORM";
const ALPHA_FEATURE: &str = "ALPHA_PASS";
const DITHERING_FEATURE: &str = "DITHERING";
const DUAL_SOURCE_FEATURE: &str = "DUAL_SOURCE_BLENDING";

pub(crate) enum ShaderKind {
    Primitive,
    Cache(VertexArrayKind),
    ClipCache,
    Brush,
    Text,
    #[allow(dead_code)]
    VectorStencil,
    #[allow(dead_code)]
    VectorCover,
}

pub struct LazilyCompiledShader {
    program: Option<Program>,
    name: &'static str,
    kind: ShaderKind,
    features: Vec<&'static str>,
}

impl LazilyCompiledShader {
    pub(crate) fn new(
        kind: ShaderKind,
        name: &'static str,
        features: &[&'static str],
        device: &mut Device,
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let mut shader = LazilyCompiledShader {
            program: None,
            name,
            kind,
            features: features.to_vec(),
        };

        if precache {
            let t0 = precise_time_ns();
            let program = shader.get(device)?;
            let t1 = precise_time_ns();
            device.bind_program(program);
            device.draw_triangles_u16(0, 3);
            let t2 = precise_time_ns();
            debug!("[C: {:.1} ms D: {:.1} ms] Precache {} {:?}",
                (t1 - t0) as f64 / 1000000.0,
                (t2 - t1) as f64 / 1000000.0,
                name,
                features
            );
        }

        Ok(shader)
    }

    pub fn bind(
        &mut self,
        device: &mut Device,
        projection: &Transform3D<f32>,
        renderer_errors: &mut Vec<RendererError>,
    ) {
        let program = match self.get(device) {
            Ok(program) => program,
            Err(e) => {
                renderer_errors.push(RendererError::from(e));
                return;
            }
        };
        device.bind_program(program);
        device.set_uniforms(program, projection);
    }

    fn get(&mut self, device: &mut Device) -> Result<&Program, ShaderError> {
        if self.program.is_none() {
            let program = match self.kind {
                ShaderKind::Primitive | ShaderKind::Brush | ShaderKind::Text => {
                    create_prim_shader(self.name,
                                       device,
                                       &self.features,
                                       VertexArrayKind::Primitive)
                }
                ShaderKind::Cache(format) => {
                    create_prim_shader(self.name,
                                       device,
                                       &self.features,
                                       format)
                }
                ShaderKind::VectorStencil => {
                    create_prim_shader(self.name,
                                       device,
                                       &self.features,
                                       VertexArrayKind::VectorStencil)
                }
                ShaderKind::VectorCover => {
                    create_prim_shader(self.name,
                                       device,
                                       &self.features,
                                       VertexArrayKind::VectorCover)
                }
                ShaderKind::ClipCache => {
                    create_clip_shader(self.name, device)
                }
            };
            self.program = Some(program?);
        }

        Ok(self.program.as_ref().unwrap())
    }

    fn deinit(self, device: &mut Device) {
        if let Some(program) = self.program {
            device.delete_program(program);
        }
    }
}

// A brush shader supports two modes:
// opaque:
//   Used for completely opaque primitives,
//   or inside segments of partially
//   opaque primitives. Assumes no need
//   for clip masks, AA etc.
// alpha:
//   Used for brush primitives in the alpha
//   pass. Assumes that AA should be applied
//   along the primitive edge, and also that
//   clip mask is present.
struct BrushShader {
    opaque: LazilyCompiledShader,
    alpha: LazilyCompiledShader,
    dual_source: Option<LazilyCompiledShader>,
}

impl BrushShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
        dual_source: bool,
    ) -> Result<Self, ShaderError> {
        let opaque = LazilyCompiledShader::new(
            ShaderKind::Brush,
            name,
            features,
            device,
            precache,
        )?;

        let mut alpha_features = features.to_vec();
        alpha_features.push(ALPHA_FEATURE);

        let alpha = LazilyCompiledShader::new(
            ShaderKind::Brush,
            name,
            &alpha_features,
            device,
            precache,
        )?;

        let dual_source = if dual_source {
            let mut dual_source_features = alpha_features.to_vec();
            dual_source_features.push(DUAL_SOURCE_FEATURE);

            let shader = LazilyCompiledShader::new(
                ShaderKind::Brush,
                name,
                &dual_source_features,
                device,
                precache,
            )?;

            Some(shader)
        } else {
            None
        };

        Ok(BrushShader {
            opaque,
            alpha,
            dual_source,
        })
    }

    fn get(&mut self, blend_mode: BlendMode) -> &mut LazilyCompiledShader {
        match blend_mode {
            BlendMode::None => &mut self.opaque,
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelWithBgColor => &mut self.alpha,
            BlendMode::SubpixelDualSource => {
                self.dual_source
                    .as_mut()
                    .expect("bug: no dual source shader loaded")
            }
        }
    }

    fn deinit(self, device: &mut Device) {
        self.opaque.deinit(device);
        self.alpha.deinit(device);
        if let Some(dual_source) = self.dual_source {
            dual_source.deinit(device);
        }
    }
}

struct PrimitiveShader {
    simple: LazilyCompiledShader,
    transform: LazilyCompiledShader,
}

impl PrimitiveShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let simple = LazilyCompiledShader::new(
            ShaderKind::Primitive,
            name,
            features,
            device,
            precache,
        )?;

        let mut transform_features = features.to_vec();
        transform_features.push(TRANSFORM_FEATURE);

        let transform = LazilyCompiledShader::new(
            ShaderKind::Primitive,
            name,
            &transform_features,
            device,
            precache,
        )?;

        Ok(PrimitiveShader { simple, transform })
    }

    fn get(&mut self, transform_kind: TransformedRectKind) -> &mut LazilyCompiledShader {
        match transform_kind {
            TransformedRectKind::AxisAligned => &mut self.simple,
            TransformedRectKind::Complex => &mut self.transform,
        }
    }

    fn deinit(self, device: &mut Device) {
        self.simple.deinit(device);
        self.transform.deinit(device);
    }
}

pub struct TextShader {
    simple: LazilyCompiledShader,
    transform: LazilyCompiledShader,
    glyph_transform: LazilyCompiledShader,
}

impl TextShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache: bool,
    ) -> Result<Self, ShaderError> {
        let simple = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            features,
            device,
            precache,
        )?;

        let mut transform_features = features.to_vec();
        transform_features.push("TRANSFORM");

        let transform = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            &transform_features,
            device,
            precache,
        )?;

        let mut glyph_transform_features = features.to_vec();
        glyph_transform_features.push("GLYPH_TRANSFORM");

        let glyph_transform = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            &glyph_transform_features,
            device,
            precache,
        )?;

        Ok(TextShader { simple, transform, glyph_transform })
    }

    pub fn get(
        &mut self,
        glyph_format: GlyphFormat,
        transform_kind: TransformedRectKind,
    ) -> &mut LazilyCompiledShader {
        match glyph_format {
            GlyphFormat::Alpha |
            GlyphFormat::Subpixel |
            GlyphFormat::Bitmap |
            GlyphFormat::ColorBitmap => match transform_kind {
                TransformedRectKind::AxisAligned => &mut self.simple,
                TransformedRectKind::Complex => &mut self.transform,
            }
            GlyphFormat::TransformedAlpha |
            GlyphFormat::TransformedSubpixel => &mut self.glyph_transform,
        }
    }

    fn deinit(self, device: &mut Device) {
        self.simple.deinit(device);
        self.transform.deinit(device);
        self.glyph_transform.deinit(device);
    }
}

fn create_prim_shader(
    name: &'static str,
    device: &mut Device,
    features: &[&'static str],
    vertex_format: VertexArrayKind,
) -> Result<Program, ShaderError> {
    let mut prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    for feature in features {
        prefix.push_str(&format!("#define WR_FEATURE_{}\n", feature));
    }

    debug!("PrimShader {}", name);

    let vertex_descriptor = match vertex_format {
        VertexArrayKind::Primitive => desc::PRIM_INSTANCES,
        VertexArrayKind::Blur => desc::BLUR,
        VertexArrayKind::Clip => desc::CLIP,
        VertexArrayKind::DashAndDot => desc::BORDER_CORNER_DASH_AND_DOT,
        VertexArrayKind::VectorStencil => desc::VECTOR_STENCIL,
        VertexArrayKind::VectorCover => desc::VECTOR_COVER,
    };

    let program = device.create_program(name, &prefix, &vertex_descriptor);

    if let Ok(ref program) = program {
        device.bind_shader_samplers(
            program,
            &[
                ("sColor0", TextureSampler::Color0),
                ("sColor1", TextureSampler::Color1),
                ("sColor2", TextureSampler::Color2),
                ("sDither", TextureSampler::Dither),
                ("sCacheA8", TextureSampler::CacheA8),
                ("sCacheRGBA8", TextureSampler::CacheRGBA8),
                ("sClipScrollNodes", TextureSampler::ClipScrollNodes),
                ("sRenderTasks", TextureSampler::RenderTasks),
                ("sResourceCache", TextureSampler::ResourceCache),
                ("sSharedCacheA8", TextureSampler::SharedCacheA8),
                ("sLocalClipRects", TextureSampler::LocalClipRects),
            ],
        );
    }

    program
}

fn create_clip_shader(name: &'static str, device: &mut Device) -> Result<Program, ShaderError> {
    let prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}\n
        #define WR_FEATURE_TRANSFORM\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    debug!("ClipShader {}", name);

    let program = device.create_program(name, &prefix, &desc::CLIP);

    if let Ok(ref program) = program {
        device.bind_shader_samplers(
            program,
            &[
                ("sColor0", TextureSampler::Color0),
                ("sClipScrollNodes", TextureSampler::ClipScrollNodes),
                ("sRenderTasks", TextureSampler::RenderTasks),
                ("sResourceCache", TextureSampler::ResourceCache),
                ("sSharedCacheA8", TextureSampler::SharedCacheA8),
                ("sLocalClipRects", TextureSampler::LocalClipRects),
            ],
        );
    }

    program
}


pub struct Shaders {
    // These are "cache shaders". These shaders are used to
    // draw intermediate results to cache targets. The results
    // of these shaders are then used by the primitive shaders.
    pub cs_blur_a8: LazilyCompiledShader,
    pub cs_blur_rgba8: LazilyCompiledShader,

    // Brush shaders
    brush_solid: BrushShader,
    brush_image: Vec<Option<BrushShader>>,
    brush_blend: BrushShader,
    brush_mix_blend: BrushShader,
    brush_yuv_image: Vec<Option<BrushShader>>,
    brush_radial_gradient: BrushShader,
    brush_linear_gradient: BrushShader,

    /// These are "cache clip shaders". These shaders are used to
    /// draw clip instances into the cached clip mask. The results
    /// of these shaders are also used by the primitive shaders.
    pub cs_clip_rectangle: LazilyCompiledShader,
    pub cs_clip_box_shadow: LazilyCompiledShader,
    pub cs_clip_image: LazilyCompiledShader,
    pub cs_clip_border: LazilyCompiledShader,
    pub cs_clip_line: LazilyCompiledShader,

    // The are "primitive shaders". These shaders draw and blend
    // final results on screen. They are aware of tile boundaries.
    // Most draw directly to the framebuffer, but some use inputs
    // from the cache shaders to draw. Specifically, the box
    // shadow primitive shader stretches the box shadow cache
    // output, and the cache_image shader blits the results of
    // a cache shader (e.g. blur) to the screen.
    pub ps_text_run: TextShader,
    pub ps_text_run_dual_source: TextShader,
    ps_border_corner: PrimitiveShader,
    ps_border_edge: PrimitiveShader,

    ps_split_composite: LazilyCompiledShader,
}

impl Shaders {
    pub fn new(
        device: &mut Device,
        gl_type: GlType,
        options: &RendererOptions,
    ) -> Result<Self, ShaderError> {
        // needed for the precache fake draws
        let dummy_vao = if options.precache_shaders {
            let vao = device.create_custom_vao(&[]);
            device.bind_custom_vao(&vao);
            Some(vao)
        } else {
            None
        };

        let brush_solid = BrushShader::new(
            "brush_solid",
            device,
            &[],
            options.precache_shaders,
            false,
        )?;

        let brush_blend = BrushShader::new(
            "brush_blend",
            device,
            &[],
            options.precache_shaders,
            false,
        )?;

        let brush_mix_blend = BrushShader::new(
            "brush_mix_blend",
            device,
            &[],
            options.precache_shaders,
            false,
        )?;

        let brush_radial_gradient = BrushShader::new(
            "brush_radial_gradient",
            device,
            if options.enable_dithering {
               &[DITHERING_FEATURE]
            } else {
               &[]
            },
            options.precache_shaders,
            false,
        )?;

        let brush_linear_gradient = BrushShader::new(
            "brush_linear_gradient",
            device,
            if options.enable_dithering {
               &[DITHERING_FEATURE]
            } else {
               &[]
            },
            options.precache_shaders,
            false,
        )?;

        let cs_blur_a8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Blur),
            "cs_blur",
            &["ALPHA_TARGET"],
            device,
            options.precache_shaders,
        )?;

        let cs_blur_rgba8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Blur),
            "cs_blur",
            &["COLOR_TARGET"],
            device,
            options.precache_shaders,
        )?;

        let cs_clip_rectangle = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_rectangle",
            &[],
            device,
            options.precache_shaders,
        )?;

        let cs_clip_box_shadow = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_box_shadow",
            &[],
            device,
            options.precache_shaders,
        )?;

        let cs_clip_line = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_line",
            &[],
            device,
            options.precache_shaders,
        )?;

        let cs_clip_image = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_image",
            &[],
            device,
            options.precache_shaders,
        )?;

        let cs_clip_border = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_border",
            &[],
            device,
            options.precache_shaders,
        )?;

        let ps_text_run = TextShader::new("ps_text_run",
            device,
            &[],
            options.precache_shaders,
        )?;

        let ps_text_run_dual_source = TextShader::new("ps_text_run",
            device,
            &["DUAL_SOURCE_BLENDING"],
            options.precache_shaders,
        )?;

        // All image configuration.
        let mut image_features = Vec::new();
        let mut brush_image = Vec::new();
        // PrimitiveShader is not clonable. Use push() to initialize the vec.
        for _ in 0 .. IMAGE_BUFFER_KINDS.len() {
            brush_image.push(None);
        }
        for buffer_kind in 0 .. IMAGE_BUFFER_KINDS.len() {
            if IMAGE_BUFFER_KINDS[buffer_kind].has_platform_support(&gl_type) {
                let feature_string = IMAGE_BUFFER_KINDS[buffer_kind].get_feature_string();
                if feature_string != "" {
                    image_features.push(feature_string);
                }
                brush_image[buffer_kind] = Some(BrushShader::new(
                    "brush_image",
                    device,
                    &image_features,
                    options.precache_shaders,
                    true,
                )?);
            }
            image_features.clear();
        }

        // All yuv_image configuration.
        let mut yuv_features = Vec::new();
        let yuv_shader_num = IMAGE_BUFFER_KINDS.len() * YUV_FORMATS.len() * YUV_COLOR_SPACES.len();
        let mut brush_yuv_image = Vec::new();
        // PrimitiveShader is not clonable. Use push() to initialize the vec.
        for _ in 0 .. yuv_shader_num {
            brush_yuv_image.push(None);
        }
        for image_buffer_kind in &IMAGE_BUFFER_KINDS {
            if image_buffer_kind.has_platform_support(&gl_type) {
                for format_kind in &YUV_FORMATS {
                    for color_space_kind in &YUV_COLOR_SPACES {
                        let feature_string = image_buffer_kind.get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }
                        let feature_string = format_kind.get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }
                        let feature_string = color_space_kind.get_feature_string();
                        if feature_string != "" {
                            yuv_features.push(feature_string);
                        }

                        let shader = BrushShader::new(
                            "brush_yuv_image",
                            device,
                            &yuv_features,
                            options.precache_shaders,
                            false,
                        )?;
                        let index = Self::get_yuv_shader_index(
                            *image_buffer_kind,
                            *format_kind,
                            *color_space_kind,
                        );
                        brush_yuv_image[index] = Some(shader);
                        yuv_features.clear();
                    }
                }
            }
        }

        let ps_border_corner = PrimitiveShader::new(
            "ps_border_corner",
             device,
             &[],
             options.precache_shaders,
        )?;

        let ps_border_edge = PrimitiveShader::new(
            "ps_border_edge",
             device,
             &[],
             options.precache_shaders,
        )?;

        let ps_split_composite = LazilyCompiledShader::new(
            ShaderKind::Primitive,
            "ps_split_composite",
            &[],
            device,
            options.precache_shaders,
        )?;

        if let Some(vao) = dummy_vao {
            device.delete_custom_vao(vao);
        }

        Ok(Shaders {
            cs_blur_a8,
            cs_blur_rgba8,
            brush_solid,
            brush_image,
            brush_blend,
            brush_mix_blend,
            brush_yuv_image,
            brush_radial_gradient,
            brush_linear_gradient,
            cs_clip_rectangle,
            cs_clip_box_shadow,
            cs_clip_border,
            cs_clip_image,
            cs_clip_line,
            ps_text_run,
            ps_text_run_dual_source,
            ps_border_corner,
            ps_border_edge,
            ps_split_composite,
        })
    }

    fn get_yuv_shader_index(
        buffer_kind: ImageBufferKind,
        format: YuvFormat,
        color_space: YuvColorSpace,
    ) -> usize {
        ((buffer_kind as usize) * YUV_FORMATS.len() + (format as usize)) * YUV_COLOR_SPACES.len() +
            (color_space as usize)
    }

    pub fn get(&mut self, key: &BatchKey) -> &mut LazilyCompiledShader {
        match key.kind {
            BatchKind::SplitComposite => {
                &mut self.ps_split_composite
            }
            BatchKind::Brush(brush_kind) => {
                let brush_shader = match brush_kind {
                    BrushBatchKind::Solid => {
                        &mut self.brush_solid
                    }
                    BrushBatchKind::Image(image_buffer_kind) => {
                        self.brush_image[image_buffer_kind as usize]
                            .as_mut()
                            .expect("Unsupported image shader kind")
                    }
                    BrushBatchKind::Blend => {
                        &mut self.brush_blend
                    }
                    BrushBatchKind::MixBlend { .. } => {
                        &mut self.brush_mix_blend
                    }
                    BrushBatchKind::RadialGradient => {
                        &mut self.brush_radial_gradient
                    }
                    BrushBatchKind::LinearGradient => {
                        &mut self.brush_linear_gradient
                    }
                    BrushBatchKind::YuvImage(image_buffer_kind, format, color_space) => {
                        let shader_index =
                            Self::get_yuv_shader_index(image_buffer_kind, format, color_space);
                        self.brush_yuv_image[shader_index]
                            .as_mut()
                            .expect("Unsupported YUV shader kind")
                    }
                };
                brush_shader.get(key.blend_mode)
            }
            BatchKind::Transformable(transform_kind, batch_kind) => {
                let prim_shader = match batch_kind {
                    TransformBatchKind::TextRun(glyph_format) => {
                        let text_shader = match key.blend_mode {
                            BlendMode::SubpixelDualSource => {
                                &mut self.ps_text_run_dual_source
                            }
                            _ => {
                                &mut self.ps_text_run
                            }
                        };
                        return text_shader.get(glyph_format, transform_kind);
                    }
                    TransformBatchKind::BorderCorner => {
                        &mut self.ps_border_corner
                    }
                    TransformBatchKind::BorderEdge => {
                        &mut self.ps_border_edge
                    }
                };
                prim_shader.get(transform_kind)
            }
        }
    }

    pub fn deinit(self, device: &mut Device) {
        self.cs_blur_a8.deinit(device);
        self.cs_blur_rgba8.deinit(device);
        self.brush_solid.deinit(device);
        self.brush_blend.deinit(device);
        self.brush_mix_blend.deinit(device);
        self.brush_radial_gradient.deinit(device);
        self.brush_linear_gradient.deinit(device);
        self.cs_clip_rectangle.deinit(device);
        self.cs_clip_box_shadow.deinit(device);
        self.cs_clip_image.deinit(device);
        self.cs_clip_border.deinit(device);
        self.cs_clip_line.deinit(device);
        self.ps_text_run.deinit(device);
        self.ps_text_run_dual_source.deinit(device);
        for shader in self.brush_image {
            if let Some(shader) = shader {
                shader.deinit(device);
            }
        }
        for shader in self.brush_yuv_image {
            if let Some(shader) = shader {
                shader.deinit(device);
            }
        }
        self.ps_border_corner.deinit(device);
        self.ps_border_edge.deinit(device);
        self.ps_split_composite.deinit(device);
    }
}

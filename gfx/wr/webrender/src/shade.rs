/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::batch::{BatchKey, BatchKind, BrushBatchKind};
use crate::device::{Device, Program, ShaderError};
use euclid::{Transform3D};
use crate::glyph_rasterizer::GlyphFormat;
use crate::renderer::{
    desc,
    MAX_VERTEX_TEXTURE_WIDTH,
    BlendMode, DebugFlags, ImageBufferKind, RendererError, RendererOptions,
    TextureSampler, VertexArrayKind, ShaderPrecacheFlags,
};

use gleam::gl::GlType;
use time::precise_time_ns;

use std::cell::RefCell;
use std::rc::Rc;

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

const ALPHA_FEATURE: &str = "ALPHA_PASS";
const DEBUG_OVERDRAW_FEATURE: &str = "DEBUG_OVERDRAW";
const DITHERING_FEATURE: &str = "DITHERING";
const DUAL_SOURCE_FEATURE: &str = "DUAL_SOURCE_BLENDING";
const FAST_PATH_FEATURE: &str = "FAST_PATH";
const PIXEL_LOCAL_STORAGE_FEATURE: &str = "PIXEL_LOCAL_STORAGE";

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
    Resolve,
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
        precache_flags: ShaderPrecacheFlags,
    ) -> Result<Self, ShaderError> {
        let mut shader = LazilyCompiledShader {
            program: None,
            name,
            kind,
            features: features.to_vec(),
        };

        if precache_flags.intersects(ShaderPrecacheFlags::ASYNC_COMPILE | ShaderPrecacheFlags::FULL_COMPILE) {
            let t0 = precise_time_ns();
            shader.get_internal(device, precache_flags)?;
            let t1 = precise_time_ns();
            debug!("[C: {:.1} ms ] Precache {} {:?}",
                (t1 - t0) as f64 / 1000000.0,
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

    fn get_internal(
        &mut self,
        device: &mut Device,
        precache_flags: ShaderPrecacheFlags,
    ) -> Result<&mut Program, ShaderError> {
        if self.program.is_none() {
            let program = match self.kind {
                ShaderKind::Primitive | ShaderKind::Brush | ShaderKind::Text | ShaderKind::Resolve => {
                    create_prim_shader(
                        self.name,
                        device,
                        &self.features,
                    )
                }
                ShaderKind::Cache(..) => {
                    create_prim_shader(
                        self.name,
                        device,
                        &self.features,
                    )
                }
                ShaderKind::VectorStencil => {
                    create_prim_shader(
                        self.name,
                        device,
                        &self.features,
                    )
                }
                ShaderKind::VectorCover => {
                    create_prim_shader(
                        self.name,
                        device,
                        &self.features,
                    )
                }
                ShaderKind::ClipCache => {
                    create_clip_shader(
                        self.name,
                        device,
                        &self.features,
                    )
                }
            };
            self.program = Some(program?);
        }

        let program = self.program.as_mut().unwrap();

        if precache_flags.contains(ShaderPrecacheFlags::FULL_COMPILE) && !program.is_initialized() {
            let vertex_format = match self.kind {
                ShaderKind::Primitive |
                ShaderKind::Brush |
                ShaderKind::Text => VertexArrayKind::Primitive,
                ShaderKind::Cache(format) => format,
                ShaderKind::VectorStencil => VertexArrayKind::VectorStencil,
                ShaderKind::VectorCover => VertexArrayKind::VectorCover,
                ShaderKind::ClipCache => VertexArrayKind::Clip,
                ShaderKind::Resolve => VertexArrayKind::Resolve,
            };

            let vertex_descriptor = match vertex_format {
                VertexArrayKind::Primitive => &desc::PRIM_INSTANCES,
                VertexArrayKind::LineDecoration => &desc::LINE,
                VertexArrayKind::Gradient => &desc::GRADIENT,
                VertexArrayKind::Blur => &desc::BLUR,
                VertexArrayKind::Clip => &desc::CLIP,
                VertexArrayKind::VectorStencil => &desc::VECTOR_STENCIL,
                VertexArrayKind::VectorCover => &desc::VECTOR_COVER,
                VertexArrayKind::Border => &desc::BORDER,
                VertexArrayKind::Scale => &desc::SCALE,
                VertexArrayKind::Resolve => &desc::RESOLVE,
            };

            device.link_program(program, vertex_descriptor)?;
            device.bind_program(program);
            match self.kind {
                ShaderKind::ClipCache => {
                    device.bind_shader_samplers(
                        &program,
                        &[
                            ("sColor0", TextureSampler::Color0),
                            ("sTransformPalette", TextureSampler::TransformPalette),
                            ("sRenderTasks", TextureSampler::RenderTasks),
                            ("sGpuCache", TextureSampler::GpuCache),
                            ("sPrimitiveHeadersF", TextureSampler::PrimitiveHeadersF),
                            ("sPrimitiveHeadersI", TextureSampler::PrimitiveHeadersI),
                        ],
                    );
                }
                _ => {
                    device.bind_shader_samplers(
                        &program,
                        &[
                            ("sColor0", TextureSampler::Color0),
                            ("sColor1", TextureSampler::Color1),
                            ("sColor2", TextureSampler::Color2),
                            ("sDither", TextureSampler::Dither),
                            ("sPrevPassAlpha", TextureSampler::PrevPassAlpha),
                            ("sPrevPassColor", TextureSampler::PrevPassColor),
                            ("sTransformPalette", TextureSampler::TransformPalette),
                            ("sRenderTasks", TextureSampler::RenderTasks),
                            ("sGpuCache", TextureSampler::GpuCache),
                            ("sPrimitiveHeadersF", TextureSampler::PrimitiveHeadersF),
                            ("sPrimitiveHeadersI", TextureSampler::PrimitiveHeadersI),
                        ],
                    );
                }
            }
        }

        Ok(program)
    }

    fn get(&mut self, device: &mut Device) -> Result<&mut Program, ShaderError> {
        self.get_internal(device, ShaderPrecacheFlags::FULL_COMPILE)
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
    debug_overdraw: LazilyCompiledShader,
}

impl BrushShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache_flags: ShaderPrecacheFlags,
        dual_source: bool,
        use_pixel_local_storage: bool,
    ) -> Result<Self, ShaderError> {
        let opaque = LazilyCompiledShader::new(
            ShaderKind::Brush,
            name,
            features,
            device,
            precache_flags,
        )?;

        let mut alpha_features = features.to_vec();
        alpha_features.push(ALPHA_FEATURE);
        if use_pixel_local_storage {
            alpha_features.push(PIXEL_LOCAL_STORAGE_FEATURE);
        }

        let alpha = LazilyCompiledShader::new(
            ShaderKind::Brush,
            name,
            &alpha_features,
            device,
            precache_flags,
        )?;

        // If using PLS, we disable all subpixel AA implicitly. Subpixel AA is always
        // disabled on mobile devices anyway, due to uncertainty over the subpixel
        // layout configuration.
        let dual_source = if dual_source && !use_pixel_local_storage {
            let mut dual_source_features = alpha_features.to_vec();
            dual_source_features.push(DUAL_SOURCE_FEATURE);

            let shader = LazilyCompiledShader::new(
                ShaderKind::Brush,
                name,
                &dual_source_features,
                device,
                precache_flags,
            )?;

            Some(shader)
        } else {
            None
        };

        let mut debug_overdraw_features = features.to_vec();
        debug_overdraw_features.push(DEBUG_OVERDRAW_FEATURE);

        let debug_overdraw = LazilyCompiledShader::new(
            ShaderKind::Brush,
            name,
            &debug_overdraw_features,
            device,
            precache_flags,
        )?;

        Ok(BrushShader {
            opaque,
            alpha,
            dual_source,
            debug_overdraw,
        })
    }

    fn get(&mut self, blend_mode: BlendMode, debug_flags: DebugFlags)
           -> &mut LazilyCompiledShader {
        match blend_mode {
            _ if debug_flags.contains(DebugFlags::SHOW_OVERDRAW) => &mut self.debug_overdraw,
            BlendMode::None => &mut self.opaque,
            BlendMode::Alpha |
            BlendMode::PremultipliedAlpha |
            BlendMode::PremultipliedDestOut |
            BlendMode::SubpixelConstantTextColor(..) |
            BlendMode::SubpixelWithBgColor |
            BlendMode::Advanced(_) => &mut self.alpha,
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
        self.debug_overdraw.deinit(device);
    }
}

pub struct TextShader {
    simple: LazilyCompiledShader,
    glyph_transform: LazilyCompiledShader,
    debug_overdraw: LazilyCompiledShader,
}

impl TextShader {
    fn new(
        name: &'static str,
        device: &mut Device,
        features: &[&'static str],
        precache_flags: ShaderPrecacheFlags,
    ) -> Result<Self, ShaderError> {
        let simple = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            features,
            device,
            precache_flags,
        )?;

        let mut glyph_transform_features = features.to_vec();
        glyph_transform_features.push("GLYPH_TRANSFORM");

        let glyph_transform = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            &glyph_transform_features,
            device,
            precache_flags,
        )?;

        let mut debug_overdraw_features = features.to_vec();
        debug_overdraw_features.push("DEBUG_OVERDRAW");

        let debug_overdraw = LazilyCompiledShader::new(
            ShaderKind::Text,
            name,
            &debug_overdraw_features,
            device,
            precache_flags,
        )?;

        Ok(TextShader { simple, glyph_transform, debug_overdraw })
    }

    pub fn get(
        &mut self,
        glyph_format: GlyphFormat,
        debug_flags: DebugFlags,
    ) -> &mut LazilyCompiledShader {
        match glyph_format {
            _ if debug_flags.contains(DebugFlags::SHOW_OVERDRAW) => &mut self.debug_overdraw,
            GlyphFormat::Alpha |
            GlyphFormat::Subpixel |
            GlyphFormat::Bitmap |
            GlyphFormat::ColorBitmap => &mut self.simple,
            GlyphFormat::TransformedAlpha |
            GlyphFormat::TransformedSubpixel => &mut self.glyph_transform,
        }
    }

    fn deinit(self, device: &mut Device) {
        self.simple.deinit(device);
        self.glyph_transform.deinit(device);
        self.debug_overdraw.deinit(device);
    }
}

fn create_prim_shader(
    name: &'static str,
    device: &mut Device,
    features: &[&'static str],
) -> Result<Program, ShaderError> {
    let mut prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}U\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    for feature in features {
        prefix.push_str(&format!("#define WR_FEATURE_{}\n", feature));
    }

    debug!("PrimShader {}", name);

    device.create_program(name, prefix)
}

fn create_clip_shader(
    name: &'static str,
    device: &mut Device,
    features: &[&'static str],
) -> Result<Program, ShaderError> {
    let mut prefix = format!(
        "#define WR_MAX_VERTEX_TEXTURE_WIDTH {}U\n",
        MAX_VERTEX_TEXTURE_WIDTH
    );

    for feature in features {
        prefix.push_str(&format!("#define WR_FEATURE_{}\n", feature));
    }

    debug!("ClipShader {}", name);

    device.create_program(name, prefix)
}

// NB: If you add a new shader here, make sure to deinitialize it
// in `Shaders::deinit()` below.
pub struct Shaders {
    // These are "cache shaders". These shaders are used to
    // draw intermediate results to cache targets. The results
    // of these shaders are then used by the primitive shaders.
    pub cs_blur_a8: LazilyCompiledShader,
    pub cs_blur_rgba8: LazilyCompiledShader,
    pub cs_border_segment: LazilyCompiledShader,
    pub cs_border_solid: LazilyCompiledShader,
    pub cs_scale_a8: LazilyCompiledShader,
    pub cs_scale_rgba8: LazilyCompiledShader,
    pub cs_line_decoration: LazilyCompiledShader,
    pub cs_gradient: LazilyCompiledShader,

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
    pub cs_clip_rectangle_slow: LazilyCompiledShader,
    pub cs_clip_rectangle_fast: LazilyCompiledShader,
    pub cs_clip_box_shadow: LazilyCompiledShader,
    pub cs_clip_image: LazilyCompiledShader,

    // The are "primitive shaders". These shaders draw and blend
    // final results on screen. They are aware of tile boundaries.
    // Most draw directly to the framebuffer, but some use inputs
    // from the cache shaders to draw. Specifically, the box
    // shadow primitive shader stretches the box shadow cache
    // output, and the cache_image shader blits the results of
    // a cache shader (e.g. blur) to the screen.
    pub ps_text_run: TextShader,
    pub ps_text_run_dual_source: TextShader,

    // Helper shaders for pixel local storage render paths.
    // pls_init: Initialize pixel local storage, based on current framebuffer value.
    // pls_resolve: Convert pixel local storage, writing out to fragment value.
    pub pls_init: LazilyCompiledShader,
    pub pls_resolve: LazilyCompiledShader,

    ps_split_composite: LazilyCompiledShader,
}

impl Shaders {
    pub fn new(
        device: &mut Device,
        gl_type: GlType,
        options: &RendererOptions,
    ) -> Result<Self, ShaderError> {
        let use_pixel_local_storage = device
            .get_capabilities()
            .supports_pixel_local_storage;

        let brush_solid = BrushShader::new(
            "brush_solid",
            device,
            &[],
            options.precache_flags,
            false,
            use_pixel_local_storage,
        )?;

        let brush_blend = BrushShader::new(
            "brush_blend",
            device,
            &[],
            options.precache_flags,
            false,
            use_pixel_local_storage,
        )?;

        let brush_mix_blend = BrushShader::new(
            "brush_mix_blend",
            device,
            &[],
            options.precache_flags,
            false,
            use_pixel_local_storage,
        )?;

        let brush_radial_gradient = BrushShader::new(
            "brush_radial_gradient",
            device,
            if options.enable_dithering {
               &[DITHERING_FEATURE]
            } else {
               &[]
            },
            options.precache_flags,
            false,
            use_pixel_local_storage,
        )?;

        let brush_linear_gradient = BrushShader::new(
            "brush_linear_gradient",
            device,
            if options.enable_dithering {
               &[DITHERING_FEATURE]
            } else {
               &[]
            },
            options.precache_flags,
            false,
            use_pixel_local_storage,
        )?;

        let cs_blur_a8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Blur),
            "cs_blur",
            &["ALPHA_TARGET"],
            device,
            options.precache_flags,
        )?;

        let cs_blur_rgba8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Blur),
            "cs_blur",
            &["COLOR_TARGET"],
            device,
            options.precache_flags,
        )?;

        let cs_clip_rectangle_slow = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_rectangle",
            &[],
            device,
            options.precache_flags,
        )?;

        let cs_clip_rectangle_fast = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_rectangle",
            &[FAST_PATH_FEATURE],
            device,
            options.precache_flags,
        )?;

        let cs_clip_box_shadow = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_box_shadow",
            &[],
            device,
            options.precache_flags,
        )?;

        let cs_clip_image = LazilyCompiledShader::new(
            ShaderKind::ClipCache,
            "cs_clip_image",
            &[],
            device,
            options.precache_flags,
        )?;

        let pls_precache_flags = if use_pixel_local_storage {
            options.precache_flags
        } else {
            ShaderPrecacheFlags::empty()
        };

        let pls_init = LazilyCompiledShader::new(
            ShaderKind::Resolve,
            "pls_init",
            &[PIXEL_LOCAL_STORAGE_FEATURE],
            device,
            pls_precache_flags,
        )?;

        let pls_resolve = LazilyCompiledShader::new(
            ShaderKind::Resolve,
            "pls_resolve",
            &[PIXEL_LOCAL_STORAGE_FEATURE],
            device,
            pls_precache_flags,
        )?;

        let cs_scale_a8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Scale),
            "cs_scale",
            &["ALPHA_TARGET"],
            device,
            options.precache_flags,
        )?;

        let cs_scale_rgba8 = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Scale),
            "cs_scale",
            &["COLOR_TARGET"],
            device,
            options.precache_flags,
        )?;

        // TODO(gw): The split composite + text shader are special cases - the only
        //           shaders used during normal scene rendering that aren't a brush
        //           shader. Perhaps we can unify these in future?
        let mut extra_features = Vec::new();
        if use_pixel_local_storage {
            extra_features.push(PIXEL_LOCAL_STORAGE_FEATURE);
        }

        let ps_text_run = TextShader::new("ps_text_run",
            device,
            &extra_features,
            options.precache_flags,
        )?;

        let dual_source_precache_flags = if options.allow_dual_source_blending {
            options.precache_flags
        } else {
            ShaderPrecacheFlags::empty()
        };

        let ps_text_run_dual_source = TextShader::new("ps_text_run",
            device,
            &[DUAL_SOURCE_FEATURE],
            dual_source_precache_flags,
        )?;

        let ps_split_composite = LazilyCompiledShader::new(
            ShaderKind::Primitive,
            "ps_split_composite",
            &extra_features,
            device,
            options.precache_flags,
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
                    options.precache_flags,
                    options.allow_dual_source_blending,
                    use_pixel_local_storage,
                )?);
            }
            image_features.clear();
        }

        // All yuv_image configuration.
        let mut yuv_features = Vec::new();
        let yuv_shader_num = IMAGE_BUFFER_KINDS.len();
        let mut brush_yuv_image = Vec::new();
        // PrimitiveShader is not clonable. Use push() to initialize the vec.
        for _ in 0 .. yuv_shader_num {
            brush_yuv_image.push(None);
        }
        for image_buffer_kind in &IMAGE_BUFFER_KINDS {
            if image_buffer_kind.has_platform_support(&gl_type) {
                let feature_string = image_buffer_kind.get_feature_string();
                if feature_string != "" {
                    yuv_features.push(feature_string);
                }

                let shader = BrushShader::new(
                    "brush_yuv_image",
                    device,
                    &yuv_features,
                    options.precache_flags,
                    false,
                    use_pixel_local_storage,
                )?;
                let index = Self::get_yuv_shader_index(
                    *image_buffer_kind,
                );
                brush_yuv_image[index] = Some(shader);
                yuv_features.clear();
            }
        }

        let cs_line_decoration = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::LineDecoration),
            "cs_line_decoration",
            &[],
            device,
            options.precache_flags,
        )?;

        let cs_gradient = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Gradient),
            "cs_gradient",
            &[],
            device,
            options.precache_flags,
        )?;

        let cs_border_segment = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Border),
            "cs_border_segment",
             &[],
             device,
             options.precache_flags,
        )?;

        let cs_border_solid = LazilyCompiledShader::new(
            ShaderKind::Cache(VertexArrayKind::Border),
            "cs_border_solid",
            &[],
            device,
            options.precache_flags,
        )?;

        Ok(Shaders {
            cs_blur_a8,
            cs_blur_rgba8,
            cs_border_segment,
            cs_line_decoration,
            cs_gradient,
            cs_border_solid,
            cs_scale_a8,
            cs_scale_rgba8,
            brush_solid,
            brush_image,
            brush_blend,
            brush_mix_blend,
            brush_yuv_image,
            brush_radial_gradient,
            brush_linear_gradient,
            cs_clip_rectangle_slow,
            cs_clip_rectangle_fast,
            cs_clip_box_shadow,
            cs_clip_image,
            pls_init,
            pls_resolve,
            ps_text_run,
            ps_text_run_dual_source,
            ps_split_composite,
        })
    }

    fn get_yuv_shader_index(buffer_kind: ImageBufferKind) -> usize {
        (buffer_kind as usize)
    }

    pub fn get(&mut self, key: &BatchKey, debug_flags: DebugFlags) -> &mut LazilyCompiledShader {
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
                    BrushBatchKind::YuvImage(image_buffer_kind, ..) => {
                        let shader_index =
                            Self::get_yuv_shader_index(image_buffer_kind);
                        self.brush_yuv_image[shader_index]
                            .as_mut()
                            .expect("Unsupported YUV shader kind")
                    }
                };
                brush_shader.get(key.blend_mode, debug_flags)
            }
            BatchKind::TextRun(glyph_format) => {
                let text_shader = match key.blend_mode {
                    BlendMode::SubpixelDualSource => &mut self.ps_text_run_dual_source,
                    _ => &mut self.ps_text_run,
                };
                text_shader.get(glyph_format, debug_flags)
            }
        }
    }

    pub fn deinit(self, device: &mut Device) {
        self.cs_scale_a8.deinit(device);
        self.cs_scale_rgba8.deinit(device);
        self.cs_blur_a8.deinit(device);
        self.cs_blur_rgba8.deinit(device);
        self.brush_solid.deinit(device);
        self.brush_blend.deinit(device);
        self.brush_mix_blend.deinit(device);
        self.brush_radial_gradient.deinit(device);
        self.brush_linear_gradient.deinit(device);
        self.cs_clip_rectangle_slow.deinit(device);
        self.cs_clip_rectangle_fast.deinit(device);
        self.cs_clip_box_shadow.deinit(device);
        self.cs_clip_image.deinit(device);
        self.pls_init.deinit(device);
        self.pls_resolve.deinit(device);
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
        self.cs_border_solid.deinit(device);
        self.cs_gradient.deinit(device);
        self.cs_line_decoration.deinit(device);
        self.cs_border_segment.deinit(device);
        self.ps_split_composite.deinit(device);
    }
}

// A wrapper around a strong reference to a Shaders
// object. We have this so that external (ffi)
// consumers can own a reference to a shared Shaders
// instance without understanding rust's refcounting.
pub struct WrShaders {
    pub shaders: Rc<RefCell<Shaders>>,
}

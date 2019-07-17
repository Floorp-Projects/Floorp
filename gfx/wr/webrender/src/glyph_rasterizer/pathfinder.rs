/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Module only available when pathfinder is activated

use api::{FontKey, FontTemplate, NativeFontHandle};
use api::units::{DeviceIntPoint, DeviceIntSize, DevicePixel};
use euclid::{TypedPoint2D, TypedSize2D, TypedVector2D};
use pathfinder_font_renderer;
use pathfinder_partitioner::mesh::Mesh as PathfinderMesh;
use pathfinder_path_utils::cubic_to_quadratic::CubicToQuadraticTransformer;
use crate::render_task::{RenderTask, RenderTaskGraph, RenderTaskCache, RenderTaskCacheKey,
                         RenderTaskCacheEntryHandle, RenderTaskCacheKeyKind, RenderTaskId,
                         RenderTaskLocation};
use crate::resource_cache::CacheItem;
use std::ops::Deref;
use std::sync::{Arc, Mutex, MutexGuard};
use crate::glyph_rasterizer::AddFont;
use crate::internal_types::ResourceCacheError;
use crate::glyph_cache::{GlyphCache, GlyphCacheEntry, CachedGlyphInfo};
use std::collections::hash_map::Entry;
use std::f32;
use crate::glyph_rasterizer::{FontInstance, GlyphRasterizer, GlyphFormat, GlyphKey, FontContexts};
use crate::texture_cache::TextureCache;
use crate::gpu_cache::GpuCache;
use crate::profiler::TextureCacheProfileCounters;

/// Should match macOS 10.13 High Sierra.
///
/// We multiply by sqrt(2) to compensate for the fact that dilation amounts are relative to the
/// pixel square on macOS and relative to the vertex normal in Pathfinder.
const STEM_DARKENING_FACTOR_X: f32 = 0.0121 * f32::consts::SQRT_2;
const STEM_DARKENING_FACTOR_Y: f32 = 0.0121 * 1.25 * f32::consts::SQRT_2;

/// Likewise, should match macOS 10.13 High Sierra.
const MAX_STEM_DARKENING_AMOUNT: f32 = 0.3 * f32::consts::SQRT_2;

const CUBIC_TO_QUADRATIC_APPROX_TOLERANCE: f32 = 0.01;

type PathfinderFontContext = pathfinder_font_renderer::FontContext<FontKey>;

impl AddFont for PathfinderFontContext {
    fn add_font(&mut self, font_key: &FontKey, template: &FontTemplate) {
        match *template {
            FontTemplate::Raw(ref bytes, index) => {
                drop(self.add_font_from_memory(&font_key, bytes.clone(), index))
            }
            FontTemplate::Native(ref native_font_handle) => {
                drop(self.add_native_font(&font_key, NativeFontHandleWrapper(native_font_handle)))
            }
        }
    }
}

pub(in super) fn create_pathfinder_font_context()
-> Result<Box<ThreadSafePathfinderFontContext>, ResourceCacheError>
{
    match PathfinderFontContext::new() {
        Ok(context) => Ok(Box::new(ThreadSafePathfinderFontContext(Mutex::new(context)))),
        Err(_) => {
            let msg = "Failed to create the Pathfinder font context!".to_owned();
            Err(ResourceCacheError::new(msg))
        }
    }
}

pub struct ThreadSafePathfinderFontContext(Mutex<PathfinderFontContext>);

impl Deref for ThreadSafePathfinderFontContext {
    type Target = Mutex<PathfinderFontContext>;

    fn deref(&self) -> &Mutex<PathfinderFontContext> {
        &self.0
    }
}

/// PathfinderFontContext can contain a *mut IDWriteFactory.
/// However, since we know that it is wrapped in a Mutex, it is safe
/// to assume that this struct is thread-safe
unsafe impl Send for ThreadSafePathfinderFontContext {}
unsafe impl Sync for ThreadSafePathfinderFontContext { }

impl GlyphRasterizer {
    pub(in super) fn add_font_to_pathfinder(&mut self, font_key: &FontKey, template: &FontTemplate) {
        let font_contexts = Arc::clone(&self.font_contexts);
        debug!("add_font_to_pathfinder({:?})", font_key);
        font_contexts.lock_pathfinder_context().add_font(&font_key, &template);
    }

    pub fn get_cache_item_for_glyph(
        &self,
        glyph_key: &GlyphKey,
        font: &FontInstance,
        glyph_cache: &GlyphCache,
        texture_cache: &TextureCache,
        render_task_cache: &RenderTaskCache)
    -> Option<(CacheItem, GlyphFormat)>
    {
        let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font(font);
        let render_task_cache_key = match *glyph_key_cache.get(glyph_key) {
            GlyphCacheEntry::Cached(ref cached_glyph) => {
                (*cached_glyph).render_task_cache_key.clone()
            }
            GlyphCacheEntry::Blank => return None,
            GlyphCacheEntry::Pending => {
                panic!("GlyphRasterizer::get_cache_item_for_glyph(): Glyph should have been \
                        cached by now!")
            }
        };
        let cache_item = render_task_cache.get_cache_item_for_render_task(texture_cache,
                                                                          &render_task_cache_key);
        Some((cache_item, font.get_glyph_format()))
    }

    pub(in super) fn request_glyph_from_pathfinder_if_necessary(
        &mut self,
        glyph_key: &GlyphKey,
        font: &FontInstance,
        scale: f32,
        cached_glyph_info: CachedGlyphInfo,
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        render_task_cache: &mut RenderTaskCache,
        render_task_tree: &mut RenderTaskGraph,
    ) -> Result<(RenderTaskCacheEntryHandle,GlyphFormat), ()> {
        let mut pathfinder_font_context = self.font_contexts.lock_pathfinder_context();
        let render_task_cache_key = cached_glyph_info.render_task_cache_key;
        let (glyph_origin, glyph_size) = (cached_glyph_info.origin, render_task_cache_key.size);
        let user_data = [glyph_origin.x as f32, (glyph_origin.y - glyph_size.height) as f32, scale];
        let handle = render_task_cache.request_render_task(render_task_cache_key,
                                                           texture_cache,
                                                           gpu_cache,
                                                           render_task_tree,
                                                           Some(user_data),
                                                           false,
                                                           |render_tasks| {
            // TODO(pcwalton): Non-subpixel font render mode.
            request_render_task_from_pathfinder(
                glyph_key,
                font,
                scale,
                &glyph_origin,
                &glyph_size,
                &mut *pathfinder_font_context,
                render_tasks,
            )
        })?;
        Ok((handle, font.get_glyph_format()))
    }

    pub fn request_glyphs(
        &mut self,
        glyph_cache: &mut GlyphCache,
        font: FontInstance,
        glyph_keys: &[GlyphKey],
        texture_cache: &mut TextureCache,
        gpu_cache: &mut GpuCache,
        render_task_cache: &mut RenderTaskCache,
        render_task_tree: &mut RenderTaskGraph,
    ) {
        debug_assert!(self.font_contexts.lock_shared_context().has_font(&font.font_key));

        let glyph_key_cache = glyph_cache.get_glyph_key_cache_for_font_mut(font.clone());

        let (x_scale, y_scale) = font.transform.compute_scale().unwrap_or((1.0, 1.0));
        let scale = font.oversized_scale_factor(x_scale, y_scale) as f32;

        // select glyphs that have not been requested yet.
        for glyph_key in glyph_keys {
            let mut cached_glyph_info = None;
            match glyph_key_cache.entry(glyph_key.clone()) {
                Entry::Occupied(entry) => {
                    let value = entry.into_mut();
                    match *value {
                        GlyphCacheEntry::Cached(ref glyph_info) => {
                            cached_glyph_info = Some(glyph_info.clone())
                        }
                        GlyphCacheEntry::Blank | GlyphCacheEntry::Pending => {}
                    }
                }
                Entry::Vacant(_) => {}
            }

            if cached_glyph_info.is_none() {
                let pathfinder_font_context = self.font_contexts.lock_pathfinder_context();

                let pathfinder_font_instance = pathfinder_font_renderer::FontInstance {
                    font_key: font.font_key.clone(),
                    size: font.size.scale_by(scale.recip()),
                };

                // TODO: pathfinder will need to support 2D subpixel offset
                let pathfinder_subpixel_offset =
                    pathfinder_font_renderer::SubpixelOffset(glyph_key.subpixel_offset().0 as u8);
                let pathfinder_glyph_key =
                    pathfinder_font_renderer::GlyphKey::new(glyph_key.index(),
                                                            pathfinder_subpixel_offset);

                if let Ok(glyph_dimensions) =
                        pathfinder_font_context.glyph_dimensions(&pathfinder_font_instance,
                                                                 &pathfinder_glyph_key,
                                                                 false) {
                    let render_task_cache_key = RenderTaskCacheKey {
                        size: TypedSize2D::from_untyped(&glyph_dimensions.size.to_i32()),
                        kind: RenderTaskCacheKeyKind::Glyph(self.next_gpu_glyph_cache_key),
                    };
                    cached_glyph_info = Some(CachedGlyphInfo {
                        render_task_cache_key,
                        format: font.get_glyph_format(),
                        origin: DeviceIntPoint::new(glyph_dimensions.origin.x as i32,
                                                    -glyph_dimensions.origin.y as i32),
                    });
                    self.next_gpu_glyph_cache_key.0 += 1;
                }
            }

            let handle = match cached_glyph_info {
                Some(glyph_info) => {
                    match self.request_glyph_from_pathfinder_if_necessary(
                        glyph_key,
                        &font,
                        scale,
                        glyph_info.clone(),
                        texture_cache,
                        gpu_cache,
                        render_task_cache,
                        render_task_tree,
                    ) {
                        Ok(_) => GlyphCacheEntry::Cached(glyph_info),
                        Err(_) => GlyphCacheEntry::Blank,
                    }
                }
                None => GlyphCacheEntry::Blank,
            };

            glyph_key_cache.insert(glyph_key.clone(), handle);
        }
    }

    pub fn resolve_glyphs(
        &mut self,
        _: &mut GlyphCache,
        _: &mut TextureCache,
        _: &mut GpuCache,
        _: &mut RenderTaskCache,
        _: &mut RenderTaskGraph,
        _: &mut TextureCacheProfileCounters,
    ) {
        self.remove_dead_fonts();
    }
}

impl FontContexts {
    pub fn lock_pathfinder_context(&self) -> MutexGuard<PathfinderFontContext> {
        self.pathfinder_context.lock().unwrap()
    }
}

fn compute_embolden_amount(ppem: f32) -> TypedVector2D<f32, DevicePixel> {
    TypedVector2D::new(f32::min(ppem * STEM_DARKENING_FACTOR_X, MAX_STEM_DARKENING_AMOUNT),
                       f32::min(ppem * STEM_DARKENING_FACTOR_Y, MAX_STEM_DARKENING_AMOUNT))
}

fn request_render_task_from_pathfinder(
    glyph_key: &GlyphKey,
    font: &FontInstance,
    scale: f32,
    glyph_origin: &DeviceIntPoint,
    glyph_size: &DeviceIntSize,
    font_context: &mut PathfinderFontContext,
    render_tasks: &mut RenderTaskGraph,
) -> Result<RenderTaskId, ()> {
    let size = font.size.scale_by(scale.recip());
    let pathfinder_font_instance = pathfinder_font_renderer::FontInstance {
        font_key: font.font_key.clone(),
        size,
    };

    // TODO: pathfinder will need to support 2D subpixel offset
    let pathfinder_subpixel_offset =
        pathfinder_font_renderer::SubpixelOffset(glyph_key.subpixel_offset().0 as u8);
    let glyph_subpixel_offset: f64 = glyph_key.subpixel_offset().0.into();
    let pathfinder_glyph_key = pathfinder_font_renderer::GlyphKey::new(glyph_key.index(),
                                                                       pathfinder_subpixel_offset);

    // TODO(pcwalton): Fall back to CPU rendering if Pathfinder fails to collect the outline.
    let mut mesh = PathfinderMesh::new();
    let outline = font_context.glyph_outline(&pathfinder_font_instance,
                                             &pathfinder_glyph_key)?;
    let tolerance = CUBIC_TO_QUADRATIC_APPROX_TOLERANCE;
    mesh.push_stencil_segments(CubicToQuadraticTransformer::new(outline.iter(), tolerance));
    mesh.push_stencil_normals(CubicToQuadraticTransformer::new(outline.iter(), tolerance));

    // FIXME(pcwalton): Support vertical subpixel offsets.
    // FIXME(pcwalton): Embolden amount should be 0 on macOS if "Use LCD font
    // smoothing" is unchecked in System Preferences.

    let subpixel_offset = TypedPoint2D::new(glyph_subpixel_offset as f32, 0.0);
    let embolden_amount = compute_embolden_amount(size.to_f32_px());

    let location = RenderTaskLocation::Dynamic(None, *glyph_size);
    let glyph_render_task = RenderTask::new_glyph(
        location.clone(),
        mesh,
        &glyph_origin,
        &subpixel_offset,
        font.render_mode,
        &embolden_amount,
    );

    Ok(render_tasks.add(glyph_render_task))
}

pub struct NativeFontHandleWrapper<'a>(pub &'a NativeFontHandle);

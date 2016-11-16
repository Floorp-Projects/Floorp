/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use device::{ProgramId, TextureId};
use euclid::{Point2D, Rect, Size2D};
use internal_types::{AxisDirection};
use internal_types::{PackedVertexForTextureCacheUpdate};
use std::sync::atomic::Ordering::SeqCst;
use std::sync::atomic::{AtomicUsize, ATOMIC_USIZE_INIT};
use texture_cache::TexturePage;

static ID_COUNTER: AtomicUsize = ATOMIC_USIZE_INIT;

#[inline]
pub fn new_id() -> usize {
    ID_COUNTER.fetch_add(1, SeqCst)
}

// Information needed to blit an item from a raster op batch target to final destination.
pub struct BlitJob {
    pub dest_texture_id: TextureId,
    pub size: Size2D<u32>,
    pub src_origin: Point2D<u32>,
    pub dest_origin: Point2D<u32>,
}

/// A batch for raster jobs.
pub struct RasterBatch {
    pub program_id: ProgramId,
    pub blur_direction: Option<AxisDirection>,
    pub color_texture_id: TextureId,
    pub vertices: Vec<PackedVertexForTextureCacheUpdate>,
    pub indices: Vec<u16>,
    pub page_allocator: TexturePage,
    pub dest_texture_id: TextureId,
    pub blit_jobs: Vec<BlitJob>,
}

impl RasterBatch {
    pub fn new(target_texture_id: TextureId,
               target_texture_size: u32,
               program_id: ProgramId,
               blur_direction: Option<AxisDirection>,
               color_texture_id: TextureId,
               dest_texture_id: TextureId)
               -> RasterBatch {
        RasterBatch {
            program_id: program_id,
            blur_direction: blur_direction,
            color_texture_id: color_texture_id,
            dest_texture_id: dest_texture_id,
            vertices: Vec::new(),
            indices: Vec::new(),
            page_allocator: TexturePage::new(target_texture_id, target_texture_size),
            blit_jobs: Vec::new(),
        }
    }

    pub fn add_rect_if_possible<F>(&mut self,
                                   dest_texture_id: TextureId,
                                   color_texture_id: TextureId,
                                   program_id: ProgramId,
                                   blur_direction: Option<AxisDirection>,
                                   dest_rect: &Rect<u32>,
                                   f: &F) -> bool
                                   where F: Fn(&Rect<f32>) -> [PackedVertexForTextureCacheUpdate; 4] {
        // TODO(gw): How to detect / handle if border type is single pixel but not in an atlas!?

        let batch_ok = program_id == self.program_id &&
            blur_direction == self.blur_direction &&
            color_texture_id == self.color_texture_id &&
            dest_texture_id == self.dest_texture_id;

        if batch_ok {
            let origin = self.page_allocator.allocate(&dest_rect.size);

            if let Some(origin) = origin {
                let vertices = f(&Rect::new(Point2D::new(origin.x as f32,
                                                         origin.y as f32),
                                            Size2D::new(dest_rect.size.width as f32,
                                                        dest_rect.size.height as f32)));
                let mut i = 0;
                let index_offset = self.vertices.len();
                while i < vertices.len() {
                    let index_base = (index_offset + i) as u16;
                    self.indices.push(index_base + 0);
                    self.indices.push(index_base + 1);
                    self.indices.push(index_base + 2);
                    self.indices.push(index_base + 2);
                    self.indices.push(index_base + 3);
                    self.indices.push(index_base + 1);
                    i += 4;
                }

                self.vertices.extend_from_slice(&vertices);

                self.blit_jobs.push(BlitJob {
                    dest_texture_id: dest_texture_id,
                    size: dest_rect.size,
                    dest_origin: dest_rect.origin,
                    src_origin: origin,
                });

                return true;
            }
        }

        false
    }
}

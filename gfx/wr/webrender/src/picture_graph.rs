/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::frame_builder::FrameBuildingContext;
use crate::internal_types::FastHashMap;
use crate::prim_store::PictureIndex;
use crate::picture::{PicturePrimitive, SurfaceIndex, SurfaceInfo};
use crate::picture::{TileCacheInstance, SliceId};
use smallvec::SmallVec;

#[derive(Debug)]
pub struct PictureInfo {
    pub update_pass: Option<usize>,
    pub surface_index: Option<SurfaceIndex>,
    pub parent: Option<PictureIndex>,
}

/// A graph of picture dependencies, allowing updates to be processed without recursion
/// by building a list of passes.
pub struct PictureGraph {
    roots: Vec<PictureIndex>,
    pic_info: Vec<PictureInfo>,
    update_passes: Vec<Vec<PictureIndex>>,
}

impl PictureGraph {
    pub fn new() -> Self {
        PictureGraph {
            roots: Vec::new(),
            pic_info: Vec::new(),
            update_passes: Vec::new(),
        }
    }

    /// Add a root picture to the graph
    pub fn add_root(
        &mut self,
        pic_index: PictureIndex,
    ) {
        self.roots.push(pic_index);
    }

    /// Build a list of update passes based on the dependencies between pictures
    pub fn build_update_passes(
        &mut self,
        pictures: &mut [PicturePrimitive],
        frame_context: &FrameBuildingContext
    ) {
        self.pic_info.clear();
        self.pic_info.reserve(pictures.len());

        for _ in 0 .. pictures.len() {
            self.pic_info.push(PictureInfo {
                update_pass: None,
                parent: None,
                surface_index: None,
            })
        };

        let mut max_pass_index = 0;

        for pic_index in &self.roots {
            assign_update_pass(
                *pic_index,
                None,
                0,
                pictures,
                &mut self.pic_info,
                &mut max_pass_index,
                frame_context,
            );
        }

        let pass_count = max_pass_index + 1;

        self.update_passes.clear();
        self.update_passes.resize_with(pass_count, Vec::new);

        for (pic_index, info) in self.pic_info.iter().enumerate() {
            if let Some(update_pass) = info.update_pass {
                let pass = &mut self.update_passes[update_pass];
                pass.push(PictureIndex(pic_index));
            }
        }
    }

    /// Assign surfaces and scale factors to each picture (root -> leaf ordered pass)
    pub fn assign_surfaces(
        &mut self,
        pictures: &mut [PicturePrimitive],
        surfaces: &mut Vec<SurfaceInfo>,
        tile_caches: &mut FastHashMap<SliceId, Box<TileCacheInstance>>,
        frame_context: &FrameBuildingContext,
    ) {
        for pass in &self.update_passes {
            for pic_index in pass {
                let parent = self.pic_info[pic_index.0].parent;

                let parent_surface_index = parent.map(|parent| {
                    // Can unwrap here as by the time we have a parent that parent's
                    // surface must have been assigned.
                    self.pic_info[parent.0].surface_index.unwrap()
                });

                let info = &mut self.pic_info[pic_index.0];

                match pictures[pic_index.0].assign_surface(
                    frame_context,
                    parent_surface_index,
                    tile_caches,
                    surfaces,
                ) {
                    Some(surface_index) => {
                        info.surface_index = Some(surface_index);
                    }
                    None => {
                        info.surface_index = Some(parent_surface_index.unwrap());
                    }
                }
            }
        }
    }

    /// Propegate bounding rects from pictures to parents (leaf -> root ordered pass)
    pub fn propagate_bounding_rects(
        &mut self,
        pictures: &mut [PicturePrimitive],
        surfaces: &mut [SurfaceInfo],
        frame_context: &FrameBuildingContext,
    ) {
        for pass in self.update_passes.iter().rev() {
            for pic_index in pass {
                let parent = self.pic_info[pic_index.0].parent;

                let surface_index = self.pic_info[pic_index.0]
                    .surface_index
                    .expect("bug: no surface assigned during propagate_bounding_rects");

                let parent_surface_index = parent.map(|parent| {
                    // Can unwrap here as by the time we have a parent that parent's
                    // surface must have been assigned.
                    self.pic_info[parent.0].surface_index.unwrap()
                });

                pictures[pic_index.0].propagate_bounding_rect(
                    surface_index,
                    parent_surface_index,
                    surfaces,
                    frame_context,
                );
            }
        }
    }
}

/// Recursive function that assigns pictures to the earliest pass possible that they
/// can be processed in, while maintaining dependency ordering.
fn assign_update_pass(
    pic_index: PictureIndex,
    parent_pic_index: Option<PictureIndex>,
    pass: usize,
    pictures: &mut [PicturePrimitive],
    pic_info: &mut [PictureInfo],
    max_pass_index: &mut usize,
    frame_context: &FrameBuildingContext
) {
    let pic = &mut pictures[pic_index.0];
    let info = &mut pic_info[pic_index.0];

    info.parent = parent_pic_index;

    // Run pre-update to resolve animation properties etc
    pic.pre_update(frame_context);

    let can_be_drawn = match info.update_pass {
        Some(update_pass) => {
            // No point in recursing into paths in the graph if this picture already
            // has been set to update after this pass.
            if update_pass > pass {
                return;
            }

            true
        }
        None => {
            // Check if this picture can be dropped from the graph we're building this frame
            pic.is_visible(frame_context.spatial_tree)
        }
    };

    if can_be_drawn {
        info.update_pass = Some(pass);

        *max_pass_index = pass.max(*max_pass_index);

        let mut child_pictures: SmallVec<[PictureIndex; 8]> = SmallVec::new();
        child_pictures.extend_from_slice(&pic.prim_list.child_pictures);

        for child_pic_index in child_pictures {
            assign_update_pass(
                child_pic_index,
                Some(pic_index),
                pass + 1,
                pictures,
                pic_info,
                max_pass_index,
                frame_context,
            );
        }
    }
}

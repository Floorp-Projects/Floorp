/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//! Utilities to deal with coordinate spaces.

use std::fmt;

use euclid::{Transform3D, Box2D, Point2D, Vector2D};

use api::units::*;
use crate::spatial_tree::{SpatialTree, CoordinateSpaceMapping, SpatialNodeIndex, VisibleFace, SpatialNodeContainer};
use crate::util::project_rect;
use crate::util::{MatrixHelpers, ScaleOffset, RectHelpers, PointHelpers};


#[derive(Debug, Clone)]
pub struct SpaceMapper<F, T> {
    kind: CoordinateSpaceMapping<F, T>,
    pub ref_spatial_node_index: SpatialNodeIndex,
    pub current_target_spatial_node_index: SpatialNodeIndex,
    pub bounds: Box2D<f32, T>,
    visible_face: VisibleFace,
}

impl<F, T> SpaceMapper<F, T> where F: fmt::Debug {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        bounds: Box2D<f32, T>,
    ) -> Self {
        SpaceMapper {
            kind: CoordinateSpaceMapping::Local,
            ref_spatial_node_index,
            current_target_spatial_node_index: ref_spatial_node_index,
            bounds,
            visible_face: VisibleFace::Front,
        }
    }

    pub fn new_with_target(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        bounds: Box2D<f32, T>,
        spatial_tree: &SpatialTree,
    ) -> Self {
        let mut mapper = Self::new(ref_spatial_node_index, bounds);
        mapper.set_target_spatial_node(target_node_index, spatial_tree);
        mapper
    }

    pub fn set_target_spatial_node(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_tree: &SpatialTree,
    ) {
        if target_node_index == self.current_target_spatial_node_index {
            return
        }

        let ref_spatial_node = spatial_tree.get_spatial_node(self.ref_spatial_node_index);
        let target_spatial_node = spatial_tree.get_spatial_node(target_node_index);
        self.visible_face = VisibleFace::Front;

        self.kind = if self.ref_spatial_node_index == target_node_index {
            CoordinateSpaceMapping::Local
        } else if ref_spatial_node.coordinate_system_id == target_spatial_node.coordinate_system_id {
            let scale_offset = ref_spatial_node.content_transform
                .inverse()
                .accumulate(&target_spatial_node.content_transform);
            CoordinateSpaceMapping::ScaleOffset(scale_offset)
        } else {
            let transform = spatial_tree
                .get_relative_transform_with_face(
                    target_node_index,
                    self.ref_spatial_node_index,
                    Some(&mut self.visible_face),
                )
                .into_transform()
                .with_source::<F>()
                .with_destination::<T>();
            CoordinateSpaceMapping::Transform(transform)
        };

        self.current_target_spatial_node_index = target_node_index;
    }

    pub fn get_transform(&self) -> Transform3D<f32, F, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Transform3D::identity()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.to_transform()
            }
            CoordinateSpaceMapping::Transform(transform) => {
                transform
            }
        }
    }

    pub fn unmap(&self, rect: &Box2D<f32, T>) -> Option<Box2D<f32, F>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(rect.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.unmap_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.inverse_rect_footprint(rect)
            }
        }
    }

    pub fn map(&self, rect: &Box2D<f32, F>) -> Option<Box2D<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(rect.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_rect(rect))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                match project_rect(transform, rect, &self.bounds) {
                    Some(bounds) => {
                        Some(bounds)
                    }
                    None => {
                        warn!("parent relative transform can't transform the primitive rect for {:?}", rect);
                        None
                    }
                }
            }
        }
    }

    // Attempt to return a rect that is contained in the mapped rect.
    pub fn map_inner_bounds(&self, rect: &Box2D<f32, F>) -> Option<Box2D<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(rect.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_rect(rect))
            }
            CoordinateSpaceMapping::Transform(..) => {
                // We could figure out a rect that is contained in the transformed rect but
                // for now we do the simple thing here and bail out.
                return None;
            }
        }
    }

    // Map a local space point to the target coordinate space
    pub fn map_point(&self, p: Point2D<f32, F>) -> Option<Point2D<f32, T>> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                Some(p.cast_unit())
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                Some(scale_offset.map_point(&p))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.transform_point2d(p)
            }
        }
    }

    pub fn map_vector(&self, v: Vector2D<f32, F>) -> Vector2D<f32, T> {
        match self.kind {
            CoordinateSpaceMapping::Local => {
                v.cast_unit()
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                scale_offset.map_vector(&v)
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                transform.transform_vector2d(v)
            }
        }
    }
}


#[derive(Clone, Debug)]
pub struct SpaceSnapper {
    ref_spatial_node_index: SpatialNodeIndex,
    current_target_spatial_node_index: SpatialNodeIndex,
    snapping_transform: Option<ScaleOffset>,
    raster_pixel_scale: RasterPixelScale,
}

impl SpaceSnapper {
    pub fn new(
        ref_spatial_node_index: SpatialNodeIndex,
        raster_pixel_scale: RasterPixelScale,
    ) -> Self {
        SpaceSnapper {
            ref_spatial_node_index,
            current_target_spatial_node_index: SpatialNodeIndex::INVALID,
            snapping_transform: None,
            raster_pixel_scale,
        }
    }

    pub fn new_with_target<S: SpatialNodeContainer>(
        ref_spatial_node_index: SpatialNodeIndex,
        target_node_index: SpatialNodeIndex,
        raster_pixel_scale: RasterPixelScale,
        spatial_tree: &S,
    ) -> Self {
        let mut snapper = SpaceSnapper {
            ref_spatial_node_index,
            current_target_spatial_node_index: SpatialNodeIndex::INVALID,
            snapping_transform: None,
            raster_pixel_scale,
        };

        snapper.set_target_spatial_node(target_node_index, spatial_tree);
        snapper
    }

    pub fn set_target_spatial_node<S: SpatialNodeContainer>(
        &mut self,
        target_node_index: SpatialNodeIndex,
        spatial_tree: &S,
    ) {
        if target_node_index == self.current_target_spatial_node_index {
            return
        }

        let ref_snap = spatial_tree.get_node_info(self.ref_spatial_node_index).snapping_transform;
        let target_snap = spatial_tree.get_node_info(target_node_index).snapping_transform;

        self.current_target_spatial_node_index = target_node_index;
        self.snapping_transform = match (ref_snap, target_snap) {
            (Some(ref ref_scale_offset), Some(ref target_scale_offset)) => {
                Some(ref_scale_offset
                    .inverse()
                    .accumulate(target_scale_offset)
                    .scale(self.raster_pixel_scale.0))
            }
            _ => None,
        };
    }

    pub fn snap_rect<F>(&self, rect: &Box2D<f32, F>) -> Box2D<f32, F> where F: fmt::Debug {
        debug_assert!(self.current_target_spatial_node_index != SpatialNodeIndex::INVALID);
        match self.snapping_transform {
            Some(ref scale_offset) => {
                let snapped_device_rect: DeviceRect = scale_offset.map_rect(rect).snap();
                scale_offset.unmap_rect(&snapped_device_rect)
            }
            None => *rect,
        }
    }

    pub fn snap_point<F>(&self, point: &Point2D<f32, F>) -> Point2D<f32, F> where F: fmt::Debug {
        debug_assert!(self.current_target_spatial_node_index != SpatialNodeIndex::INVALID);
        match self.snapping_transform {
            Some(ref scale_offset) => {
                let snapped_device_vector : DevicePoint = scale_offset.map_point(point).snap();
                scale_offset.unmap_point(&snapped_device_vector)
            }
            None => *point,
        }
    }
}

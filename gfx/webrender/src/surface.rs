/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{LayoutPixel, PicturePixel, RasterSpace};
use clip::{ClipChainId, ClipStore, ClipUid};
use clip_scroll_tree::{ClipScrollTree, SpatialNodeIndex};
use euclid::TypedTransform3D;
use internal_types::FastHashSet;
use prim_store::{CoordinateSpaceMapping, PrimitiveUid, PrimitiveInstance, PrimitiveInstanceKind};
use std::hash;
use util::ScaleOffset;

/*

    Notes for future implementation work on surface caching:

    State that can affect the contents of a cached surface:

        Primitives
            These are handled by the PrimitiveUid value. The structure interning
            code during scene building guarantees that each PrimitiveUid will
            represent a unique identifier for the content of this primitive.
        Clip chains
            Similarly, the ClipUid value uniquely identifies the contents of
            a clip node.
        Transforms
            Each picture contains a list of transforms that affect the content
            of the picture itself. The value of the surface transform relative
            to the raster root transform is only relevant if the picture is
            being rasterized in screen-space.
        External images
            An external image (e.g. video) can change the contents of a picture
            without a scene build occurring. We don't need to handle this yet,
            but once images support interning and caching, we'll need to include
            a list of external image dependencies in the cache key.
        Property animation
            Transform animations are handled by the transforms case above. We don't
            need to handle opacity animations yet, since the interning and picture
            caching doesn't support images and / or solid rects. Once those
            primitives are ported, we'll need a list of property animation keys
            that a surface depends on.

*/

// Matches the definition of SK_ScalarNearlyZero in Skia.
// TODO(gw): Some testing to see what's reasonable for this value
//           to avoid invalidating the cache for minor changes.
const QUANTIZE_SCALE: f32 = 4096.0;

fn quantize(value: f32) -> f32 {
    (value * QUANTIZE_SCALE).round() / QUANTIZE_SCALE
}

/// A quantized, hashable version of util::ScaleOffset that
/// can be used as a cache key.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, PartialEq, Clone)]
pub struct ScaleOffsetKey {
    scale_x: f32,
    scale_y: f32,
    offset_x: f32,
    offset_y: f32,
}

impl ScaleOffsetKey {
    fn new(scale_offset: &ScaleOffset) -> Self {
        // TODO(gw): Since these are quantized, it might make sense in the future to
        //           convert these to ints to remove the need for custom hash impl.
        ScaleOffsetKey {
            scale_x: quantize(scale_offset.scale.x),
            scale_y: quantize(scale_offset.scale.y),
            offset_x: quantize(scale_offset.offset.x),
            offset_y: quantize(scale_offset.offset.y),
        }
    }
}

impl Eq for ScaleOffsetKey {}

impl hash::Hash for ScaleOffsetKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        self.scale_x.to_bits().hash(state);
        self.scale_y.to_bits().hash(state);
        self.offset_x.to_bits().hash(state);
        self.offset_y.to_bits().hash(state);
    }
}

/// A quantized, hashable version of PictureTransform that
/// can be used as a cache key.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, PartialEq, Clone)]
pub struct MatrixKey {
    values: [f32; 16],
}

impl MatrixKey {
    fn new<Src, Dst>(transform: &TypedTransform3D<f32, Src, Dst>) -> Self {
        let mut values = transform.to_row_major_array();

        // TODO(gw): Since these are quantized, it might make sense in the future to
        //           convert these to ints to remove the need for custom hash impl.
        for value in &mut values {
            *value = quantize(*value);
        }

        MatrixKey {
            values,
        }
    }
}

impl Eq for MatrixKey {}

impl hash::Hash for MatrixKey {
    fn hash<H: hash::Hasher>(&self, state: &mut H) {
        for value in &self.values {
            value.to_bits().hash(state);
        }
    }
}

/// A quantized, hashable version of CoordinateSpaceMapping that
/// can be used as a cache key.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, PartialEq, Clone, Hash, Eq)]
pub enum TransformKey {
    Local,
    ScaleOffset(ScaleOffsetKey),
    Transform(MatrixKey),
}

impl TransformKey {
    pub fn local() -> Self {
        TransformKey::Local
    }
}

impl<F, T> From<CoordinateSpaceMapping<F, T>> for TransformKey {
    /// Construct a transform cache key from a coordinate space mapping.
    fn from(mapping: CoordinateSpaceMapping<F, T>) -> TransformKey {
        match mapping {
            CoordinateSpaceMapping::Local => {
                TransformKey::Local
            }
            CoordinateSpaceMapping::ScaleOffset(ref scale_offset) => {
                TransformKey::ScaleOffset(ScaleOffsetKey::new(scale_offset))
            }
            CoordinateSpaceMapping::Transform(ref transform) => {
                TransformKey::Transform(MatrixKey::new(transform))
            }
        }
    }
}

/// This key uniquely identifies the contents of a cached
/// picture surface.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Debug, Eq, PartialEq, Hash, Clone)]
pub struct SurfaceCacheKey {
    /// The list of primitives that are part of this surface.
    /// The uid uniquely identifies the content of the primitive.
    pub primitive_ids: Vec<PrimitiveUid>,
    /// The list of clips that affect the primitives on this surface.
    /// The uid uniquely identifies the content of the clip.
    pub clip_ids: Vec<ClipUid>,
    /// A list of transforms that can affect the contents of primitives
    /// and/or clips on this picture surface.
    pub transforms: Vec<TransformKey>,
    /// Information about the transform of the picture surface itself. If we are
    /// drawing in screen-space, then the value of this affects the contents
    /// of the cached surface. If we're drawing in local space, then the transform
    /// of the surface in its parent is not relevant to the contents.
    pub raster_transform: TransformKey,
}

/// A descriptor for the contents of a picture surface.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SurfaceDescriptor {
    /// The cache key identifies the contents or primitives, clips and the current
    /// state of relevant transforms.
    pub cache_key: SurfaceCacheKey,

    /// The spatial nodes array is used to update the cache key each frame, without
    /// relying on the value of a spatial node index (these may change, if other parts of the
    /// display list result in a different clip-scroll tree).
    pub spatial_nodes: Vec<SpatialNodeIndex>,
}

impl SurfaceDescriptor {
    /// Construct a new surface descriptor for this list of primitives.
    /// This method is fallible - it will return None if this picture
    /// contains primitives that can't currently be cached safely.
    pub fn new(
        prim_instances: &[PrimitiveInstance],
        pic_spatial_node_index: SpatialNodeIndex,
        clip_store: &ClipStore,
    ) -> Option<Self> {
        let mut relevant_spatial_nodes = FastHashSet::default();
        let mut primitive_ids = Vec::new();
        let mut clip_ids = Vec::new();

        for prim_instance in prim_instances {
            // If the prim has the same spatial node as the surface,
            // then the content can't move relative to it, so we don't
            // care if the transform changes.
            if pic_spatial_node_index != prim_instance.spatial_node_index {
                relevant_spatial_nodes.insert(prim_instance.spatial_node_index);
            }

            // Collect clip node transforms that we care about.
            let mut clip_chain_id = prim_instance.clip_chain_id;
            while clip_chain_id != ClipChainId::NONE {
                let clip_chain_node = &clip_store.clip_chain_nodes[clip_chain_id.0 as usize];

                // TODO(gw): This needs to be a bit more careful once we create
                //           descriptors for pictures that might be pass-through.

                // Ignore clip chain nodes that will be handled by the clip node collector.
                if clip_chain_node.spatial_node_index > pic_spatial_node_index {
                    relevant_spatial_nodes.insert(prim_instance.spatial_node_index);

                    clip_ids.push(clip_chain_node.handle.uid());
                }

                clip_chain_id = clip_chain_node.parent_clip_chain_id;
            }

            // For now, we only handle interned primitives. If we encounter
            // a legacy primitive or picture, then fail to create a cache
            // descriptor.
            match prim_instance.kind {
                PrimitiveInstanceKind::Picture { .. } |
                PrimitiveInstanceKind::LegacyPrimitive { .. } => {
                    return None;
                }
                PrimitiveInstanceKind::LineDecoration { .. } |
                PrimitiveInstanceKind::TextRun { .. } |
                PrimitiveInstanceKind::Clear => {}
            }

            // Record the unique identifier for the content represented
            // by this primitive.
            primitive_ids.push(prim_instance.prim_data_handle.uid());
        }

        // Get a list of spatial nodes that are relevant for the contents
        // of this picture. Sort them to ensure that for a given clip-scroll
        // tree, we end up with the same transform ordering.
        let mut spatial_nodes: Vec<SpatialNodeIndex> = relevant_spatial_nodes
            .into_iter()
            .collect();
        spatial_nodes.sort();

        // Create the array of transform values that gets built each
        // frame during update.
        let transforms = vec![TransformKey::local(); spatial_nodes.len()];

        let cache_key = SurfaceCacheKey {
            primitive_ids,
            clip_ids,
            transforms,
            raster_transform: TransformKey::local(),
        };

        Some(SurfaceDescriptor {
            cache_key,
            spatial_nodes,
        })
    }

    /// Update the transforms for this cache key, by extracting the current
    /// values from the clip-scroll tree state.
    pub fn update(
        &mut self,
        surface_spatial_node_index: SpatialNodeIndex,
        raster_spatial_node_index: SpatialNodeIndex,
        clip_scroll_tree: &ClipScrollTree,
        raster_space: RasterSpace,
    ) {
        // Update the state of the transform for compositing this picture.
        self.cache_key.raster_transform = match raster_space {
            RasterSpace::Screen => {
                // In general cases, if we're rasterizing a picture in screen space, then the
                // value of the surface spatial node will affect the contents of the picture
                // itself. However, if the surface and raster spatial nodes are in the same
                // coordinate system (which is the common case!) then we are effectively drawing
                // in a local space anyway, so don't care about that transform for the purposes
                // of validating the surface cache contents.
                let raster_spatial_node = &clip_scroll_tree.spatial_nodes[raster_spatial_node_index.0];
                let surface_spatial_node = &clip_scroll_tree.spatial_nodes[surface_spatial_node_index.0];

                let mut key = CoordinateSpaceMapping::<LayoutPixel, PicturePixel>::new(
                    raster_spatial_node_index,
                    surface_spatial_node_index,
                    clip_scroll_tree,
                ).into();

                if let TransformKey::ScaleOffset(ref mut key) = key {
                    if raster_spatial_node.coordinate_system_id == surface_spatial_node.coordinate_system_id {
                        key.offset_x = 0.0;
                        key.offset_y = 0.0;
                    }
                }

                key
            }
            RasterSpace::Local(..) => {
                TransformKey::local()
            }
        };

        // Update the state of any relevant transforms for this picture.
        for (spatial_node_index, transform) in self.spatial_nodes
            .iter()
            .zip(self.cache_key.transforms.iter_mut())
        {
            *transform = CoordinateSpaceMapping::<LayoutPixel, PicturePixel>::new(
                raster_spatial_node_index,
                *spatial_node_index,
                clip_scroll_tree,
            ).into();
        }
    }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, HitTestResultItem, HitTestResult, ItemTag, PrimitiveFlags};
use api::{PipelineId, ApiHitTester};
use api::units::*;
use crate::clip::{rounded_rectangle_contains_point, ClipNodeId, ClipTreeBuilder};
use crate::clip::{polygon_contains_point, ClipItemKey, ClipItemKeyKind};
use crate::prim_store::PolygonKey;
use crate::scene_builder_thread::Interners;
use crate::spatial_tree::{SpatialNodeIndex, SpatialTree, get_external_scroll_offset};
use crate::internal_types::{FastHashMap, LayoutPrimitiveInfo};
use std::sync::{Arc, Mutex};
use crate::util::{LayoutToWorldFastTransform};

pub struct SharedHitTester {
    // We don't really need a mutex here. We could do with some sort of
    // atomic-atomic-ref-counted pointer (an Arc which would let the pointer
    // be swapped atomically like an AtomicPtr).
    // In practive this shouldn't cause performance issues, though.
    hit_tester: Mutex<Arc<HitTester>>,
}

impl SharedHitTester {
    pub fn new() -> Self {
        SharedHitTester {
            hit_tester: Mutex::new(Arc::new(HitTester::empty())),
        }
    }

    pub fn get_ref(&self) -> Arc<HitTester> {
        let guard = self.hit_tester.lock().unwrap();
        Arc::clone(&*guard)
    }

    pub(crate) fn update(&self, new_hit_tester: Arc<HitTester>) {
        let mut guard = self.hit_tester.lock().unwrap();
        *guard = new_hit_tester;
    }
}

impl ApiHitTester for SharedHitTester {
    fn hit_test(&self,
        point: WorldPoint,
    ) -> HitTestResult {
        self.get_ref().hit_test(HitTest::new(point))
    }
}

/// A copy of important spatial node data to use during hit testing. This a copy of
/// data from the SpatialTree that will persist as a new frame is under construction,
/// allowing hit tests consistent with the currently rendered frame.
#[derive(MallocSizeOf)]
struct HitTestSpatialNode {
    /// The pipeline id of this node.
    pipeline_id: PipelineId,

    /// World transform for content transformed by this node.
    world_content_transform: LayoutToWorldFastTransform,

    /// World viewport transform for content transformed by this node.
    world_viewport_transform: LayoutToWorldFastTransform,

    /// The accumulated external scroll offset for this spatial node.
    external_scroll_offset: LayoutVector2D,
}

#[derive(MallocSizeOf)]
struct HitTestClipNode {
    /// A particular point must be inside all of these regions to be considered clipped in
    /// for the purposes of a hit test.
    region: HitTestRegion,
    /// The positioning node for this clip
    spatial_node_index: SpatialNodeIndex,
    /// Parent clip node
    parent: ClipNodeId,
}

impl HitTestClipNode {
    fn new(
        item: &ClipItemKey,
        interners: &Interners,
        parent: ClipNodeId,
    ) -> Self {
        let region = match item.kind {
            ClipItemKeyKind::Rectangle(rect, mode) => {
                HitTestRegion::Rectangle(rect.into(), mode)
            }
            ClipItemKeyKind::RoundedRectangle(rect, radius, mode) => {
                HitTestRegion::RoundedRectangle(rect.into(), radius.into(), mode)
            }
            ClipItemKeyKind::ImageMask(rect, _, _, polygon_handle) => {
                if let Some(handle) = polygon_handle {
                    // Retrieve the polygon data from the interner.
                    let polygon = &interners.polygon[handle];
                    HitTestRegion::Polygon(rect.into(), *polygon)
                } else {
                    HitTestRegion::Rectangle(rect.into(), ClipMode::Clip)
                }
            }
            ClipItemKeyKind::BoxShadow(..) => HitTestRegion::Invalid,
        };

        HitTestClipNode {
            region,
            spatial_node_index: item.spatial_node_index,
            parent,
        }
    }
}

#[derive(Clone, MallocSizeOf)]
struct HitTestingItem {
    rect: LayoutRect,
    tag: ItemTag,
    animation_id: u64,
    is_backface_visible: bool,
    spatial_node_index: SpatialNodeIndex,
    clip_node_id: ClipNodeId,
}

impl HitTestingItem {
    fn new(
        tag: ItemTag,
        animation_id: u64,
        info: &LayoutPrimitiveInfo,
        spatial_node_index: SpatialNodeIndex,
        clip_node_id: ClipNodeId,
    ) -> HitTestingItem {
        HitTestingItem {
            rect: info.rect,
            tag,
            animation_id,
            is_backface_visible: info.flags.contains(PrimitiveFlags::IS_BACKFACE_VISIBLE),
            spatial_node_index,
            clip_node_id,
        }
    }
}

/// Statistics about allocation sizes of current hit tester,
/// used to pre-allocate size of the next hit tester.
pub struct HitTestingSceneStats {
    pub clip_nodes_count: usize,
    pub items_count: usize,
}

impl HitTestingSceneStats {
    pub fn empty() -> Self {
        HitTestingSceneStats {
            clip_nodes_count: 0,
            items_count: 0,
        }
    }
}

#[derive(MallocSizeOf, Debug, Copy, Clone)]
pub struct ClipNodeIndex(u32);

/// Defines the immutable part of a hit tester for a given scene.
/// The hit tester is recreated each time a frame is built, since
/// it relies on the current values of the spatial tree.
/// However, the clip chain and item definitions don't change,
/// so they are created once per scene, and shared between
/// hit tester instances via Arc.
#[derive(MallocSizeOf)]
pub struct HitTestingScene {
    clip_nodes: FastHashMap<ClipNodeId, HitTestClipNode>,

    /// List of hit testing primitives.
    items: Vec<HitTestingItem>,
}

impl HitTestingScene {
    /// Construct a new hit testing scene, pre-allocating to size
    /// provided by previous scene stats.
    pub fn new(stats: &HitTestingSceneStats) -> Self {
        HitTestingScene {
            clip_nodes: FastHashMap::default(),
            items: Vec::with_capacity(stats.items_count),
        }
    }

    /// Get stats about the current scene allocation sizes.
    pub fn get_stats(&self) -> HitTestingSceneStats {
        HitTestingSceneStats {
            clip_nodes_count: 0,
            items_count: self.items.len(),
        }
    }

    fn add_clip_node(
        &mut self,
        clip_node_id: ClipNodeId,
        clip_tree_builder: &ClipTreeBuilder,
        interners: &Interners,
    ) {
        if clip_node_id == ClipNodeId::NONE {
            return;
        }

        if !self.clip_nodes.contains_key(&clip_node_id) {
            let src_clip_node = clip_tree_builder.get_node(clip_node_id);
            let clip_item = &interners.clip[src_clip_node.handle];

            let clip_node = HitTestClipNode::new(
                &clip_item.key,
                interners,
                src_clip_node.parent,
            );

            self.clip_nodes.insert(clip_node_id, clip_node);

            self.add_clip_node(
                src_clip_node.parent,
                clip_tree_builder,
                interners,
            );
        }
    }

    /// Add a hit testing primitive.
    pub fn add_item(
        &mut self,
        tag: ItemTag,
        anim_id: u64,
        info: &LayoutPrimitiveInfo,
        spatial_node_index: SpatialNodeIndex,
        clip_node_id: ClipNodeId,
        clip_tree_builder: &ClipTreeBuilder,
        interners: &Interners,
    ) {
        self.add_clip_node(
            clip_node_id,
            clip_tree_builder,
            interners,
        );

        let item = HitTestingItem::new(
            tag,
            anim_id,
            info,
            spatial_node_index,
            clip_node_id,
        );

        self.items.push(item);
    }
}

#[derive(MallocSizeOf)]
enum HitTestRegion {
    Invalid,
    Rectangle(LayoutRect, ClipMode),
    RoundedRectangle(LayoutRect, BorderRadius, ClipMode),
    Polygon(LayoutRect, PolygonKey),
}

impl HitTestRegion {
    fn contains(&self, point: &LayoutPoint) -> bool {
        match *self {
            HitTestRegion::Rectangle(ref rectangle, ClipMode::Clip) =>
                rectangle.contains(*point),
            HitTestRegion::Rectangle(ref rectangle, ClipMode::ClipOut) =>
                !rectangle.contains(*point),
            HitTestRegion::RoundedRectangle(rect, radii, ClipMode::Clip) =>
                rounded_rectangle_contains_point(point, &rect, &radii),
            HitTestRegion::RoundedRectangle(rect, radii, ClipMode::ClipOut) =>
                !rounded_rectangle_contains_point(point, &rect, &radii),
            HitTestRegion::Polygon(rect, polygon) =>
                polygon_contains_point(point, &rect, &polygon),
            HitTestRegion::Invalid => true,
        }
    }
}

#[derive(MallocSizeOf)]
pub struct HitTester {
    #[ignore_malloc_size_of = "Arc"]
    scene: Arc<HitTestingScene>,
    spatial_nodes: FastHashMap<SpatialNodeIndex, HitTestSpatialNode>,
}

impl HitTester {
    pub fn empty() -> Self {
        HitTester {
            scene: Arc::new(HitTestingScene::new(&HitTestingSceneStats::empty())),
            spatial_nodes: FastHashMap::default(),
        }
    }

    pub fn new(
        scene: Arc<HitTestingScene>,
        spatial_tree: &SpatialTree,
    ) -> HitTester {
        let mut hit_tester = HitTester {
            scene,
            spatial_nodes: FastHashMap::default(),
        };
        hit_tester.read_spatial_tree(spatial_tree);
        hit_tester
    }

    fn read_spatial_tree(
        &mut self,
        spatial_tree: &SpatialTree,
    ) {
        self.spatial_nodes.clear();
        self.spatial_nodes.reserve(spatial_tree.spatial_node_count());

        spatial_tree.visit_nodes(|index, node| {
            //TODO: avoid inverting more than necessary:
            //  - if the coordinate system is non-invertible, no need to try any of these concrete transforms
            //  - if there are other places where inversion is needed, let's not repeat the step

            self.spatial_nodes.insert(index, HitTestSpatialNode {
                pipeline_id: node.pipeline_id,
                world_content_transform: spatial_tree
                    .get_world_transform(index)
                    .into_fast_transform(),
                world_viewport_transform: spatial_tree
                    .get_world_viewport_transform(index)
                    .into_fast_transform(),
                external_scroll_offset: get_external_scroll_offset(spatial_tree, index),
            });
        });
    }

    pub fn hit_test(&self, test: HitTest) -> HitTestResult {
        let mut result = HitTestResult::default();

        let mut current_spatial_node_index = SpatialNodeIndex::INVALID;
        let mut point_in_layer = None;

        // For each hit test primitive
        for item in self.scene.items.iter().rev() {
            let scroll_node = &self.spatial_nodes[&item.spatial_node_index];
            let pipeline_id = scroll_node.pipeline_id;

            // Update the cached point in layer space, if the spatial node
            // changed since last primitive.
            if item.spatial_node_index != current_spatial_node_index {
                point_in_layer = scroll_node
                    .world_content_transform
                    .inverse()
                    .and_then(|inverted| inverted.project_point2d(test.point));
                current_spatial_node_index = item.spatial_node_index;
            }

            // Only consider hit tests on transformable layers.
            let point_in_layer = match point_in_layer {
                Some(p) => p,
                None => continue,
            };

            // If the item's rect or clip rect don't contain this point, it's
            // not a valid hit.
            if !item.rect.contains(point_in_layer) {
                continue;
            }

            // See if any of the clips for this primitive cull out the item.
            let mut current_clip_node_id = item.clip_node_id;
            let mut is_valid = true;

            while current_clip_node_id != ClipNodeId::NONE {
                let clip_node = &self.scene.clip_nodes[&current_clip_node_id];

                let transform = self
                    .spatial_nodes[&clip_node.spatial_node_index]
                    .world_content_transform;
                if let Some(transformed_point) = transform
                    .inverse()
                    .and_then(|inverted| inverted.project_point2d(test.point))
                {
                    if !clip_node.region.contains(&transformed_point) {
                        is_valid = false;
                        break;
                    }
                }

                current_clip_node_id = clip_node.parent;
            }

            if !is_valid {
                continue;
            }

            // Don't hit items with backface-visibility:hidden if they are facing the back.
            if !item.is_backface_visible && scroll_node.world_content_transform.is_backface_visible() {
                continue;
            }

            result.items.push(HitTestResultItem {
                pipeline: pipeline_id,
                tag: item.tag,
                animation_id: item.animation_id,
            });
        }

        result.items.dedup();
        result
    }
}

#[derive(MallocSizeOf)]
pub struct HitTest {
    point: WorldPoint,
}

impl HitTest {
    pub fn new(
        point: WorldPoint,
    ) -> HitTest {
        HitTest {
            point,
        }
    }
}

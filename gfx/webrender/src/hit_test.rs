/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipMode, HitTestFlags, HitTestItem, HitTestResult, ItemTag, LayoutPoint};
use api::{LayoutPrimitiveInfo, LayoutRect, PipelineId, VoidPtrToSizeFn, WorldPoint};
use clip::{ClipDataStore, ClipNode, ClipItem, ClipStore};
use clip::{rounded_rectangle_contains_point};
use clip_scroll_tree::{SpatialNodeIndex, ClipScrollTree};
use internal_types::FastHashMap;
use prim_store::ScrollNodeAndClipChain;
use std::os::raw::c_void;
use std::u32;
use util::LayoutToWorldFastTransform;

/// A copy of important clip scroll node data to use during hit testing. This a copy of
/// data from the ClipScrollTree that will persist as a new frame is under construction,
/// allowing hit tests consistent with the currently rendered frame.
pub struct HitTestSpatialNode {
    /// The pipeline id of this node.
    pipeline_id: PipelineId,

    /// World transform for content transformed by this node.
    world_content_transform: LayoutToWorldFastTransform,

    /// World viewport transform for content transformed by this node.
    world_viewport_transform: LayoutToWorldFastTransform,
}

pub struct HitTestClipNode {
    /// A particular point must be inside all of these regions to be considered clipped in
    /// for the purposes of a hit test.
    region: HitTestRegion,
}

impl HitTestClipNode {
    fn new(node: &ClipNode) -> Self {
        let region = match node.item {
            ClipItem::Rectangle(ref rect, mode) => HitTestRegion::Rectangle(*rect, mode),
            ClipItem::RoundedRectangle(ref rect, ref radii, ref mode) =>
                HitTestRegion::RoundedRectangle(*rect, *radii, *mode),
            ClipItem::Image(ref mask, ..) => HitTestRegion::Rectangle(mask.rect, ClipMode::Clip),
            ClipItem::BoxShadow(_) => HitTestRegion::Invalid,
        };

        HitTestClipNode {
            region,
        }
    }
}

// A hit testing clip chain node is the same as a
// normal clip chain node, except that the clip
// node is embedded inside the clip chain, rather
// than referenced. This means we don't need to
// copy the complete interned clip data store for
// hit testing.

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct HitTestClipChainId(u32);

impl HitTestClipChainId {
    pub const NONE: Self = HitTestClipChainId(u32::MAX);
}

pub struct HitTestClipChainNode {
    pub region: HitTestClipNode,
    pub spatial_node_index: SpatialNodeIndex,
    pub parent_clip_chain_id: HitTestClipChainId,
}

#[derive(Clone)]
pub struct HitTestingItem {
    rect: LayoutRect,
    clip_rect: LayoutRect,
    tag: ItemTag,
    is_backface_visible: bool,
}

impl HitTestingItem {
    pub fn new(tag: ItemTag, info: &LayoutPrimitiveInfo) -> HitTestingItem {
        HitTestingItem {
            rect: info.rect,
            clip_rect: info.clip_rect,
            tag,
            is_backface_visible: info.is_backface_visible,
        }
    }
}

#[derive(Clone)]
pub struct HitTestingRun(pub Vec<HitTestingItem>, pub ScrollNodeAndClipChain);

enum HitTestRegion {
    Invalid,
    Rectangle(LayoutRect, ClipMode),
    RoundedRectangle(LayoutRect, BorderRadius, ClipMode),
}

impl HitTestRegion {
    pub fn contains(&self, point: &LayoutPoint) -> bool {
        match *self {
            HitTestRegion::Rectangle(ref rectangle, ClipMode::Clip) =>
                rectangle.contains(point),
            HitTestRegion::Rectangle(ref rectangle, ClipMode::ClipOut) =>
                !rectangle.contains(point),
            HitTestRegion::RoundedRectangle(rect, radii, ClipMode::Clip) =>
                rounded_rectangle_contains_point(point, &rect, &radii),
            HitTestRegion::RoundedRectangle(rect, radii, ClipMode::ClipOut) =>
                !rounded_rectangle_contains_point(point, &rect, &radii),
            HitTestRegion::Invalid => true,
        }
    }
}

pub struct HitTester {
    runs: Vec<HitTestingRun>,
    spatial_nodes: Vec<HitTestSpatialNode>,
    clip_chains: Vec<HitTestClipChainNode>,
    pipeline_root_nodes: FastHashMap<PipelineId, SpatialNodeIndex>,
}

impl HitTester {
    pub fn new(
        runs: &Vec<HitTestingRun>,
        clip_scroll_tree: &ClipScrollTree,
        clip_store: &ClipStore,
        clip_data_store: &ClipDataStore,
    ) -> HitTester {
        let mut hit_tester = HitTester {
            runs: runs.clone(),
            spatial_nodes: Vec::new(),
            clip_chains: Vec::new(),
            pipeline_root_nodes: FastHashMap::default(),
        };
        hit_tester.read_clip_scroll_tree(
            clip_scroll_tree,
            clip_store,
            clip_data_store,
        );
        hit_tester
    }

    fn read_clip_scroll_tree(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        clip_store: &ClipStore,
        clip_data_store: &ClipDataStore,
    ) {
        self.spatial_nodes.clear();
        self.clip_chains.clear();

        for (index, node) in clip_scroll_tree.spatial_nodes.iter().enumerate() {
            let index = SpatialNodeIndex(index);

            // If we haven't already seen a node for this pipeline, record this one as the root
            // node.
            self.pipeline_root_nodes.entry(node.pipeline_id).or_insert(index);

            self.spatial_nodes.push(HitTestSpatialNode {
                pipeline_id: node.pipeline_id,
                world_content_transform: node.world_content_transform,
                world_viewport_transform: node.world_viewport_transform,
            });
        }

        // For each clip chain node, extract the clip node from the clip
        // data store, and store it inline with the clip chain node.
        for node in &clip_store.clip_chain_nodes {
            let clip_node = &clip_data_store[node.handle];
            self.clip_chains.push(HitTestClipChainNode {
                region: HitTestClipNode::new(clip_node),
                spatial_node_index: node.spatial_node_index,
                parent_clip_chain_id: HitTestClipChainId(node.parent_clip_chain_id.0),
            });
        }
    }

    fn is_point_clipped_in_for_clip_chain(
        &self,
        point: WorldPoint,
        clip_chain_id: HitTestClipChainId,
        test: &mut HitTest
    ) -> bool {
        if clip_chain_id == HitTestClipChainId::NONE {
            return true;
        }

        if let Some(result) = test.get_from_clip_chain_cache(clip_chain_id) {
            return result == ClippedIn::ClippedIn;
        }

        let descriptor = &self.clip_chains[clip_chain_id.0 as usize];
        let parent_clipped_in = self.is_point_clipped_in_for_clip_chain(
            point,
            descriptor.parent_clip_chain_id,
            test,
        );

        if !parent_clipped_in {
            test.set_in_clip_chain_cache(clip_chain_id, ClippedIn::NotClippedIn);
            return false;
        }

        if !self.is_point_clipped_in_for_clip_node(
            point,
            clip_chain_id,
            descriptor.spatial_node_index,
            test,
        ) {
            test.set_in_clip_chain_cache(clip_chain_id, ClippedIn::NotClippedIn);
            return false;
        }

        test.set_in_clip_chain_cache(clip_chain_id, ClippedIn::ClippedIn);
        true
    }

    fn is_point_clipped_in_for_clip_node(
        &self,
        point: WorldPoint,
        clip_chain_node_id: HitTestClipChainId,
        spatial_node_index: SpatialNodeIndex,
        test: &mut HitTest
    ) -> bool {
        if let Some(clipped_in) = test.node_cache.get(&clip_chain_node_id) {
            return *clipped_in == ClippedIn::ClippedIn;
        }

        let node = &self.clip_chains[clip_chain_node_id.0 as usize].region;
        let transform = self
            .spatial_nodes[spatial_node_index.0]
            .world_viewport_transform;
        let transformed_point = match transform
            .inverse()
            .and_then(|inverted| inverted.transform_point2d(&point))
        {
            Some(point) => point,
            None => {
                test.node_cache.insert(clip_chain_node_id, ClippedIn::NotClippedIn);
                return false;
            }
        };

        if !node.region.contains(&transformed_point) {
            test.node_cache.insert(clip_chain_node_id, ClippedIn::NotClippedIn);
            return false;
        }

        test.node_cache.insert(clip_chain_node_id, ClippedIn::ClippedIn);
        true
    }

    pub fn find_node_under_point(&self, mut test: HitTest) -> Option<SpatialNodeIndex> {
        let point = test.get_absolute_point(self);

        for &HitTestingRun(ref items, ref clip_and_scroll) in self.runs.iter().rev() {
            let spatial_node_index = clip_and_scroll.spatial_node_index;
            let scroll_node = &self.spatial_nodes[spatial_node_index.0];
            let transform = scroll_node.world_content_transform;
            let point_in_layer = match transform
                .inverse()
                .and_then(|inverted| inverted.transform_point2d(&point))
            {
                Some(point) => point,
                None => continue,
            };

            let mut clipped_in = false;
            for item in items.iter().rev() {
                if !item.rect.contains(&point_in_layer) ||
                    !item.clip_rect.contains(&point_in_layer) {
                    continue;
                }

                let clip_chain_id = HitTestClipChainId(clip_and_scroll.clip_chain_id.0);
                clipped_in |=
                    self.is_point_clipped_in_for_clip_chain(point, clip_chain_id, &mut test);
                if !clipped_in {
                    break;
                }

                return Some(spatial_node_index);
            }
        }

        None
    }

    pub fn hit_test(&self, mut test: HitTest) -> HitTestResult {
        let point = test.get_absolute_point(self);

        let mut result = HitTestResult::default();
        for &HitTestingRun(ref items, ref clip_and_scroll) in self.runs.iter().rev() {
            let spatial_node_index = clip_and_scroll.spatial_node_index;
            let scroll_node = &self.spatial_nodes[spatial_node_index.0];
            let pipeline_id = scroll_node.pipeline_id;
            match (test.pipeline_id, pipeline_id) {
                (Some(id), node_id) if node_id != id => continue,
                _ => {},
            }

            let transform = scroll_node.world_content_transform;
            let mut facing_backwards: Option<bool> = None;  // will be computed on first use
            let point_in_layer = match transform
                .inverse()
                .and_then(|inverted| inverted.transform_point2d(&point))
            {
                Some(point) => point,
                None => continue,
            };

            let mut clipped_in = false;
            for item in items.iter().rev() {
                if !item.rect.contains(&point_in_layer) ||
                    !item.clip_rect.contains(&point_in_layer) {
                    continue;
                }

                let clip_chain_id = HitTestClipChainId(clip_and_scroll.clip_chain_id.0);
                clipped_in = clipped_in ||
                    self.is_point_clipped_in_for_clip_chain(point, clip_chain_id, &mut test);
                if !clipped_in {
                    break;
                }

                // Don't hit items with backface-visibility:hidden if they are facing the back.
                if !item.is_backface_visible {
                    if *facing_backwards.get_or_insert_with(|| transform.is_backface_visible()) {
                        continue;
                    }
                }

                // We need to calculate the position of the test point relative to the origin of
                // the pipeline of the hit item. If we cannot get a transformed point, we are
                // in a situation with an uninvertible transformation so we should just skip this
                // result.
                let root_node = &self.spatial_nodes[self.pipeline_root_nodes[&pipeline_id].0];
                let point_in_viewport = match root_node.world_viewport_transform
                    .inverse()
                    .and_then(|inverted| inverted.transform_point2d(&point))
                {
                    Some(point) => point,
                    None => continue,
                };

                result.items.push(HitTestItem {
                    pipeline: pipeline_id,
                    tag: item.tag,
                    point_in_viewport,
                    point_relative_to_item: point_in_layer - item.rect.origin.to_vector(),
                });
                if !test.flags.contains(HitTestFlags::FIND_ALL) {
                    return result;
                }
            }
        }

        result.items.dedup();
        result
    }

    pub fn get_pipeline_root(&self, pipeline_id: PipelineId) -> &HitTestSpatialNode {
        &self.spatial_nodes[self.pipeline_root_nodes[&pipeline_id].0]
    }

    // Reports the CPU heap usage of this HitTester struct.
    pub fn malloc_size_of(&self, op: VoidPtrToSizeFn) -> usize {
        let mut size = 0;
        unsafe {
            size += op(self.runs.as_ptr() as *const c_void);
            size += op(self.spatial_nodes.as_ptr() as *const c_void);
            size += op(self.clip_chains.as_ptr() as *const c_void);
            // We can't measure pipeline_root_nodes because we don't have the
            // real machinery from the malloc_size_of crate. We could estimate
            // it but it should generally be very small so we don't bother.
        }
        size
    }
}

#[derive(Clone, Copy, PartialEq)]
enum ClippedIn {
    ClippedIn,
    NotClippedIn,
}

pub struct HitTest {
    pipeline_id: Option<PipelineId>,
    point: WorldPoint,
    flags: HitTestFlags,
    node_cache: FastHashMap<HitTestClipChainId, ClippedIn>,
    clip_chain_cache: Vec<Option<ClippedIn>>,
}

impl HitTest {
    pub fn new(
        pipeline_id: Option<PipelineId>,
        point: WorldPoint,
        flags: HitTestFlags,
    ) -> HitTest {
        HitTest {
            pipeline_id,
            point,
            flags,
            node_cache: FastHashMap::default(),
            clip_chain_cache: Vec::new(),
        }
    }

    fn get_from_clip_chain_cache(&mut self, index: HitTestClipChainId) -> Option<ClippedIn> {
        let index = index.0 as usize;
        if index >= self.clip_chain_cache.len() {
            None
        } else {
            self.clip_chain_cache[index]
        }
    }

    fn set_in_clip_chain_cache(&mut self, index: HitTestClipChainId, value: ClippedIn) {
        let index = index.0 as usize;
        if index >= self.clip_chain_cache.len() {
            self.clip_chain_cache.resize(index + 1, None);
        }
        self.clip_chain_cache[index] = Some(value);
    }

    fn get_absolute_point(&self, hit_tester: &HitTester) -> WorldPoint {
        if !self.flags.contains(HitTestFlags::POINT_RELATIVE_TO_PIPELINE_VIEWPORT) {
            return self.point;
        }

        let point =  &LayoutPoint::new(self.point.x, self.point.y);
        self.pipeline_id
            .and_then(|id|
                hit_tester
                    .get_pipeline_root(id)
                    .world_viewport_transform
                    .transform_point2d(point)
            )
            .unwrap_or_else(|| {
                WorldPoint::new(self.point.x, self.point.y)
            })
    }
}

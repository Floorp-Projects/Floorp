/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipId, ClipMode, HitTestFlags, HitTestItem, HitTestResult, ItemTag};
use api::{LayerPoint, LayerPrimitiveInfo, LayerRect, LocalClip, PipelineId, WorldPoint};
use clip::{ClipSource, ClipStore, Contains, rounded_rectangle_contains_point};
use clip_scroll_node::{ClipScrollNode, NodeType};
use clip_scroll_tree::{ClipChainIndex, ClipScrollTree};
use internal_types::FastHashMap;
use prim_store::ScrollNodeAndClipChain;
use util::LayerToWorldFastTransform;

/// A copy of important clip scroll node data to use during hit testing. This a copy of
/// data from the ClipScrollTree that will persist as a new frame is under construction,
/// allowing hit tests consistent with the currently rendered frame.
pub struct HitTestClipScrollNode {
    /// A particular point must be inside all of these regions to be considered clipped in
    /// for the purposes of a hit test.
    regions: Vec<HitTestRegion>,

    /// World transform for content transformed by this node.
    world_content_transform: LayerToWorldFastTransform,

    /// World viewport transform for content transformed by this node.
    world_viewport_transform: LayerToWorldFastTransform,

    /// Origin of the viewport of the node, used to calculate node-relative positions.
    node_origin: LayerPoint,
}

/// A description of a clip chain in the HitTester. This is used to describe
/// hierarchical clip scroll nodes as well as ClipChains, so that they can be
/// handled the same way during hit testing. Once we represent all ClipChains
/// using ClipChainDescriptors, we can get rid of this and just use the
/// ClipChainDescriptor here.
#[derive(Clone)]
struct HitTestClipChainDescriptor {
    parent: Option<ClipChainIndex>,
    clips: Vec<ClipId>,
}

impl HitTestClipChainDescriptor {
    fn empty() -> HitTestClipChainDescriptor {
        HitTestClipChainDescriptor {
            parent: None,
            clips: Vec::new(),
        }
    }
}

#[derive(Clone)]
pub struct HitTestingItem {
    rect: LayerRect,
    clip: LocalClip,
    tag: ItemTag,
}

impl HitTestingItem {
    pub fn new(tag: ItemTag, info: &LayerPrimitiveInfo) -> HitTestingItem {
        HitTestingItem {
            rect: info.rect,
            clip: info.local_clip,
            tag: tag,
        }
    }
}

#[derive(Clone)]
pub struct HitTestingRun(pub Vec<HitTestingItem>, pub ScrollNodeAndClipChain);

enum HitTestRegion {
    Rectangle(LayerRect),
    RoundedRectangle(LayerRect, BorderRadius, ClipMode),
}

impl HitTestRegion {
    pub fn contains(&self, point: &LayerPoint) -> bool {
        match self {
            &HitTestRegion::Rectangle(ref rectangle) => rectangle.contains(point),
            &HitTestRegion::RoundedRectangle(rect, radii, ClipMode::Clip) =>
                rounded_rectangle_contains_point(point, &rect, &radii),
            &HitTestRegion::RoundedRectangle(rect, radii, ClipMode::ClipOut) =>
                !rounded_rectangle_contains_point(point, &rect, &radii),
        }
    }
}

pub struct HitTester {
    runs: Vec<HitTestingRun>,
    nodes: FastHashMap<ClipId, HitTestClipScrollNode>,
    clip_chains: Vec<HitTestClipChainDescriptor>,
}

impl HitTester {
    pub fn new(
        runs: &Vec<HitTestingRun>,
        clip_scroll_tree: &ClipScrollTree,
        clip_store: &ClipStore
    ) -> HitTester {
        let mut hit_tester = HitTester {
            runs: runs.clone(),
            nodes: FastHashMap::default(),
            clip_chains: Vec::new(),
        };
        hit_tester.read_clip_scroll_tree(clip_scroll_tree, clip_store);
        hit_tester
    }

    fn read_clip_scroll_tree(
        &mut self,
        clip_scroll_tree: &ClipScrollTree,
        clip_store: &ClipStore
    ) {
        self.nodes.clear();
        self.clip_chains.clear();
        self.clip_chains.resize(
            clip_scroll_tree.clip_chains.len(),
            HitTestClipChainDescriptor::empty()
        );

        for (id, node) in &clip_scroll_tree.nodes {
            self.nodes.insert(*id, HitTestClipScrollNode {
                regions: get_regions_for_clip_scroll_node(node, clip_store),
                world_content_transform: node.world_content_transform,
                world_viewport_transform: node.world_viewport_transform,
                node_origin: node.local_viewport_rect.origin,
            });

            if let NodeType::Clip { clip_chain_index, .. } = node.node_type {
              let clip_chain = self.clip_chains.get_mut(clip_chain_index.0).unwrap();
              clip_chain.parent =
                  clip_scroll_tree.get_clip_chain(clip_chain_index).parent_index;
              clip_chain.clips = vec![*id];
            }
        }

        for descriptor in &clip_scroll_tree.clip_chains_descriptors {
            let clip_chain = self.clip_chains.get_mut(descriptor.index.0).unwrap();
            clip_chain.parent = clip_scroll_tree.get_clip_chain(descriptor.index).parent_index;
            clip_chain.clips = descriptor.clips.clone();
        }
    }

    fn is_point_clipped_in_for_clip_chain(
        &self,
        point: WorldPoint,
        clip_chain_index: ClipChainIndex,
        test: &mut HitTest
    ) -> bool {
        if let Some(result) = test.get_from_clip_chain_cache(clip_chain_index) {
            return result;
        }

        let descriptor = &self.clip_chains[clip_chain_index.0];
        let parent_clipped_in = match descriptor.parent {
            None => true,
            Some(parent) => self.is_point_clipped_in_for_clip_chain(point, parent, test),
        };

        if !parent_clipped_in {
            test.set_in_clip_chain_cache(clip_chain_index, false);
            return false;
        }

        for clip_node in &descriptor.clips {
            if !self.is_point_clipped_in_for_node(point, clip_node, test) {
                test.set_in_clip_chain_cache(clip_chain_index, false);
                return false;
            }
        }

        test.set_in_clip_chain_cache(clip_chain_index, true);
        true
    }

    fn is_point_clipped_in_for_node(
        &self,
        point: WorldPoint,
        node_id: &ClipId,
        test: &mut HitTest
    ) -> bool {
        if let Some(point) = test.node_cache.get(node_id) {
            return point.is_some();
        }

        let node = self.nodes.get(node_id).unwrap();
        let transform = node.world_viewport_transform;
        let transformed_point = match transform.inverse() {
            Some(inverted) => inverted.transform_point2d(&point),
            None => {
                test.node_cache.insert(*node_id, None);
                return false;
            }
        };

        let point_in_layer = transformed_point - node.node_origin.to_vector();
        for region in &node.regions {
            if !region.contains(&transformed_point) {
                test.node_cache.insert(*node_id, None);
                return false;
            }
        }

        test.node_cache.insert(*node_id, Some(point_in_layer));
        true
    }

    pub fn hit_test(&self, mut test: HitTest) -> HitTestResult {
        let point = test.get_absolute_point(self);

        let mut result = HitTestResult::default();
        for &HitTestingRun(ref items, ref clip_and_scroll) in self.runs.iter().rev() {
            let scroll_node_id = clip_and_scroll.scroll_node_id;
            let scroll_node = &self.nodes[&scroll_node_id];
            let pipeline_id = scroll_node_id.pipeline_id();
            match (test.pipeline_id, clip_and_scroll.scroll_node_id.pipeline_id()) {
                (Some(id), node_id) if node_id != id => continue,
                _ => {},
            }

            let transform = scroll_node.world_content_transform;
            let point_in_layer = match transform.inverse() {
                Some(inverted) => inverted.transform_point2d(&point),
                None => continue,
            };

            let mut clipped_in = false;
            for item in items.iter().rev() {
                if !item.rect.contains(&point_in_layer) || !item.clip.contains(&point_in_layer) {
                    continue;
                }

                let clip_chain_index = clip_and_scroll.clip_chain_index;
                clipped_in |=
                    self.is_point_clipped_in_for_clip_chain(point, clip_chain_index, &mut test);
                if !clipped_in {
                    break;
                }

                // We need to trigger a lookup against the root reference frame here, because
                // items that are clipped by clip chains won't test against that part of the
                // hierarchy. If we don't have a valid point for this test, we are likely
                // in a situation where the reference frame has an univertible transform, but the
                // item's clip does not.
                let root_reference_frame = ClipId::root_reference_frame(pipeline_id);
                if !self.is_point_clipped_in_for_node(point, &root_reference_frame, &mut test) {
                    continue;
                }
                let point_in_viewport = match test.node_cache[&root_reference_frame] {
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
}

fn get_regions_for_clip_scroll_node(
    node: &ClipScrollNode,
    clip_store: &ClipStore
) -> Vec<HitTestRegion> {
    let clips = match node.node_type {
        NodeType::Clip{ ref handle, .. } => clip_store.get(handle).clips(),
        _ => return Vec::new(),
    };

    clips.iter().map(|ref source| {
        match source.0 {
            ClipSource::Rectangle(ref rect) => HitTestRegion::Rectangle(*rect),
            ClipSource::RoundedRectangle(ref rect, ref radii, ref mode) =>
                HitTestRegion::RoundedRectangle(*rect, *radii, *mode),
            ClipSource::Image(ref mask) => HitTestRegion::Rectangle(mask.rect),
            ClipSource::BorderCorner(_) =>
                unreachable!("Didn't expect to hit test against BorderCorner"),
        }
    }).collect()
}

pub struct HitTest {
    pipeline_id: Option<PipelineId>,
    point: WorldPoint,
    flags: HitTestFlags,
    node_cache: FastHashMap<ClipId, Option<LayerPoint>>,
    clip_chain_cache: Vec<Option<bool>>,
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

    pub fn get_from_clip_chain_cache(&mut self, index: ClipChainIndex) -> Option<bool> {
        if index.0 >= self.clip_chain_cache.len() {
            None
        } else {
            self.clip_chain_cache[index.0]
        }
    }

    pub fn set_in_clip_chain_cache(&mut self, index: ClipChainIndex, value: bool) {
        if index.0 >= self.clip_chain_cache.len() {
            self.clip_chain_cache.resize(index.0 + 1, None);
        }
        self.clip_chain_cache[index.0] = Some(value);
    }

    pub fn get_absolute_point(&self, hit_tester: &HitTester) -> WorldPoint {
        if !self.flags.contains(HitTestFlags::POINT_RELATIVE_TO_PIPELINE_VIEWPORT) {
            return self.point;
        }

        let point =  &LayerPoint::new(self.point.x, self.point.y);
        self.pipeline_id.and_then(|id| hit_tester.nodes.get(&ClipId::root_reference_frame(id)))
                   .map(|node| node.world_viewport_transform.transform_point2d(&point))
                   .unwrap_or_else(|| WorldPoint::new(self.point.x, self.point.y))
    }
}

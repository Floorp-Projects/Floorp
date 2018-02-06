/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{BorderRadius, ClipAndScrollInfo, ClipId, ClipMode, HitTestFlags, HitTestItem};
use api::{HitTestResult, ItemTag, LayerPoint, LayerPrimitiveInfo, LayerRect};
use api::{LayerToWorldTransform, LocalClip, PipelineId, WorldPoint};
use clip::{ClipSource, ClipStore, Contains, rounded_rectangle_contains_point};
use clip_scroll_node::{ClipScrollNode, NodeType};
use clip_scroll_tree::ClipScrollTree;
use internal_types::FastHashMap;

/// A copy of important clip scroll node data to use during hit testing. This a copy of
/// data from the ClipScrollTree that will persist as a new frame is under construction,
/// allowing hit tests consistent with the currently rendered frame.
pub struct HitTestClipScrollNode {
    /// This node's parent in the ClipScrollTree. This is used to ensure that we can
    /// travel up the tree of nodes.
    parent: Option<ClipId>,

    /// A particular point must be inside all of these regions to be considered clipped in
    /// for the purposes of a hit test.
    regions: Vec<HitTestRegion>,

    /// World transform for content transformed by this node.
    world_content_transform: LayerToWorldTransform,

    /// World viewport transform for content transformed by this node.
    world_viewport_transform: LayerToWorldTransform,

    /// Origin of the viewport of the node, used to calculate node-relative positions.
    node_origin: LayerPoint,
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
pub struct HitTestingRun(pub Vec<HitTestingItem>, pub ClipAndScrollInfo);

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

        for (id, node) in &clip_scroll_tree.nodes {
            self.nodes.insert(*id, HitTestClipScrollNode {
                parent: node.parent,
                regions: get_regions_for_clip_scroll_node(node, clip_store),
                world_content_transform: node.world_content_transform,
                world_viewport_transform: node.world_viewport_transform,
                node_origin: node.local_viewport_rect.origin,
            });
        }
    }

    pub fn is_point_clipped_in_for_node(
        &self,
        point: WorldPoint,
        node_id: &ClipId,
        cache: &mut FastHashMap<ClipId, Option<LayerPoint>>,
    ) -> bool {
        if let Some(point) = cache.get(node_id) {
            return point.is_some();
        }

        let node = self.nodes.get(node_id).unwrap();
        let parent_clipped_in = match node.parent {
            None => true, // This is the root node.
            Some(ref parent_id) => {
                self.is_point_clipped_in_for_node(point, parent_id, cache)
            }
        };

        if !parent_clipped_in {
            cache.insert(*node_id, None);
            return false;
        }

        let transform = node.world_viewport_transform;
        let transformed_point = match transform.inverse() {
            Some(inverted) => inverted.transform_point2d(&point),
            None => {
                cache.insert(*node_id, None);
                return false;
            }
        };

        let point_in_layer = transformed_point - node.node_origin.to_vector();
        for region in &node.regions {
            if !region.contains(&transformed_point) {
                cache.insert(*node_id, None);
                return false;
            }
        }

        cache.insert(*node_id, Some(point_in_layer));
        true
    }

    pub fn make_node_relative_point_absolute(
        &self,
        pipeline_id: Option<PipelineId>,
        point: &LayerPoint
    ) -> WorldPoint {
        pipeline_id.and_then(|id| self.nodes.get(&ClipId::root_reference_frame(id)))
                   .map(|node| node.world_viewport_transform.transform_point2d(point))
                   .unwrap_or_else(|| WorldPoint::new(point.x, point.y))

    }

    pub fn hit_test(
        &self,
        pipeline_id: Option<PipelineId>,
        point: WorldPoint,
        flags: HitTestFlags
    ) -> HitTestResult {
        let point = if flags.contains(HitTestFlags::POINT_RELATIVE_TO_PIPELINE_VIEWPORT) {
            self.make_node_relative_point_absolute(pipeline_id, &LayerPoint::new(point.x, point.y))
        } else {
            point
        };

        let mut node_cache = FastHashMap::default();
        let mut result = HitTestResult::default();
        for &HitTestingRun(ref items, ref clip_and_scroll) in self.runs.iter().rev() {
            let scroll_node = &self.nodes[&clip_and_scroll.scroll_node_id];
            match (pipeline_id, clip_and_scroll.scroll_node_id.pipeline_id()) {
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

                let clip_id = &clip_and_scroll.clip_node_id();
                if !clipped_in {
                    clipped_in = self.is_point_clipped_in_for_node(point, clip_id, &mut node_cache);
                    if !clipped_in {
                        break;
                    }
                }

                let point_in_viewport = node_cache
                    .get(&ClipId::root_reference_frame(clip_id.pipeline_id()))
                    .expect("Hittest target's root reference frame not hit.")
                    .expect("Hittest target's root reference frame not hit.");

                result.items.push(HitTestItem {
                    pipeline: clip_and_scroll.clip_node_id().pipeline_id(),
                    tag: item.tag,
                    point_in_viewport,
                    point_relative_to_item: point_in_layer - item.rect.origin.to_vector(),
                });
                if !flags.contains(HitTestFlags::FIND_ALL) {
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
        NodeType::Clip(ref handle) => clip_store.get(handle).clips(),
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

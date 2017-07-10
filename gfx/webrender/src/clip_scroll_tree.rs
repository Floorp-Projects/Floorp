/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clip_scroll_node::{ClipScrollNode, NodeType, ScrollingState};
use fnv::FnvHasher;
use print_tree::PrintTree;
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use api::{ClipId, LayerPoint, LayerRect, LayerToScrollTransform};
use api::{LayerToWorldTransform, PipelineId, ScrollClamping, ScrollEventPhase};
use api::{LayerVector2D, ScrollLayerState, ScrollLocation, WorldPoint};

pub type ScrollStates = HashMap<ClipId, ScrollingState, BuildHasherDefault<FnvHasher>>;

pub struct ClipScrollTree {
    pub nodes: HashMap<ClipId, ClipScrollNode, BuildHasherDefault<FnvHasher>>,
    pub pending_scroll_offsets: HashMap<ClipId, (LayerPoint, ScrollClamping)>,

    /// The ClipId of the currently scrolling node. Used to allow the same
    /// node to scroll even if a touch operation leaves the boundaries of that node.
    pub currently_scrolling_node_id: Option<ClipId>,

    /// The current frame id, used for giving a unique id to all new dynamically
    /// added frames and clips. The ClipScrollTree increments this by one every
    /// time a new dynamic frame is created.
    current_new_node_item: u64,

    /// The root reference frame, which is the true root of the ClipScrollTree. Initially
    /// this ID is not valid, which is indicated by ```node``` being empty.
    pub root_reference_frame_id: ClipId,

    /// The root scroll node which is the first child of the root reference frame.
    /// Initially this ID is not valid, which is indicated by ```nodes``` being empty.
    pub topmost_scrolling_node_id: ClipId,

    /// A set of pipelines which should be discarded the next time this
    /// tree is drained.
    pub pipelines_to_discard: HashSet<PipelineId>,
}

impl ClipScrollTree {
    pub fn new() -> ClipScrollTree {
        let dummy_pipeline = PipelineId(0, 0);
        ClipScrollTree {
            nodes: HashMap::default(),
            pending_scroll_offsets: HashMap::new(),
            currently_scrolling_node_id: None,
            root_reference_frame_id: ClipId::root_reference_frame(dummy_pipeline),
            topmost_scrolling_node_id: ClipId::root_scroll_node(dummy_pipeline),
            current_new_node_item: 1,
            pipelines_to_discard: HashSet::new(),
        }
    }

    pub fn root_reference_frame_id(&self) -> ClipId {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.nodes.is_empty());
        debug_assert!(self.nodes.contains_key(&self.root_reference_frame_id));
        self.root_reference_frame_id
    }

    pub fn topmost_scrolling_node_id(&self) -> ClipId {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.nodes.is_empty());
        debug_assert!(self.nodes.contains_key(&self.topmost_scrolling_node_id));
        self.topmost_scrolling_node_id
    }

    pub fn collect_nodes_bouncing_back(&self)
                                       -> HashSet<ClipId, BuildHasherDefault<FnvHasher>> {
        let mut nodes_bouncing_back = HashSet::default();
        for (clip_id, node) in self.nodes.iter() {
            if let NodeType::ScrollFrame(ref scrolling) = node.node_type {
                if scrolling.bouncing_back {
                    nodes_bouncing_back.insert(*clip_id);
                }
            }
        }
        nodes_bouncing_back
    }

    fn find_scrolling_node_at_point_in_node(&self,
                                            cursor: &WorldPoint,
                                            clip_id: ClipId)
                                            -> Option<ClipId> {
        self.nodes.get(&clip_id).and_then(|node| {
            for child_layer_id in node.children.iter().rev() {
                if let Some(layer_id) =
                   self.find_scrolling_node_at_point_in_node(cursor, *child_layer_id) {
                    return Some(layer_id);
                }
            }

            match node.node_type {
                NodeType::ScrollFrame(..) => {},
                _ => return None,
            }

            if node.ray_intersects_node(cursor) {
                Some(clip_id)
            } else {
                None
            }
        })
    }

    pub fn find_scrolling_node_at_point(&self, cursor: &WorldPoint) -> ClipId {
        self.find_scrolling_node_at_point_in_node(cursor, self.root_reference_frame_id())
            .unwrap_or(self.topmost_scrolling_node_id())
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollLayerState> {
        let mut result = vec![];
        for (id, node) in self.nodes.iter() {
            if let NodeType::ScrollFrame(scrolling) = node.node_type {
                result.push(ScrollLayerState { id: *id, scroll_offset: scrolling.offset })
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        self.current_new_node_item = 1;

        let mut scroll_states = HashMap::default();
        for (layer_id, old_node) in &mut self.nodes.drain() {
            if self.pipelines_to_discard.contains(&layer_id.pipeline_id()) {
                continue;
            }

            if let NodeType::ScrollFrame(scrolling) = old_node.node_type {
                scroll_states.insert(layer_id, scrolling);
            }
        }

        self.pipelines_to_discard.clear();
        scroll_states
    }

    pub fn scroll_node(&mut self, origin: LayerPoint, id: ClipId, clamp: ScrollClamping) -> bool {
        if self.nodes.is_empty() {
            self.pending_scroll_offsets.insert(id, (origin, clamp));
            return false;
        }

        if let Some(node) = self.nodes.get_mut(&id) {
            return node.set_scroll_origin(&origin, clamp);
        }

        self.pending_scroll_offsets.insert(id, (origin, clamp));
        false
    }

    pub fn scroll(&mut self,
                  scroll_location: ScrollLocation,
                  cursor: WorldPoint,
                  phase: ScrollEventPhase)
                  -> bool {
        if self.nodes.is_empty() {
            return false;
        }

        let clip_id = match (
            phase,
            self.find_scrolling_node_at_point(&cursor),
            self.currently_scrolling_node_id) {
            (ScrollEventPhase::Start, scroll_node_at_point_id, _) => {
                self.currently_scrolling_node_id = Some(scroll_node_at_point_id);
                scroll_node_at_point_id
            },
            (_, scroll_node_at_point_id, Some(cached_clip_id)) => {
                let clip_id = match self.nodes.get(&cached_clip_id) {
                    Some(_) => cached_clip_id,
                    None => {
                        self.currently_scrolling_node_id = Some(scroll_node_at_point_id);
                        scroll_node_at_point_id
                    },
                };
                clip_id
            },
            (_, _, None) => return false,
        };

        let topmost_scrolling_node_id = self.topmost_scrolling_node_id();
        let non_root_overscroll = if clip_id != topmost_scrolling_node_id {
            self.nodes.get(&clip_id).unwrap().is_overscrolling()
        } else {
            false
        };

        let mut switch_node = false;
        if let Some(node) = self.nodes.get_mut(&clip_id) {
            if let NodeType::ScrollFrame(ref mut scrolling) = node.node_type {
                match phase {
                    ScrollEventPhase::Start => {
                        // if this is a new gesture, we do not switch node,
                        // however we do save the state of non_root_overscroll,
                        // for use in the subsequent Move phase.
                        scrolling.should_handoff_scroll = non_root_overscroll;
                    },
                    ScrollEventPhase::Move(_) => {
                        // Switch node if movement originated in a new gesture,
                        // from a non root node in overscroll.
                        switch_node = scrolling.should_handoff_scroll && non_root_overscroll
                    },
                    ScrollEventPhase::End => {
                        // clean-up when gesture ends.
                        scrolling.should_handoff_scroll = false;
                    }
                }
            }
        }

        let clip_id = if switch_node {
            topmost_scrolling_node_id
        } else {
            clip_id
        };

        self.nodes.get_mut(&clip_id).unwrap().scroll(scroll_location, phase)
    }

    pub fn update_all_node_transforms(&mut self, pan: LayerPoint) {
        if self.nodes.is_empty() {
            return;
        }

        let root_reference_frame_id = self.root_reference_frame_id();
        let root_viewport = self.nodes[&root_reference_frame_id].local_clip_rect;
        self.update_node_transform(root_reference_frame_id,
                                   &LayerToWorldTransform::create_translation(pan.x, pan.y, 0.0),
                                   &root_viewport,
                                   LayerVector2D::zero(),
                                   LayerVector2D::zero());
    }

    fn update_node_transform(&mut self,
                             layer_id: ClipId,
                             parent_reference_frame_transform: &LayerToWorldTransform,
                             parent_viewport_rect: &LayerRect,
                             parent_scroll_offset: LayerVector2D,
                             parent_accumulated_scroll_offset: LayerVector2D) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let (reference_frame_transform,
             combined_local_viewport_rect,
             scroll_offset,
             accumulated_scroll_offset,
             node_children) = {

            let mut node = match self.nodes.get_mut(&layer_id) {
                Some(node) => node,
                None => return,
            };
            node.update_transform(parent_reference_frame_transform,
                                  parent_viewport_rect,
                                  parent_scroll_offset,
                                  parent_accumulated_scroll_offset);

            // The transformation we are passing is the transformation of the parent
            // reference frame and the offset is the accumulated offset of all the nodes
            // between us and the parent reference frame. If we are a reference frame,
            // we need to reset both these values.
            let (reference_frame_transform, scroll_offset, accumulated_scroll_offset) = match node.node_type {
                NodeType::ReferenceFrame(..) =>
                    (node.world_viewport_transform,
                     LayerVector2D::zero(),
                     LayerVector2D::zero()),
                NodeType::Clip(..) =>
                    (*parent_reference_frame_transform,
                     LayerVector2D::zero(),
                     parent_accumulated_scroll_offset),
                NodeType::ScrollFrame(ref scrolling) =>
                    (*parent_reference_frame_transform,
                     scrolling.offset,
                     scrolling.offset + parent_accumulated_scroll_offset),
            };

            (reference_frame_transform,
             node.combined_local_viewport_rect,
             scroll_offset,
             accumulated_scroll_offset,
             node.children.clone())
        };

        for child_layer_id in node_children {
            self.update_node_transform(child_layer_id,
                                       &reference_frame_transform,
                                       &combined_local_viewport_rect,
                                       scroll_offset,
                                       accumulated_scroll_offset);
        }
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        for (_, node) in &mut self.nodes {
            node.tick_scrolling_bounce_animation()
        }
    }

    pub fn finalize_and_apply_pending_scroll_offsets(&mut self, old_states: ScrollStates) {
        // TODO(gw): These are all independent - can be run through thread pool if it shows up
        // in the profile!
        for (clip_id, node) in &mut self.nodes {
            let scrolling_state = match old_states.get(clip_id) {
                Some(old_scrolling_state) => *old_scrolling_state,
                None => ScrollingState::new(),
            };

            node.finalize(&scrolling_state);

            if let Some((pending_offset, clamping)) = self.pending_scroll_offsets.remove(clip_id) {
                node.set_scroll_origin(&pending_offset, clamping);
            }
        }
    }

    pub fn generate_new_clip_id(&mut self, pipeline_id: PipelineId) -> ClipId {
        let new_id = ClipId::DynamicallyAddedNode(self.current_new_node_item, pipeline_id);
        self.current_new_node_item += 1;
        new_id
    }

    pub fn add_reference_frame(&mut self,
                               rect: &LayerRect,
                               transform: &LayerToScrollTransform,
                               pipeline_id: PipelineId,
                               parent_id: Option<ClipId>)
                               -> ClipId {
        let reference_frame_id = self.generate_new_clip_id(pipeline_id);
        let node = ClipScrollNode::new_reference_frame(parent_id,
                                                       rect,
                                                       rect.size,
                                                       transform,
                                                       pipeline_id);
        self.add_node(node, reference_frame_id);
        reference_frame_id
    }

    pub fn add_node(&mut self, node: ClipScrollNode, id: ClipId) {
        // When the parent node is None this means we are adding the root.
        match node.parent {
            Some(parent_id) => self.nodes.get_mut(&parent_id).unwrap().add_child(id),
            None => self.root_reference_frame_id = id,
        }

        debug_assert!(!self.nodes.contains_key(&id));
        self.nodes.insert(id, node);
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.pipelines_to_discard.insert(pipeline_id);

        match self.currently_scrolling_node_id {
            Some(id) if id.pipeline_id() == pipeline_id => self.currently_scrolling_node_id = None,
            _ => {}
        }
    }

    fn print_node(&self, id: &ClipId, pt: &mut PrintTree) {
        let node = self.nodes.get(id).unwrap();

        match node.node_type {
            NodeType::Clip(ref info) => {
                pt.new_level("Clip".to_owned());
                pt.add_item(format!("screen_bounding_rect: {:?}", info.screen_bounding_rect));
                pt.add_item(format!("screen_inner_rect: {:?}", info.screen_inner_rect));

                pt.new_level(format!("Clip Sources [{}]", info.clip_sources.len()));
                for source in &info.clip_sources {
                    pt.add_item(format!("{:?}", source));
                }
                pt.end_level();
            }
            NodeType::ReferenceFrame(ref transform) => {
                pt.new_level(format!("ReferenceFrame {:?}", transform));
            }
            NodeType::ScrollFrame(scrolling_info) => {
                pt.new_level(format!("ScrollFrame"));
                pt.add_item(format!("scroll.offset: {:?}", scrolling_info.offset));
            }
        }

        pt.add_item(format!("content_size: {:?}", node.content_size));
        pt.add_item(format!("local_viewport_rect: {:?}", node.local_viewport_rect));
        pt.add_item(format!("local_clip_rect: {:?}", node.local_clip_rect));
        pt.add_item(format!("combined_local_viewport_rect: {:?}", node.combined_local_viewport_rect));
        pt.add_item(format!("world_viewport_transform: {:?}", node.world_viewport_transform));
        pt.add_item(format!("world_content_transform: {:?}", node.world_content_transform));

        for child_id in &node.children {
            self.print_node(child_id, pt);
        }

        pt.end_level();
    }

    #[allow(dead_code)]
    pub fn print(&self) {
        if !self.nodes.is_empty() {
            let mut pt = PrintTree::new("clip_scroll tree");
            self.print_node(&self.root_reference_frame_id, &mut pt);
        }
    }
}


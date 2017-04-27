/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clip_scroll_node::{ClipScrollNode, NodeType, ScrollingState};
use fnv::FnvHasher;
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use webrender_traits::{ClipId, LayerPoint, LayerRect, LayerToScrollTransform};
use webrender_traits::{LayerToWorldTransform, PipelineId, ScrollEventPhase, ScrollLayerRect};
use webrender_traits::{ScrollLayerState, ScrollLocation, WorldPoint, as_scroll_parent_rect};

pub type ScrollStates = HashMap<ClipId, ScrollingState, BuildHasherDefault<FnvHasher>>;

pub struct ClipScrollTree {
    pub nodes: HashMap<ClipId, ClipScrollNode, BuildHasherDefault<FnvHasher>>,
    pub pending_scroll_offsets: HashMap<ClipId, LayerPoint>,

    /// The ClipId of the currently scrolling node. Used to allow the same
    /// node to scroll even if a touch operation leaves the boundaries of that node.
    pub currently_scrolling_node_id: Option<ClipId>,

    /// The current reference frame id, used for giving a unique id to all new
    /// reference frames. The ClipScrollTree increments this by one every time a
    /// reference frame is created.
    current_reference_frame_id: u64,

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
            current_reference_frame_id: 0,
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
            if node.scrolling.bouncing_back {
                nodes_bouncing_back.insert(*clip_id);
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

            if clip_id.is_reference_frame() {
                return None;
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
            match node.node_type {
                NodeType::Clip(_) => result.push(
                    ScrollLayerState { id: *id, scroll_offset: node.scrolling.offset }),
                _ => {},
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        self.current_reference_frame_id = 1;

        let mut scroll_states = HashMap::default();
        for (layer_id, old_node) in &mut self.nodes.drain() {
            if !self.pipelines_to_discard.contains(&layer_id.pipeline_id()) {
                scroll_states.insert(layer_id, old_node.scrolling);
            }
        }

        self.pipelines_to_discard.clear();
        scroll_states
    }

    pub fn scroll_nodes(&mut self, origin: LayerPoint, id: ClipId) -> bool {
        if id.is_reference_frame() {
            warn!("Tried to scroll a reference frame.");
            return false;
        }

        if self.nodes.is_empty() {
            self.pending_scroll_offsets.insert(id, origin);
            return false;
        }

        let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));
        if let Some(node) = self.nodes.get_mut(&id) {
            return node.set_scroll_origin(&origin);
        }

        self.pending_scroll_offsets.insert(id, origin);
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
            // true if the current node is overscrolling,
            // and it is not the root scroll node.
            let child_node = self.nodes.get(&clip_id).unwrap();
            let overscroll_amount = child_node.overscroll_amount();
            overscroll_amount.width != 0.0 || overscroll_amount.height != 0.0
        } else {
            false
        };

        let switch_node = match phase {
            ScrollEventPhase::Start => {
                // if this is a new gesture, we do not switch node,
                // however we do save the state of non_root_overscroll,
                // for use in the subsequent Move phase.
                let mut current_node = self.nodes.get_mut(&clip_id).unwrap();
                current_node.scrolling.should_handoff_scroll = non_root_overscroll;
                false
            },
            ScrollEventPhase::Move(_) => {
                // Switch node if movement originated in a new gesture,
                // from a non root node in overscroll.
                let current_node = self.nodes.get_mut(&clip_id).unwrap();
                current_node.scrolling.should_handoff_scroll && non_root_overscroll
            },
            ScrollEventPhase::End => {
                // clean-up when gesture ends.
                let mut current_node = self.nodes.get_mut(&clip_id).unwrap();
                current_node.scrolling.should_handoff_scroll = false;
                false
            },
        };

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
                                   &as_scroll_parent_rect(&root_viewport),
                                   LayerPoint::zero(),
                                   LayerPoint::zero());
    }

    fn update_node_transform(&mut self,
                             layer_id: ClipId,
                             parent_reference_frame_transform: &LayerToWorldTransform,
                             parent_viewport_rect: &ScrollLayerRect,
                             parent_scroll_offset: LayerPoint,
                             parent_accumulated_scroll_offset: LayerPoint) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let (reference_frame_transform,
             viewport_rect,
             scroll_offset,
             accumulated_scroll_offset,
             node_children) = {
            match self.nodes.get_mut(&layer_id) {
                Some(node) => {
                    node.update_transform(parent_reference_frame_transform,
                                          parent_viewport_rect,
                                          parent_scroll_offset,
                                          parent_accumulated_scroll_offset);

                    // The transformation we are passing is the transformation of the parent
                    // reference frame and the offset is the accumulated offset of all the nodes
                    // between us and the parent reference frame. If we are a reference frame,
                    // we need to reset both these values.
                    let (transform, offset, accumulated_scroll_offset) = match node.node_type {
                        NodeType::ReferenceFrame(..) =>
                            (node.world_viewport_transform, LayerPoint::zero(), LayerPoint::zero()),
                        NodeType::Clip(_) => {
                            (*parent_reference_frame_transform,
                             node.scrolling.offset,
                             node.scrolling.offset + parent_accumulated_scroll_offset)
                        }
                    };

                    (transform,
                     as_scroll_parent_rect(&node.combined_local_viewport_rect),
                     offset,
                     accumulated_scroll_offset,
                     node.children.clone())
                }
                None => return,
            }
        };

        for child_layer_id in node_children {
            self.update_node_transform(child_layer_id,
                                       &reference_frame_transform,
                                       &viewport_rect,
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

            if let Some(pending_offset) = self.pending_scroll_offsets.remove(clip_id) {
                node.set_scroll_origin(&pending_offset);
            }
        }

    }

    pub fn add_reference_frame(&mut self,
                               rect: &LayerRect,
                               transform: &LayerToScrollTransform,
                               pipeline_id: PipelineId,
                               parent_id: Option<ClipId>)
                               -> ClipId {

        let reference_frame_id =
            ClipId::ReferenceFrame(self.current_reference_frame_id, pipeline_id);
        self.current_reference_frame_id += 1;

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
}


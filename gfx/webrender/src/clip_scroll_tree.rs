/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use clip_scroll_node::{ClipScrollNode, NodeType, ScrollingState};
use fnv::FnvHasher;
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform};
use webrender_traits::{LayerToWorldTransform, PipelineId, ScrollEventPhase, ScrollLayerId};
use webrender_traits::{ScrollLayerInfo, ScrollLayerRect, ScrollLayerState, ScrollLocation};
use webrender_traits::{ServoScrollRootId, WorldPoint, as_scroll_parent_rect};

pub type ScrollStates = HashMap<ScrollLayerId, ScrollingState, BuildHasherDefault<FnvHasher>>;

pub struct ClipScrollTree {
    pub nodes: HashMap<ScrollLayerId, ClipScrollNode, BuildHasherDefault<FnvHasher>>,
    pub pending_scroll_offsets: HashMap<(PipelineId, ServoScrollRootId), LayerPoint>,

    /// The ScrollLayerId of the currently scrolling node. Used to allow the same
    /// node to scroll even if a touch operation leaves the boundaries of that node.
    pub current_scroll_layer_id: Option<ScrollLayerId>,

    /// The current reference frame id, used for giving a unique id to all new
    /// reference frames. The ClipScrollTree increments this by one every time a
    /// reference frame is created.
    current_reference_frame_id: usize,

    /// The root reference frame, which is the true root of the ClipScrollTree. Initially
    /// this ID is not valid, which is indicated by ```node``` being empty.
    pub root_reference_frame_id: ScrollLayerId,

    /// The root scroll node which is the first child of the root reference frame.
    /// Initially this ID is not valid, which is indicated by ```nodes``` being empty.
    pub topmost_scroll_layer_id: ScrollLayerId,

    /// A set of pipelines which should be discarded the next time this
    /// tree is drained.
    pub pipelines_to_discard: HashSet<PipelineId>,
}

impl ClipScrollTree {
    pub fn new() -> ClipScrollTree {
        let dummy_pipeline = PipelineId(0, 0);
        ClipScrollTree {
            nodes: HashMap::with_hasher(Default::default()),
            pending_scroll_offsets: HashMap::new(),
            current_scroll_layer_id: None,
            root_reference_frame_id: ScrollLayerId::root_reference_frame(dummy_pipeline),
            topmost_scroll_layer_id: ScrollLayerId::root_scroll_layer(dummy_pipeline),
            current_reference_frame_id: 1,
            pipelines_to_discard: HashSet::new(),
        }
    }

    pub fn root_reference_frame_id(&self) -> ScrollLayerId {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.nodes.is_empty());
        debug_assert!(self.nodes.contains_key(&self.root_reference_frame_id));
        self.root_reference_frame_id
    }

    pub fn topmost_scroll_layer_id(&self) -> ScrollLayerId {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.nodes.is_empty());
        debug_assert!(self.nodes.contains_key(&self.topmost_scroll_layer_id));
        self.topmost_scroll_layer_id
    }

    pub fn establish_root(&mut self,
                          pipeline_id: PipelineId,
                          viewport_size: &LayerSize,
                          viewport_offset: LayerPoint,
                          clip_size: LayerSize,
                          content_size: &LayerSize) {
        debug_assert!(self.nodes.is_empty());

        let transform = LayerToScrollTransform::create_translation(viewport_offset.x, viewport_offset.y, 0.0);
        let viewport = LayerRect::new(LayerPoint::zero(), *viewport_size);
        let clip = LayerRect::new(LayerPoint::new(-viewport_offset.x, -viewport_offset.y),
                                  LayerSize::new(clip_size.width, clip_size.height));
        let root_reference_frame_id = ScrollLayerId::root_reference_frame(pipeline_id);
        self.root_reference_frame_id = root_reference_frame_id;
        let reference_frame = ClipScrollNode::new_reference_frame(&viewport,
                                                                  &clip,
                                                                  viewport.size,
                                                                  &transform,
                                                                  pipeline_id);
        self.nodes.insert(self.root_reference_frame_id, reference_frame);

        let scroll_node = ClipScrollNode::new(&viewport, &clip, *content_size, pipeline_id);
        let topmost_scroll_layer_id = ScrollLayerId::root_scroll_layer(pipeline_id);
        self.topmost_scroll_layer_id = topmost_scroll_layer_id;
        self.add_node(scroll_node, topmost_scroll_layer_id, root_reference_frame_id);
    }

    pub fn collect_nodes_bouncing_back(&self)
                                       -> HashSet<ScrollLayerId, BuildHasherDefault<FnvHasher>> {
        let mut nodes_bouncing_back = HashSet::with_hasher(Default::default());
        for (scroll_layer_id, node) in self.nodes.iter() {
            if node.scrolling.bouncing_back {
                nodes_bouncing_back.insert(*scroll_layer_id);
            }
        }
        nodes_bouncing_back
    }

    fn find_scrolling_node_at_point_in_node(&self,
                                            cursor: &WorldPoint,
                                            scroll_layer_id: ScrollLayerId)
                                            -> Option<ScrollLayerId> {
        self.nodes.get(&scroll_layer_id).and_then(|node| {
            for child_layer_id in node.children.iter().rev() {
            if let Some(layer_id) =
                self.find_scrolling_node_at_point_in_node(cursor, *child_layer_id) {
                    return Some(layer_id);
                }
            }

            if let ScrollLayerInfo::ReferenceFrame(_) = scroll_layer_id.info {
                return None;
            }

            if node.ray_intersects_node(cursor) {
                Some(scroll_layer_id)
            } else {
                None
            }
        })
    }

    pub fn find_scrolling_node_at_point(&self, cursor: &WorldPoint) -> ScrollLayerId {
        self.find_scrolling_node_at_point_in_node(cursor, self.root_reference_frame_id())
            .unwrap_or(self.topmost_scroll_layer_id())
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollLayerState> {
        let mut result = vec![];
        for (scroll_layer_id, scroll_node) in self.nodes.iter() {
            match scroll_layer_id.info {
                ScrollLayerInfo::Scrollable(_, servo_scroll_root_id) => {
                    result.push(ScrollLayerState {
                        pipeline_id: scroll_node.pipeline_id,
                        scroll_root_id: servo_scroll_root_id,
                        scroll_offset: scroll_node.scrolling.offset,
                    })
                }
                ScrollLayerInfo::ReferenceFrame(..) => {}
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        self.current_reference_frame_id = 1;

        let mut scroll_states = HashMap::with_hasher(Default::default());
        for (layer_id, old_node) in &mut self.nodes.drain() {
            if !self.pipelines_to_discard.contains(&layer_id.pipeline_id) {
                scroll_states.insert(layer_id, old_node.scrolling);
            }
        }

        self.pipelines_to_discard.clear();
        scroll_states
    }

    pub fn scroll_nodes(&mut self,
                        origin: LayerPoint,
                        pipeline_id: PipelineId,
                        scroll_root_id: ServoScrollRootId)
                        -> bool {
        if self.nodes.is_empty() {
            self.pending_scroll_offsets.insert((pipeline_id, scroll_root_id), origin);
            return false;
        }

        let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));

        let mut scrolled_a_node = false;
        let mut found_node = false;
        for (layer_id, node) in self.nodes.iter_mut() {
            if layer_id.pipeline_id != pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::ReferenceFrame(..) => continue,
                ScrollLayerInfo::Scrollable(..) => {}
            }

            found_node = true;
            scrolled_a_node |= node.set_scroll_origin(&origin);
        }

        if !found_node {
            self.pending_scroll_offsets.insert((pipeline_id, scroll_root_id), origin);
        }

        scrolled_a_node
    }

    pub fn scroll(&mut self,
                  scroll_location: ScrollLocation,
                  cursor: WorldPoint,
                  phase: ScrollEventPhase)
                  -> bool {
        if self.nodes.is_empty() {
            return false;
        }

        let scroll_layer_id = match (
            phase,
            self.find_scrolling_node_at_point(&cursor),
            self.current_scroll_layer_id) {
            (ScrollEventPhase::Start, scroll_node_at_point_id, _) => {
                self.current_scroll_layer_id = Some(scroll_node_at_point_id);
                scroll_node_at_point_id
            },
            (_, scroll_node_at_point_id, Some(cached_scroll_layer_id)) => {
                let scroll_layer_id = match self.nodes.get(&cached_scroll_layer_id) {
                    Some(_) => cached_scroll_layer_id,
                    None => {
                        self.current_scroll_layer_id = Some(scroll_node_at_point_id);
                        scroll_node_at_point_id
                    },
                };
                scroll_layer_id
            },
            (_, _, None) => return false,
        };

        let topmost_scroll_layer_id = self.topmost_scroll_layer_id();
        let non_root_overscroll = if scroll_layer_id != topmost_scroll_layer_id {
            // true if the current node is overscrolling,
            // and it is not the root scroll node.
            let child_node = self.nodes.get(&scroll_layer_id).unwrap();
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
                let mut current_node = self.nodes.get_mut(&scroll_layer_id).unwrap();
                current_node.scrolling.should_handoff_scroll = non_root_overscroll;
                false
            },
            ScrollEventPhase::Move(_) => {
                // Switch node if movement originated in a new gesture,
                // from a non root node in overscroll.
                let current_node = self.nodes.get_mut(&scroll_layer_id).unwrap();
                current_node.scrolling.should_handoff_scroll && non_root_overscroll
            },
            ScrollEventPhase::End => {
                // clean-up when gesture ends.
                let mut current_node = self.nodes.get_mut(&scroll_layer_id).unwrap();
                current_node.scrolling.should_handoff_scroll = false;
                false
            },
        };

        let scroll_node_info = if switch_node {
            topmost_scroll_layer_id.info
        } else {
            scroll_layer_id.info
        };

        let scroll_root_id = match scroll_node_info {
             ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
             _ => unreachable!("Tried to scroll a reference frame."),

        };

        let mut scrolled_a_node = false;
        for (layer_id, node) in self.nodes.iter_mut() {
            if layer_id.pipeline_id != scroll_layer_id.pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::ReferenceFrame(..) => continue,
                _ => {}
            }

            let scrolled_this_node = node.scroll(scroll_location, phase);
            scrolled_a_node = scrolled_a_node || scrolled_this_node;
        }
        scrolled_a_node
    }

    pub fn update_all_node_transforms(&mut self) {
        if self.nodes.is_empty() {
            return;
        }

        let root_reference_frame_id = self.root_reference_frame_id();
        let root_viewport = self.nodes[&root_reference_frame_id].local_clip_rect;
        self.update_node_transform(root_reference_frame_id,
                                   &LayerToWorldTransform::identity(),
                                   &as_scroll_parent_rect(&root_viewport),
                                   LayerPoint::zero());
    }

    fn update_node_transform(&mut self,
                             layer_id: ScrollLayerId,
                             parent_reference_frame_transform: &LayerToWorldTransform,
                             parent_viewport_rect: &ScrollLayerRect,
                             parent_accumulated_scroll_offset: LayerPoint) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let (reference_frame_transform, viewport_rect, accumulated_scroll_offset, node_children) = {
            match self.nodes.get_mut(&layer_id) {
                Some(node) => {
                    node.update_transform(parent_reference_frame_transform,
                                          parent_viewport_rect,
                                          parent_accumulated_scroll_offset);

                    // The transformation we are passing is the transformation of the parent
                    // reference frame and the offset is the accumulated offset of all the nodes
                    // between us and the parent reference frame. If we are a reference frame,
                    // we need to reset both these values.
                    let (transform, offset) = match node.node_type {
                        NodeType::ReferenceFrame(..) =>
                            (node.world_viewport_transform, LayerPoint::zero()),
                        NodeType::ClipRect => {
                            (*parent_reference_frame_transform,
                             parent_accumulated_scroll_offset + node.scrolling.offset)
                        }
                    };

                    (transform,
                     as_scroll_parent_rect(&node.combined_local_viewport_rect),
                     offset,
                     node.children.clone())
                }
                None => return,
            }
        };

        for child_layer_id in node_children {
            self.update_node_transform(child_layer_id,
                                       &reference_frame_transform,
                                       &viewport_rect,
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
        for (scroll_layer_id, node) in &mut self.nodes {
            let scrolling_state = match old_states.get(&scroll_layer_id) {
                Some(old_scrolling_state) => *old_scrolling_state,
                None => ScrollingState::new(),
            };

            node.finalize(&scrolling_state);

            let scroll_root_id = match scroll_layer_id.info {
                ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
                _ => continue,
            };


            let pipeline_id = scroll_layer_id.pipeline_id;
            if let Some(pending_offset) =
                self.pending_scroll_offsets.remove(&(pipeline_id, scroll_root_id)) {
                node.set_scroll_origin(&pending_offset);
            }
        }

    }

    pub fn add_reference_frame(&mut self,
                               rect: LayerRect,
                               transform: LayerToScrollTransform,
                               pipeline_id: PipelineId,
                               parent_id: ScrollLayerId) -> ScrollLayerId {
        let reference_frame_id = ScrollLayerId {
            pipeline_id: pipeline_id,
            info: ScrollLayerInfo::ReferenceFrame(self.current_reference_frame_id),
        };
        self.current_reference_frame_id += 1;

        let node = ClipScrollNode::new_reference_frame(&rect, &rect, rect.size, &transform, pipeline_id);
        self.add_node(node, reference_frame_id, parent_id);
        reference_frame_id
    }

    pub fn add_node(&mut self, node: ClipScrollNode, id: ScrollLayerId, parent_id: ScrollLayerId) {
        debug_assert!(!self.nodes.contains_key(&id));
        self.nodes.insert(id, node);

        debug_assert!(parent_id != id);
        self.nodes.get_mut(&parent_id).unwrap().add_child(id);
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.pipelines_to_discard.insert(pipeline_id);

        match self.current_scroll_layer_id {
            Some(id) if id.pipeline_id == pipeline_id => self.current_scroll_layer_id = None,
            _ => {}
        }
    }
}


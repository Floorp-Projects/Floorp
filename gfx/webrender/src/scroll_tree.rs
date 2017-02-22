/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fnv::FnvHasher;
use layer::{Layer, ScrollingState};
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use webrender_traits::{LayerPoint, LayerRect, LayerSize, LayerToScrollTransform, PipelineId};
use webrender_traits::{ScrollEventPhase, ScrollLayerId, ScrollLayerInfo, ScrollLayerPixel};
use webrender_traits::{ScrollLayerRect, ScrollLayerState, ScrollLocation, ScrollToWorldTransform};
use webrender_traits::{ServoScrollRootId, WorldPoint, as_scroll_parent_rect};

pub type ScrollStates = HashMap<ScrollLayerId, ScrollingState, BuildHasherDefault<FnvHasher>>;

pub struct ScrollTree {
    pub layers: HashMap<ScrollLayerId, Layer, BuildHasherDefault<FnvHasher>>,
    pub pending_scroll_offsets: HashMap<(PipelineId, ServoScrollRootId), LayerPoint>,

    /// The ScrollLayerId of the currently scrolling layer. Used to allow the same
    /// layer to scroll even if a touch operation leaves the boundaries of that layer.
    pub current_scroll_layer_id: Option<ScrollLayerId>,

    /// The current reference frame id, used for giving a unique id to all new
    /// reference frames. The ScrollTree increments this by one every time a
    /// reference frame is created.
    current_reference_frame_id: usize,

    /// The root reference frame, which is the true root of the ScrollTree. Initially
    /// this ID is not valid, which is indicated by ```layers``` being empty.
    pub root_reference_frame_id: ScrollLayerId,

    /// The root scroll layer, which is the first child of the root reference frame.
    /// Initially this ID is not valid, which is indicated by ```layers``` being empty.
    pub topmost_scroll_layer_id: ScrollLayerId,

    /// A set of pipelines which should be discarded the next time this
    /// tree is drained.
    pub pipelines_to_discard: HashSet<PipelineId>,
}

impl ScrollTree {
    pub fn new() -> ScrollTree {
        let dummy_pipeline = PipelineId(0, 0);
        ScrollTree {
            layers: HashMap::with_hasher(Default::default()),
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
        debug_assert!(!self.layers.is_empty());
        debug_assert!(self.layers.contains_key(&self.root_reference_frame_id));
        self.root_reference_frame_id
    }

    pub fn topmost_scroll_layer_id(&self) -> ScrollLayerId {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.layers.is_empty());
        debug_assert!(self.layers.contains_key(&self.topmost_scroll_layer_id));
        self.topmost_scroll_layer_id
    }

    pub fn establish_root(&mut self,
                          pipeline_id: PipelineId,
                          viewport_size: &LayerSize,
                          content_size: &LayerSize) {
        debug_assert!(self.layers.is_empty());

        let identity = LayerToScrollTransform::identity();
        let viewport = LayerRect::new(LayerPoint::zero(), *viewport_size);

        let root_reference_frame_id = ScrollLayerId::root_reference_frame(pipeline_id);
        self.root_reference_frame_id = root_reference_frame_id;
        let reference_frame = Layer::new(&viewport, viewport.size, &identity, pipeline_id);
        self.layers.insert(self.root_reference_frame_id, reference_frame);

        let scroll_layer = Layer::new(&viewport, *content_size, &identity, pipeline_id);
        let topmost_scroll_layer_id = ScrollLayerId::root_scroll_layer(pipeline_id);
        self.topmost_scroll_layer_id = topmost_scroll_layer_id;
        self.add_layer(scroll_layer, topmost_scroll_layer_id, root_reference_frame_id);
    }

    pub fn collect_layers_bouncing_back(&self)
                                        -> HashSet<ScrollLayerId, BuildHasherDefault<FnvHasher>> {
        let mut layers_bouncing_back = HashSet::with_hasher(Default::default());
        for (scroll_layer_id, layer) in self.layers.iter() {
            if layer.scrolling.bouncing_back {
                layers_bouncing_back.insert(*scroll_layer_id);
            }
        }
        layers_bouncing_back
    }

    fn find_scrolling_layer_at_point_in_layer(&self,
                                              cursor: &WorldPoint,
                                              scroll_layer_id: ScrollLayerId)
                                              -> Option<ScrollLayerId> {
        self.layers.get(&scroll_layer_id).and_then(|layer| {
            for child_layer_id in layer.children.iter().rev() {
            if let Some(layer_id) =
                self.find_scrolling_layer_at_point_in_layer(cursor, *child_layer_id) {
                    return Some(layer_id);
                }
            }

            if let ScrollLayerInfo::ReferenceFrame(_) = scroll_layer_id.info {
                return None;
            }

            if layer.ray_intersects_layer(cursor) {
                Some(scroll_layer_id)
            } else {
                None
            }
        })
    }

    pub fn find_scrolling_layer_at_point(&self, cursor: &WorldPoint) -> ScrollLayerId {
        self.find_scrolling_layer_at_point_in_layer(cursor, self.root_reference_frame_id())
            .unwrap_or(self.topmost_scroll_layer_id())
    }

    pub fn get_scroll_layer_state(&self) -> Vec<ScrollLayerState> {
        let mut result = vec![];
        for (scroll_layer_id, scroll_layer) in self.layers.iter() {
            match scroll_layer_id.info {
                ScrollLayerInfo::Scrollable(_, servo_scroll_root_id) => {
                    result.push(ScrollLayerState {
                        pipeline_id: scroll_layer.pipeline_id,
                        scroll_root_id: servo_scroll_root_id,
                        scroll_offset: scroll_layer.scrolling.offset,
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
        for (layer_id, old_layer) in &mut self.layers.drain() {
            if !self.pipelines_to_discard.contains(&layer_id.pipeline_id) {
                scroll_states.insert(layer_id, old_layer.scrolling);
            }
        }

        self.pipelines_to_discard.clear();
        scroll_states
    }

    pub fn scroll_layers(&mut self,
                         origin: LayerPoint,
                         pipeline_id: PipelineId,
                         scroll_root_id: ServoScrollRootId)
                         -> bool {
        if self.layers.is_empty() {
            self.pending_scroll_offsets.insert((pipeline_id, scroll_root_id), origin);
            return false;
        }

        let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));

        let mut scrolled_a_layer = false;
        let mut found_layer = false;
        for (layer_id, layer) in self.layers.iter_mut() {
            if layer_id.pipeline_id != pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::ReferenceFrame(..) => continue,
                ScrollLayerInfo::Scrollable(..) => {}
            }

            found_layer = true;
            scrolled_a_layer |= layer.set_scroll_origin(&origin);
        }

        if !found_layer {
            self.pending_scroll_offsets.insert((pipeline_id, scroll_root_id), origin);
        }

        scrolled_a_layer
    }

    pub fn scroll(&mut self,
                  scroll_location: ScrollLocation,
                  cursor: WorldPoint,
                  phase: ScrollEventPhase)
                  -> bool {
        if self.layers.is_empty() {
            return false;
        }

        let scroll_layer_id = match (
            phase,
            self.find_scrolling_layer_at_point(&cursor),
            self.current_scroll_layer_id) {
            (ScrollEventPhase::Start, scroll_layer_at_point_id, _) => {
                self.current_scroll_layer_id = Some(scroll_layer_at_point_id);
                scroll_layer_at_point_id
            },
            (_, scroll_layer_at_point_id, Some(cached_scroll_layer_id)) => {
                let scroll_layer_id = match self.layers.get(&cached_scroll_layer_id) {
                    Some(_) => cached_scroll_layer_id,
                    None => {
                        self.current_scroll_layer_id = Some(scroll_layer_at_point_id);
                        scroll_layer_at_point_id
                    },
                };
                scroll_layer_id
            },
            (_, _, None) => return false,
        };

        let topmost_scroll_layer_id = self.topmost_scroll_layer_id();
        let non_root_overscroll = if scroll_layer_id != topmost_scroll_layer_id {
            // true if the current layer is overscrolling,
            // and it is not the root scroll layer.
            let child_layer = self.layers.get(&scroll_layer_id).unwrap();
            let overscroll_amount = child_layer.overscroll_amount();
            overscroll_amount.width != 0.0 || overscroll_amount.height != 0.0
        } else {
            false
        };

        let switch_layer = match phase {
            ScrollEventPhase::Start => {
                // if this is a new gesture, we do not switch layer,
                // however we do save the state of non_root_overscroll,
                // for use in the subsequent Move phase.
                let mut current_layer = self.layers.get_mut(&scroll_layer_id).unwrap();
                current_layer.scrolling.should_handoff_scroll = non_root_overscroll;
                false
            },
            ScrollEventPhase::Move(_) => {
                // Switch layer if movement originated in a new gesture,
                // from a non root layer in overscroll.
                let current_layer = self.layers.get_mut(&scroll_layer_id).unwrap();
                current_layer.scrolling.should_handoff_scroll && non_root_overscroll
            },
            ScrollEventPhase::End => {
                // clean-up when gesture ends.
                let mut current_layer = self.layers.get_mut(&scroll_layer_id).unwrap();
                current_layer.scrolling.should_handoff_scroll = false;
                false
            },
        };

        let scroll_layer_info = if switch_layer {
            topmost_scroll_layer_id.info
        } else {
            scroll_layer_id.info
        };

        let scroll_root_id = match scroll_layer_info {
             ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
             _ => unreachable!("Tried to scroll a reference frame."),

        };

        let mut scrolled_a_layer = false;
        for (layer_id, layer) in self.layers.iter_mut() {
            if layer_id.pipeline_id != scroll_layer_id.pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::ReferenceFrame(..) => continue,
                _ => {}
            }

            let scrolled_this_layer = layer.scroll(scroll_location, phase);
            scrolled_a_layer = scrolled_a_layer || scrolled_this_layer;
        }
        scrolled_a_layer
    }

    pub fn update_all_layer_transforms(&mut self) {
        if self.layers.is_empty() {
            return;
        }

        let root_reference_frame_id = self.root_reference_frame_id();
        let root_viewport = self.layers[&root_reference_frame_id].local_viewport_rect;
        self.update_layer_transform(root_reference_frame_id,
                                    &ScrollToWorldTransform::identity(),
                                    &as_scroll_parent_rect(&root_viewport));
    }

    fn update_layer_transform(&mut self,
                              layer_id: ScrollLayerId,
                              parent_world_transform: &ScrollToWorldTransform,
                              parent_viewport_rect: &ScrollLayerRect) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let (layer_transform_for_children, viewport_rect, layer_children) = {
            match self.layers.get_mut(&layer_id) {
                Some(layer) => {
                    layer.update_transform(parent_world_transform, parent_viewport_rect);

                    (layer.world_content_transform.with_source::<ScrollLayerPixel>(),
                     as_scroll_parent_rect(&layer.combined_local_viewport_rect),
                     layer.children.clone())
                }
                None => return,
            }
        };

        for child_layer_id in layer_children {
            self.update_layer_transform(child_layer_id,
                                        &layer_transform_for_children,
                                        &viewport_rect);
        }
    }

    pub fn tick_scrolling_bounce_animations(&mut self) {
        for (_, layer) in &mut self.layers {
            layer.tick_scrolling_bounce_animation()
        }
    }

    pub fn finalize_and_apply_pending_scroll_offsets(&mut self, old_states: ScrollStates) {
        // TODO(gw): These are all independent - can be run through thread pool if it shows up
        // in the profile!
        for (scroll_layer_id, layer) in &mut self.layers {
            let scrolling_state = match old_states.get(&scroll_layer_id) {
                Some(old_scrolling_state) => *old_scrolling_state,
                None => ScrollingState::new(),
            };

            layer.finalize(&scrolling_state);

            let scroll_root_id = match scroll_layer_id.info {
                ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
                _ => continue,
            };


            let pipeline_id = scroll_layer_id.pipeline_id;
            if let Some(pending_offset) =
                self.pending_scroll_offsets.remove(&(pipeline_id, scroll_root_id)) {
                layer.set_scroll_origin(&pending_offset);
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

        let layer = Layer::new(&rect, rect.size, &transform, pipeline_id);
        self.add_layer(layer, reference_frame_id, parent_id);
        reference_frame_id
    }

    pub fn add_layer(&mut self, layer: Layer, id: ScrollLayerId, parent_id: ScrollLayerId) {
        debug_assert!(!self.layers.contains_key(&id));
        self.layers.insert(id, layer);

        debug_assert!(parent_id != id);
        self.layers.get_mut(&parent_id).unwrap().add_child(id);
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.pipelines_to_discard.insert(pipeline_id);

        match self.current_scroll_layer_id {
            Some(id) if id.pipeline_id == pipeline_id => self.current_scroll_layer_id = None,
            _ => {}
        }
    }
}


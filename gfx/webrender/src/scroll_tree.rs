/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use fnv::FnvHasher;
use layer::{Layer, ScrollingState};
use std::collections::{HashMap, HashSet};
use std::hash::BuildHasherDefault;
use webrender_traits::{LayerPoint, PipelineId, ScrollEventPhase, ScrollLayerId, ScrollLayerInfo};
use webrender_traits::{ScrollLayerPixel, ScrollLayerRect, ScrollLayerState, ScrollLocation};
use webrender_traits::{ScrollToWorldTransform, ServoScrollRootId, WorldPoint};
use webrender_traits::as_scroll_parent_rect;

pub type ScrollStates = HashMap<ScrollLayerId, ScrollingState, BuildHasherDefault<FnvHasher>>;

pub struct ScrollTree {
    pub layers: HashMap<ScrollLayerId, Layer, BuildHasherDefault<FnvHasher>>,
    pub pending_scroll_offsets: HashMap<(PipelineId, ServoScrollRootId), LayerPoint>,
    pub current_scroll_layer_id: Option<ScrollLayerId>,
    pub root_scroll_layer_id: Option<ScrollLayerId>,
}

impl ScrollTree {
    pub fn new() -> ScrollTree {
        ScrollTree {
            layers: HashMap::with_hasher(Default::default()),
            pending_scroll_offsets: HashMap::new(),
            current_scroll_layer_id: None,
            root_scroll_layer_id: None,
        }
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

    pub fn get_scroll_layer(&self,
                            cursor: &WorldPoint,
                            scroll_layer_id: ScrollLayerId)
                            -> Option<ScrollLayerId> {
        self.layers.get(&scroll_layer_id).and_then(|layer| {
            for child_layer_id in layer.children.iter().rev() {
                if let Some(layer_id) = self.get_scroll_layer(cursor, *child_layer_id) {
                    return Some(layer_id);
                }
            }

            if scroll_layer_id.info == ScrollLayerInfo::Fixed {
                return None;
            }

            if layer.ray_intersects_layer(cursor) {
                Some(scroll_layer_id)
            } else {
                None
            }
        })
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
                ScrollLayerInfo::Fixed => {}
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        let mut scroll_states = HashMap::with_hasher(Default::default());
        for (layer_id, old_layer) in &mut self.layers.drain() {
            scroll_states.insert(layer_id, old_layer.scrolling);
        }
        scroll_states
    }

    pub fn scroll_layers(&mut self,
                         origin: LayerPoint,
                         pipeline_id: PipelineId,
                         scroll_root_id: ServoScrollRootId)
                         -> bool {
        let origin = LayerPoint::new(origin.x.max(0.0), origin.y.max(0.0));

        let mut scrolled_a_layer = false;
        let mut found_layer = false;
        for (layer_id, layer) in self.layers.iter_mut() {
            if layer_id.pipeline_id != pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::Fixed => continue,
                _ => {}
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
        let root_scroll_layer_id = match self.root_scroll_layer_id {
            Some(root_scroll_layer_id) => root_scroll_layer_id,
            None => return false,
        };

        let scroll_layer_id = match (
            phase,
            self.get_scroll_layer(&cursor, root_scroll_layer_id),
            self.current_scroll_layer_id) {
            (ScrollEventPhase::Start, Some(scroll_layer_id), _) => {
                self.current_scroll_layer_id = Some(scroll_layer_id);
                scroll_layer_id
            },
            (ScrollEventPhase::Start, None, _) => return false,
            (_, _, Some(scroll_layer_id)) => scroll_layer_id,
            (_, _, None) => return false,
        };

        let non_root_overscroll = if scroll_layer_id != root_scroll_layer_id {
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
            root_scroll_layer_id.info
        } else {
            scroll_layer_id.info
        };

        let scroll_root_id = match scroll_layer_info {
             ScrollLayerInfo::Scrollable(_, scroll_root_id) => scroll_root_id,
             _ => unreachable!("Tried to scroll a non-scrolling layer."),

        };

        let mut scrolled_a_layer = false;
        for (layer_id, layer) in self.layers.iter_mut() {
            if layer_id.pipeline_id != scroll_layer_id.pipeline_id {
                continue;
            }

            match layer_id.info {
                ScrollLayerInfo::Scrollable(_, id) if id != scroll_root_id => continue,
                ScrollLayerInfo::Fixed => continue,
                _ => {}
            }

            let scrolled_this_layer = layer.scroll(scroll_location, phase);
            scrolled_a_layer = scrolled_a_layer || scrolled_this_layer;
        }
        scrolled_a_layer
    }

    pub fn update_all_layer_transforms(&mut self) {
        let root_scroll_layer_id = self.root_scroll_layer_id;
        self.update_layer_transforms(root_scroll_layer_id);
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

    pub fn update_layer_transforms(&mut self, root_scroll_layer_id: Option<ScrollLayerId>) {
        if let Some(root_scroll_layer_id) = root_scroll_layer_id {
            let root_viewport = self.layers[&root_scroll_layer_id].local_viewport_rect;

            self.update_layer_transform(root_scroll_layer_id,
                                        &ScrollToWorldTransform::identity(),
                                        &as_scroll_parent_rect(&root_viewport));

            // Update any fixed layers
            let mut fixed_layers = Vec::new();
            for (layer_id, _) in &self.layers {
                match layer_id.info {
                    ScrollLayerInfo::Scrollable(..) => {}
                    ScrollLayerInfo::Fixed => {
                        fixed_layers.push(*layer_id);
                    }
                }
            }

            for layer_id in fixed_layers {
                self.update_layer_transform(layer_id,
                                            &ScrollToWorldTransform::identity(),
                                            &as_scroll_parent_rect(&root_viewport));
            }
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
                self.pending_scroll_offsets.get_mut(&(pipeline_id, scroll_root_id)) {
                layer.set_scroll_origin(pending_offset);
            }
        }

    }

    pub fn add_layer(&mut self, layer: Layer, id: ScrollLayerId, parent_id: Option<ScrollLayerId>) {
        debug_assert!(!self.layers.contains_key(&id));
        self.layers.insert(id, layer);

        if let Some(parent_id) = parent_id {
            debug_assert!(parent_id != id);
            self.layers.get_mut(&parent_id).unwrap().add_child(id);
        }
    }
}


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DeviceIntRect, DevicePixelScale, ExternalScrollId, LayoutPoint, LayoutRect, LayoutVector2D};
use api::{PipelineId, ScrollClamping, ScrollLocation, ScrollNodeState};
use api::WorldPoint;
use clip::{ClipChain, ClipSourcesHandle, ClipStore};
use clip_scroll_node::{ClipScrollNode, NodeType, ScrollFrameInfo, StickyFrameInfo};
use gpu_cache::GpuCache;
use gpu_types::{ClipScrollNodeIndex as GPUClipScrollNodeIndex, ClipScrollNodeData};
use internal_types::{FastHashMap, FastHashSet};
use print_tree::{PrintTree, PrintTreePrinter};
use resource_cache::ResourceCache;
use scene::SceneProperties;
use util::{LayoutFastTransform, LayoutToWorldFastTransform};

pub type ScrollStates = FastHashMap<ExternalScrollId, ScrollFrameInfo>;

/// An id that identifies coordinate systems in the ClipScrollTree. Each
/// coordinate system has an id and those ids will be shared when the coordinates
/// system are the same or are in the same axis-aligned space. This allows
/// for optimizing mask generation.
#[derive(Debug, Copy, Clone, PartialEq)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct CoordinateSystemId(pub u32);

#[derive(Debug, Copy, Clone, Eq, Hash, PartialEq)]
pub struct ClipScrollNodeIndex(pub usize);

const ROOT_REFERENCE_FRAME_INDEX: ClipScrollNodeIndex = ClipScrollNodeIndex(0);
const TOPMOST_SCROLL_NODE_INDEX: ClipScrollNodeIndex = ClipScrollNodeIndex(1);

impl CoordinateSystemId {
    pub fn root() -> Self {
        CoordinateSystemId(0)
    }

    pub fn next(&self) -> Self {
        let CoordinateSystemId(id) = *self;
        CoordinateSystemId(id + 1)
    }

    pub fn advance(&mut self) {
        self.0 += 1;
    }
}

pub struct ClipChainDescriptor {
    pub index: ClipChainIndex,
    pub parent: Option<ClipChainIndex>,
    pub clips: Vec<ClipScrollNodeIndex>,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ClipChainIndex(pub usize);

pub struct ClipScrollTree {
    pub nodes: Vec<ClipScrollNode>,

    /// A Vec of all descriptors that describe ClipChains in the order in which they are
    /// encountered during display list flattening. ClipChains are expected to never be
    /// the children of ClipChains later in the list.
    pub clip_chains_descriptors: Vec<ClipChainDescriptor>,

    /// A vector of all ClipChains in this ClipScrollTree including those from
    /// ClipChainDescriptors and also those defined by the clipping node hierarchy.
    pub clip_chains: Vec<ClipChain>,

    pub pending_scroll_offsets: FastHashMap<ExternalScrollId, (LayoutPoint, ScrollClamping)>,

    /// The current frame id, used for giving a unique id to all new dynamically
    /// added frames and clips. The ClipScrollTree increments this by one every
    /// time a new dynamic frame is created.
    current_new_node_item: u64,

    /// A set of pipelines which should be discarded the next time this
    /// tree is drained.
    pub pipelines_to_discard: FastHashSet<PipelineId>,
}

#[derive(Clone)]
pub struct TransformUpdateState {
    pub parent_reference_frame_transform: LayoutToWorldFastTransform,
    pub parent_accumulated_scroll_offset: LayoutVector2D,
    pub nearest_scrolling_ancestor_offset: LayoutVector2D,
    pub nearest_scrolling_ancestor_viewport: LayoutRect,

    /// The index of the current parent's clip chain.
    pub parent_clip_chain_index: ClipChainIndex,

    /// An id for keeping track of the axis-aligned space of this node. This is used in
    /// order to to track what kinds of clip optimizations can be done for a particular
    /// display list item, since optimizations can usually only be done among
    /// coordinate systems which are relatively axis aligned.
    pub current_coordinate_system_id: CoordinateSystemId,

    /// Transform from the coordinate system that started this compatible coordinate system.
    pub coordinate_system_relative_transform: LayoutFastTransform,

    /// True if this node is transformed by an invertible transform.  If not, display items
    /// transformed by this node will not be displayed and display items not transformed by this
    /// node will not be clipped by clips that are transformed by this node.
    pub invertible: bool,
}

impl ClipScrollTree {
    pub fn new() -> Self {
        ClipScrollTree {
            nodes: Vec::new(),
            clip_chains_descriptors: Vec::new(),
            clip_chains: vec![ClipChain::empty(&DeviceIntRect::zero())],
            pending_scroll_offsets: FastHashMap::default(),
            current_new_node_item: 1,
            pipelines_to_discard: FastHashSet::default(),
        }
    }

    /// The root reference frame, which is the true root of the ClipScrollTree. Initially
    /// this ID is not valid, which is indicated by ```nodes``` being empty.
    pub fn root_reference_frame_index(&self) -> ClipScrollNodeIndex {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.nodes.is_empty());
        ROOT_REFERENCE_FRAME_INDEX
    }

    /// The root scroll node which is the first child of the root reference frame.
    /// Initially this ID is not valid, which is indicated by ```nodes``` being empty.
    pub fn topmost_scroll_node_index(&self) -> ClipScrollNodeIndex {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(self.nodes.len() >= 1);
        TOPMOST_SCROLL_NODE_INDEX
    }

    fn find_scrolling_node_at_point_in_node(
        &self,
        cursor: &WorldPoint,
        index: ClipScrollNodeIndex,
    ) -> Option<ClipScrollNodeIndex> {
        let node = &self.nodes[index.0];
        for child_index in node.children.iter().rev() {
            let found_index = self.find_scrolling_node_at_point_in_node(cursor, *child_index);
            if found_index.is_some() {
                return found_index;
            }
        }

        match node.node_type {
            NodeType::ScrollFrame(state) if state.sensitive_to_input_events() => {}
            _ => return None,
        }

        if node.ray_intersects_node(cursor) {
            Some(index)
        } else {
            None
        }
    }

    pub fn find_scrolling_node_at_point(&self, cursor: &WorldPoint) -> ClipScrollNodeIndex {
        self.find_scrolling_node_at_point_in_node(cursor, self.root_reference_frame_index())
            .unwrap_or(self.topmost_scroll_node_index())
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollNodeState> {
        let mut result = vec![];
        for node in &self.nodes {
            if let NodeType::ScrollFrame(info) = node.node_type {
                if let Some(id) = info.external_id {
                    result.push(ScrollNodeState { id, scroll_offset: info.offset })
                }
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        self.current_new_node_item = 1;

        let mut scroll_states = FastHashMap::default();
        for old_node in &mut self.nodes.drain(..) {
            if self.pipelines_to_discard.contains(&old_node.pipeline_id) {
                continue;
            }

            match old_node.node_type {
                NodeType::ScrollFrame(info) if info.external_id.is_some() => {
                    scroll_states.insert(info.external_id.unwrap(), info);
                }
                _ => {}
            }
        }

        self.pipelines_to_discard.clear();
        self.clip_chains = vec![ClipChain::empty(&DeviceIntRect::zero())];
        self.clip_chains_descriptors.clear();
        scroll_states
    }

    pub fn scroll_node(
        &mut self,
        origin: LayoutPoint,
        id: ExternalScrollId,
        clamp: ScrollClamping
    ) -> bool {
        for node in &mut self.nodes {
            if node.matches_external_id(id) {
                return node.set_scroll_origin(&origin, clamp);
            }
        }

        self.pending_scroll_offsets.insert(id, (origin, clamp));
        false
    }

    pub fn scroll(
        &mut self,
        scroll_location: ScrollLocation,
        cursor: WorldPoint,
    ) -> bool {
        if self.nodes.is_empty() {
            return false;
        }

        let node_index = self.find_scrolling_node_at_point(&cursor);
        self.nodes[node_index.0].scroll(scroll_location)
    }

    pub fn update_tree(
        &mut self,
        screen_rect: &DeviceIntRect,
        device_pixel_scale: DevicePixelScale,
        clip_store: &mut ClipStore,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        pan: WorldPoint,
        node_data: &mut Vec<ClipScrollNodeData>,
        scene_properties: &SceneProperties,
    ) {
        if self.nodes.is_empty() {
            return;
        }

        self.clip_chains[0] = ClipChain::empty(screen_rect);

        let root_reference_frame_index = self.root_reference_frame_index();
        let mut state = TransformUpdateState {
            parent_reference_frame_transform: LayoutVector2D::new(pan.x, pan.y).into(),
            parent_accumulated_scroll_offset: LayoutVector2D::zero(),
            nearest_scrolling_ancestor_offset: LayoutVector2D::zero(),
            nearest_scrolling_ancestor_viewport: LayoutRect::zero(),
            parent_clip_chain_index: ClipChainIndex(0),
            current_coordinate_system_id: CoordinateSystemId::root(),
            coordinate_system_relative_transform: LayoutFastTransform::identity(),
            invertible: true,
        };
        let mut next_coordinate_system_id = state.current_coordinate_system_id.next();
        self.update_node(
            root_reference_frame_index,
            &mut state,
            &mut next_coordinate_system_id,
            device_pixel_scale,
            clip_store,
            resource_cache,
            gpu_cache,
            node_data,
            scene_properties,
        );

        self.build_clip_chains(screen_rect);
    }

    fn update_node(
        &mut self,
        node_index: ClipScrollNodeIndex,
        state: &mut TransformUpdateState,
        next_coordinate_system_id: &mut CoordinateSystemId,
        device_pixel_scale: DevicePixelScale,
        clip_store: &mut ClipStore,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        gpu_node_data: &mut Vec<ClipScrollNodeData>,
        scene_properties: &SceneProperties,
    ) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let mut state = state.clone();
        let node_children = {
            let node = match self.nodes.get_mut(node_index.0) {
                Some(node) => node,
                None => return,
            };

            // We set this early so that we can use it to populate the ClipChain.
            node.node_data_index = GPUClipScrollNodeIndex(gpu_node_data.len() as u32);

            node.update(
                &mut state,
                next_coordinate_system_id,
                device_pixel_scale,
                clip_store,
                resource_cache,
                gpu_cache,
                scene_properties,
                &mut self.clip_chains,
            );

            node.push_gpu_node_data(gpu_node_data);

            if node.children.is_empty() {
                return;
            }

            node.prepare_state_for_children(&mut state);
            node.children.clone()
        };

        for child_node_index in node_children {
            self.update_node(
                child_node_index,
                &mut state,
                next_coordinate_system_id,
                device_pixel_scale,
                clip_store,
                resource_cache,
                gpu_cache,
                gpu_node_data,
                scene_properties,
            );
        }
    }

    pub fn build_clip_chains(&mut self, screen_rect: &DeviceIntRect) {
        for descriptor in &self.clip_chains_descriptors {
            // A ClipChain is an optional parent (which is another ClipChain) and a list of
            // ClipScrollNode clipping nodes. Here we start the ClipChain with a clone of the
            // parent's node, if necessary.
            let mut chain = match descriptor.parent {
                Some(index) => self.clip_chains[index.0].clone(),
                None => ClipChain::empty(screen_rect),
            };

            // Now we walk through each ClipScrollNode in the vector of clip nodes and
            // extract their ClipChain nodes to construct the final list.
            for clip_index in &descriptor.clips {
                match self.nodes[clip_index.0].node_type {
                    NodeType::Clip { clip_chain_node: Some(ref node), .. } => {
                        chain.add_node(node.clone());
                    }
                    NodeType::Clip { .. } => warn!("Found uninitialized clipping ClipScrollNode."),
                    _ => warn!("Tried to create a clip chain with non-clipping node."),
                };
            }

            chain.parent_index = descriptor.parent;
            self.clip_chains[descriptor.index.0] = chain;
        }
    }

    pub fn finalize_and_apply_pending_scroll_offsets(&mut self, old_states: ScrollStates) {
        for node in &mut self.nodes {
            let external_id = match node.node_type {
                NodeType::ScrollFrame(ScrollFrameInfo { external_id: Some(id), ..} ) => id,
                _ => continue,
            };

            if let Some(scrolling_state) = old_states.get(&external_id) {
                node.apply_old_scrolling_state(scrolling_state);
            }

            if let Some((offset, clamping)) = self.pending_scroll_offsets.remove(&external_id) {
                node.set_scroll_origin(&offset, clamping);
            }
        }
    }

    pub fn add_clip_node(
        &mut self,
        index: ClipScrollNodeIndex,
        parent_index: ClipScrollNodeIndex,
        handle: ClipSourcesHandle,
        clip_rect: LayoutRect,
        pipeline_id: PipelineId,
    )  -> ClipChainIndex {
        let clip_chain_index = self.allocate_clip_chain();
        let node_type = NodeType::Clip { handle, clip_chain_index, clip_chain_node: None };
        let node = ClipScrollNode::new(pipeline_id, Some(parent_index), &clip_rect, node_type);
        self.add_node(node, index);
        clip_chain_index
    }

    pub fn add_sticky_frame(
        &mut self,
        index: ClipScrollNodeIndex,
        parent_index: ClipScrollNodeIndex,
        frame_rect: LayoutRect,
        sticky_frame_info: StickyFrameInfo,
        pipeline_id: PipelineId,
    ) {
        let node = ClipScrollNode::new_sticky_frame(
            parent_index,
            frame_rect,
            sticky_frame_info,
            pipeline_id,
        );
        self.add_node(node, index);
    }

    pub fn add_clip_chain_descriptor(
        &mut self,
        parent: Option<ClipChainIndex>,
        clips: Vec<ClipScrollNodeIndex>
    ) -> ClipChainIndex {
        let index = self.allocate_clip_chain();
        self.clip_chains_descriptors.push(ClipChainDescriptor { index, parent, clips });
        index
    }

    pub fn add_node(&mut self, node: ClipScrollNode, index: ClipScrollNodeIndex) {
        // When the parent node is None this means we are adding the root.
        if let Some(parent_index) = node.parent {
            self.nodes[parent_index.0].add_child(index);
        }

        if index.0 == self.nodes.len() {
            self.nodes.push(node);
            return;
        }


        if let Some(empty_node) = self.nodes.get_mut(index.0) {
            *empty_node = node;
            return
        }

        let length_to_reserve = index.0 + 1 - self.nodes.len();
        self.nodes.reserve_exact(length_to_reserve);

        // We would like to use `Vec::resize` here, but the Clone trait is not supported
        // for ClipScrollNodes. We can fix this either by splitting the clip nodes out into
        // their own tree or when support is added for something like `Vec::resize_default`.
        let length_to_extend = self.nodes.len() .. index.0;
        self.nodes.extend(length_to_extend.map(|_| ClipScrollNode::empty()));

        self.nodes.push(node);
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.pipelines_to_discard.insert(pipeline_id);
    }

    fn print_node<T: PrintTreePrinter>(
        &self,
        index: ClipScrollNodeIndex,
        pt: &mut T,
        clip_store: &ClipStore
    ) {
        let node = &self.nodes[index.0];
        match node.node_type {
            NodeType::Clip { ref handle, .. } => {
                pt.new_level("Clip".to_owned());

                pt.add_item(format!("index: {:?}", index));
                let clips = clip_store.get(handle).clips();
                pt.new_level(format!("Clip Sources [{}]", clips.len()));
                for source in clips {
                    pt.add_item(format!("{:?}", source));
                }
                pt.end_level();
            }
            NodeType::ReferenceFrame(ref info) => {
                pt.new_level(format!("ReferenceFrame {:?}", info.resolved_transform));
                pt.add_item(format!("index: {:?}", index));
            }
            NodeType::ScrollFrame(scrolling_info) => {
                pt.new_level(format!("ScrollFrame"));
                pt.add_item(format!("index: {:?}", index));
                pt.add_item(format!("scrollable_size: {:?}", scrolling_info.scrollable_size));
                pt.add_item(format!("scroll.offset: {:?}", scrolling_info.offset));
            }
            NodeType::StickyFrame(ref sticky_frame_info) => {
                pt.new_level(format!("StickyFrame"));
                pt.add_item(format!("index: {:?}", index));
                pt.add_item(format!("sticky info: {:?}", sticky_frame_info));
            }
            NodeType::Empty => unreachable!("Empty node remaining in ClipScrollTree."),
        }

        pt.add_item(format!("local_viewport_rect: {:?}", node.local_viewport_rect));
        pt.add_item(format!("world_viewport_transform: {:?}", node.world_viewport_transform));
        pt.add_item(format!("world_content_transform: {:?}", node.world_content_transform));
        pt.add_item(format!("coordinate_system_id: {:?}", node.coordinate_system_id));

        for child_index in &node.children {
            self.print_node(*child_index, pt, clip_store);
        }

        pt.end_level();
    }

    #[allow(dead_code)]
    pub fn print(&self, clip_store: &ClipStore) {
        if !self.nodes.is_empty() {
            let mut pt = PrintTree::new("clip_scroll tree");
            self.print_with(clip_store, &mut pt);
        }
    }

    pub fn print_with<T: PrintTreePrinter>(&self, clip_store: &ClipStore, pt: &mut T) {
        if !self.nodes.is_empty() {
            self.print_node(self.root_reference_frame_index(), pt, clip_store);
        }
    }

    pub fn allocate_clip_chain(&mut self) -> ClipChainIndex {
        debug_assert!(!self.clip_chains.is_empty());
        let new_clip_chain =self.clip_chains[0].clone();
        self.clip_chains.push(new_clip_chain);
        ClipChainIndex(self.clip_chains.len() - 1)
    }

    pub fn get_clip_chain(&self, index: ClipChainIndex) -> &ClipChain {
        &self.clip_chains[index.0]
    }
}

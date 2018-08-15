/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ExternalScrollId, LayoutPoint, LayoutRect, LayoutVector2D};
use api::{PipelineId, ScrollClamping, ScrollNodeState, ScrollLocation};
use api::{LayoutSize, LayoutTransform, PropertyBinding, ScrollSensitivity, WorldPoint};
use clip::{ClipStore};
use gpu_types::TransformPalette;
use internal_types::{FastHashMap, FastHashSet};
use print_tree::{PrintTree, PrintTreePrinter};
use scene::SceneProperties;
use spatial_node::{ScrollFrameInfo, SpatialNode, SpatialNodeType, StickyFrameInfo};
use util::LayoutToWorldFastTransform;

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
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct SpatialNodeIndex(pub usize);

const ROOT_REFERENCE_FRAME_INDEX: SpatialNodeIndex = SpatialNodeIndex(0);
const TOPMOST_SCROLL_NODE_INDEX: SpatialNodeIndex = SpatialNodeIndex(1);

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

pub struct ClipScrollTree {
    /// Nodes which determine the positions (offsets and transforms) for primitives
    /// and clips.
    pub spatial_nodes: Vec<SpatialNode>,

    pub pending_scroll_offsets: FastHashMap<ExternalScrollId, (LayoutPoint, ScrollClamping)>,

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

    /// An id for keeping track of the axis-aligned space of this node. This is used in
    /// order to to track what kinds of clip optimizations can be done for a particular
    /// display list item, since optimizations can usually only be done among
    /// coordinate systems which are relatively axis aligned.
    pub current_coordinate_system_id: CoordinateSystemId,

    /// Offset from the coordinate system that started this compatible coordinate system.
    pub coordinate_system_relative_offset: LayoutVector2D,

    /// True if this node is transformed by an invertible transform.  If not, display items
    /// transformed by this node will not be displayed and display items not transformed by this
    /// node will not be clipped by clips that are transformed by this node.
    pub invertible: bool,
}

impl ClipScrollTree {
    pub fn new() -> Self {
        ClipScrollTree {
            spatial_nodes: Vec::new(),
            pending_scroll_offsets: FastHashMap::default(),
            pipelines_to_discard: FastHashSet::default(),
        }
    }

    /// The root reference frame, which is the true root of the ClipScrollTree. Initially
    /// this ID is not valid, which is indicated by ```spatial_nodes``` being empty.
    pub fn root_reference_frame_index(&self) -> SpatialNodeIndex {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(!self.spatial_nodes.is_empty());
        ROOT_REFERENCE_FRAME_INDEX
    }

    /// The root scroll node which is the first child of the root reference frame.
    /// Initially this ID is not valid, which is indicated by ```spatial_nodes``` being empty.
    pub fn topmost_scroll_node_index(&self) -> SpatialNodeIndex {
        // TODO(mrobinson): We should eventually make this impossible to misuse.
        debug_assert!(self.spatial_nodes.len() >= 1);
        TOPMOST_SCROLL_NODE_INDEX
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollNodeState> {
        let mut result = vec![];
        for node in &self.spatial_nodes {
            if let SpatialNodeType::ScrollFrame(info) = node.node_type {
                if let Some(id) = info.external_id {
                    result.push(ScrollNodeState { id, scroll_offset: info.offset })
                }
            }
        }
        result
    }

    pub fn drain(&mut self) -> ScrollStates {
        let mut scroll_states = FastHashMap::default();
        for old_node in &mut self.spatial_nodes.drain(..) {
            if self.pipelines_to_discard.contains(&old_node.pipeline_id) {
                continue;
            }

            match old_node.node_type {
                SpatialNodeType::ScrollFrame(info) if info.external_id.is_some() => {
                    scroll_states.insert(info.external_id.unwrap(), info);
                }
                _ => {}
            }
        }

        self.pipelines_to_discard.clear();
        scroll_states
    }

    pub fn scroll_node(
        &mut self,
        origin: LayoutPoint,
        id: ExternalScrollId,
        clamp: ScrollClamping
    ) -> bool {
        for node in &mut self.spatial_nodes {
            if node.matches_external_id(id) {
                return node.set_scroll_origin(&origin, clamp);
            }
        }

        self.pending_scroll_offsets.insert(id, (origin, clamp));
        false
    }

    fn find_nearest_scrolling_ancestor(
        &self,
        index: Option<SpatialNodeIndex>
    ) -> SpatialNodeIndex {
        let index = match index {
            Some(index) => index,
            None => return self.topmost_scroll_node_index(),
        };

        let node = &self.spatial_nodes[index.0];
        match node.node_type {
            SpatialNodeType::ScrollFrame(state) if state.sensitive_to_input_events() => index,
            _ => self.find_nearest_scrolling_ancestor(node.parent)
        }
    }

    pub fn scroll_nearest_scrolling_ancestor(
        &mut self,
        scroll_location: ScrollLocation,
        node_index: Option<SpatialNodeIndex>,
    ) -> bool {
        if self.spatial_nodes.is_empty() {
            return false;
        }
        let node_index = self.find_nearest_scrolling_ancestor(node_index);
        self.spatial_nodes[node_index.0].scroll(scroll_location)
    }

    pub fn update_tree(
        &mut self,
        pan: WorldPoint,
        scene_properties: &SceneProperties,
    ) -> TransformPalette {
        let mut transform_palette = TransformPalette::new(self.spatial_nodes.len());
        if self.spatial_nodes.is_empty() {
            return transform_palette;
        }

        let root_reference_frame_index = self.root_reference_frame_index();
        let mut state = TransformUpdateState {
            parent_reference_frame_transform: LayoutVector2D::new(pan.x, pan.y).into(),
            parent_accumulated_scroll_offset: LayoutVector2D::zero(),
            nearest_scrolling_ancestor_offset: LayoutVector2D::zero(),
            nearest_scrolling_ancestor_viewport: LayoutRect::zero(),
            current_coordinate_system_id: CoordinateSystemId::root(),
            coordinate_system_relative_offset: LayoutVector2D::zero(),
            invertible: true,
        };

        let mut next_coordinate_system_id = state.current_coordinate_system_id.next();
        self.update_node(
            root_reference_frame_index,
            &mut state,
            &mut next_coordinate_system_id,
            &mut transform_palette,
            scene_properties,
        );

        transform_palette
    }

    fn update_node(
        &mut self,
        node_index: SpatialNodeIndex,
        state: &mut TransformUpdateState,
        next_coordinate_system_id: &mut CoordinateSystemId,
        transform_palette: &mut TransformPalette,
        scene_properties: &SceneProperties,
    ) {
        // TODO(gw): This is an ugly borrow check workaround to clone these.
        //           Restructure this to avoid the clones!
        let mut state = state.clone();
        let node_children = {
            let node = match self.spatial_nodes.get_mut(node_index.0) {
                Some(node) => node,
                None => return,
            };

            node.update(&mut state, next_coordinate_system_id, scene_properties);
            node.push_gpu_data(transform_palette, node_index);

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
                transform_palette,
                scene_properties,
            );
        }
    }

    pub fn finalize_and_apply_pending_scroll_offsets(&mut self, old_states: ScrollStates) {
        for node in &mut self.spatial_nodes {
            let external_id = match node.node_type {
                SpatialNodeType::ScrollFrame(ScrollFrameInfo { external_id: Some(id), ..} ) => id,
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

    pub fn add_scroll_frame(
        &mut self,
        parent_index: SpatialNodeIndex,
        external_id: Option<ExternalScrollId>,
        pipeline_id: PipelineId,
        frame_rect: &LayoutRect,
        content_size: &LayoutSize,
        scroll_sensitivity: ScrollSensitivity,
    ) -> SpatialNodeIndex {
        let node = SpatialNode::new_scroll_frame(
            pipeline_id,
            parent_index,
            external_id,
            frame_rect,
            content_size,
            scroll_sensitivity,
        );
        self.add_spatial_node(node)
    }

    pub fn add_reference_frame(
        &mut self,
        parent_index: Option<SpatialNodeIndex>,
        source_transform: Option<PropertyBinding<LayoutTransform>>,
        source_perspective: Option<LayoutTransform>,
        origin_in_parent_reference_frame: LayoutVector2D,
        pipeline_id: PipelineId,
    ) -> SpatialNodeIndex {
        let node = SpatialNode::new_reference_frame(
            parent_index,
            source_transform,
            source_perspective,
            origin_in_parent_reference_frame,
            pipeline_id,
        );
        self.add_spatial_node(node)
    }

    pub fn add_sticky_frame(
        &mut self,
        parent_index: SpatialNodeIndex,
        sticky_frame_info: StickyFrameInfo,
        pipeline_id: PipelineId,
    ) -> SpatialNodeIndex {
        let node = SpatialNode::new_sticky_frame(
            parent_index,
            sticky_frame_info,
            pipeline_id,
        );
        self.add_spatial_node(node)
    }

    pub fn add_spatial_node(&mut self, node: SpatialNode) -> SpatialNodeIndex {
        let index = SpatialNodeIndex(self.spatial_nodes.len());

        // When the parent node is None this means we are adding the root.
        if let Some(parent_index) = node.parent {
            self.spatial_nodes[parent_index.0].add_child(index);
        }

        self.spatial_nodes.push(node);
        index
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.pipelines_to_discard.insert(pipeline_id);
    }

    fn print_node<T: PrintTreePrinter>(
        &self,
        index: SpatialNodeIndex,
        pt: &mut T,
        clip_store: &ClipStore
    ) {
        let node = &self.spatial_nodes[index.0];
        match node.node_type {
            SpatialNodeType::StickyFrame(ref sticky_frame_info) => {
                pt.new_level(format!("StickyFrame"));
                pt.add_item(format!("index: {:?}", index));
                pt.add_item(format!("sticky info: {:?}", sticky_frame_info));
            }
            SpatialNodeType::ScrollFrame(scrolling_info) => {
                pt.new_level(format!("ScrollFrame"));
                pt.add_item(format!("index: {:?}", index));
                pt.add_item(format!("viewport: {:?}", scrolling_info.viewport_rect));
                pt.add_item(format!("scrollable_size: {:?}", scrolling_info.scrollable_size));
                pt.add_item(format!("scroll offset: {:?}", scrolling_info.offset));
            }
            SpatialNodeType::ReferenceFrame(ref info) => {
                pt.new_level(format!("ReferenceFrame {:?}", info.resolved_transform));
                pt.add_item(format!("index: {:?}", index));
            }
        }

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
        if !self.spatial_nodes.is_empty() {
            let mut pt = PrintTree::new("clip_scroll tree");
            self.print_with(clip_store, &mut pt);
        }
    }

    pub fn print_with<T: PrintTreePrinter>(&self, clip_store: &ClipStore, pt: &mut T) {
        if !self.spatial_nodes.is_empty() {
            self.print_node(self.root_reference_frame_index(), pt, clip_store);
        }
    }
}

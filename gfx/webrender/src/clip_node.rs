/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::DevicePixelScale;
use clip::{ClipChain, ClipChainNode, ClipSourcesIndex, ClipStore, ClipWorkItem};
use clip_scroll_tree::{ClipChainIndex};
use gpu_cache::GpuCache;
use resource_cache::ResourceCache;
use spatial_node::SpatialNode;

#[derive(Debug)]
pub struct ClipNode {
    /// A handle to this clip nodes clips in the ClipStore.
    pub clip_sources_index: ClipSourcesIndex,

    /// An index to a ClipChain defined by this ClipNode's hiearchy in the display
    /// list.
    pub clip_chain_index: ClipChainIndex,

    /// The index of the parent ClipChain of this node's hiearchical ClipChain.
    pub parent_clip_chain_index: ClipChainIndex,

    /// A copy of the ClipChainNode this node would produce. We need to keep a copy,
    /// because the ClipChain may not contain our node if is optimized out, but API
    /// defined ClipChains will still need to access it.
    pub clip_chain_node: Option<ClipChainNode>,
}

impl ClipNode {
    pub fn update(
        &mut self,
        device_pixel_scale: DevicePixelScale,
        clip_store: &mut ClipStore,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        clip_chains: &mut [ClipChain],
        spatial_nodes: &[SpatialNode],
    ) {
        let clip_sources = &mut clip_store[self.clip_sources_index];
        clip_sources.update(gpu_cache, resource_cache, device_pixel_scale);
        let spatial_node = &spatial_nodes[clip_sources.spatial_node_index.0];

        let (screen_inner_rect, screen_outer_rect) = clip_sources.get_screen_bounds(
            &spatial_node.world_content_transform,
            device_pixel_scale,
            None,
        );

        // All clipping SpatialNodes should have outer rectangles, because they never
        // use the BorderCorner clip type and they always have at last one non-ClipOut
        // Rectangle ClipSource.
        let screen_outer_rect = screen_outer_rect
            .expect("Clipping node didn't have outer rect.");
        let local_outer_rect = clip_sources.local_outer_rect
            .expect("Clipping node didn't have outer rect.");

        let new_node = ClipChainNode {
            work_item: ClipWorkItem {
                clip_sources_index: self.clip_sources_index,
                coordinate_system_id: spatial_node.coordinate_system_id,
            },
            local_clip_rect: local_outer_rect
                .translate(&spatial_node.coordinate_system_relative_offset),
            screen_outer_rect,
            screen_inner_rect,
            prev: None,
        };

        let mut clip_chain =
            clip_chains[self.parent_clip_chain_index.0]
            .new_with_added_node(&new_node);

        self.clip_chain_node = Some(new_node);
        clip_chain.parent_index = Some(self.parent_clip_chain_index);
        clip_chains[self.clip_chain_index.0] = clip_chain;
    }
}

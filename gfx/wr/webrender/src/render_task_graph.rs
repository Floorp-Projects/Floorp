//! This module contains the render task graph.
//!
//! Code associated with creating specific render tasks is in the render_task
//! module.

use api::ImageFormat;
use crate::frame_graph::{FrameGraph, FrameGraphBuilder, Pass};
use crate::internal_types::{CacheTextureId, FastHashMap};
use crate::render_target::{RenderTargetList, ColorRenderTarget};
use crate::render_target::{PictureCacheTarget, TextureCacheRenderTarget, AlphaRenderTarget};
use crate::render_task::RenderTask;
use crate::util::Allocation;
use std::{usize, f32, u32};

// TODO(gw): To reduce the size of the initial inegration patch, typedef the
//           old task graph and builder to the new frame graph / builder, which
//           are API compatible. In future, use the new types directly and adjust
//           the API.
pub type RenderTaskGraphBuilder = FrameGraphBuilder;
pub type RenderTaskGraph = FrameGraph;

/// Allows initializing a render task directly into the render task buffer.
///
/// See utils::VecHelpers. RenderTask is fairly large so avoiding the move when
/// pushing into the vector can save a lot of exensive memcpys on pages with many
/// render tasks.
pub struct RenderTaskAllocation<'a> {
    pub alloc: Allocation<'a, RenderTask>,
}

impl<'l> RenderTaskAllocation<'l> {
    #[inline(always)]
    pub fn init(self, value: RenderTask) -> RenderTaskId {
        RenderTaskId {
            index: self.alloc.init(value) as u32,
        }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderTaskId {
    pub index: u32,
}

impl RenderTaskId {
    pub const INVALID: RenderTaskId = RenderTaskId {
        index: u32::MAX,
    };
}

/// A render pass represents a set of rendering operations that don't depend on one
/// another.
///
/// A render pass can have several render targets if there wasn't enough space in one
/// target to do all of the rendering for that pass. See `RenderTargetList`.
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct RenderPass {
    /// The subpasses that describe targets being rendered to in this pass
    pub alpha: RenderTargetList<AlphaRenderTarget>,
    pub color: RenderTargetList<ColorRenderTarget>,
    pub texture_cache: FastHashMap<(CacheTextureId, usize), TextureCacheRenderTarget>,
    pub picture_cache: Vec<PictureCacheTarget>,
    /// The set of tasks to be performed in this pass, as indices into the
    /// `RenderTaskGraph`.
    pub tasks: Vec<RenderTaskId>,
    pub textures_to_invalidate: Vec<CacheTextureId>,
}

impl RenderPass {
    /// Creates an intermediate off-screen pass.
    pub fn new(src: &Pass) -> Self {
        RenderPass {
            color: RenderTargetList::new(
                ImageFormat::RGBA8,
            ),
            alpha: RenderTargetList::new(
                ImageFormat::R8,
            ),
            texture_cache: FastHashMap::default(),
            picture_cache: Vec::new(),
            tasks: vec![],
            textures_to_invalidate: src.textures_to_invalidate.clone(),
        }
    }
}

// Dump an SVG visualization of the render graph for debugging purposes
#[allow(dead_code)]
pub fn dump_render_tasks_as_svg(
    render_tasks: &RenderTaskGraph,
    passes: &[RenderPass],
    output: &mut dyn std::io::Write,
) -> std::io::Result<()> {
    use svg_fmt::*;

    let node_width = 80.0;
    let node_height = 30.0;
    let vertical_spacing = 8.0;
    let horizontal_spacing = 20.0;
    let margin = 10.0;
    let text_size = 10.0;

    let mut pass_rects = Vec::new();
    let mut nodes = vec![None; render_tasks.tasks.len()];

    let mut x = margin;
    let mut max_y: f32 = 0.0;

    #[derive(Clone)]
    struct Node {
        rect: Rectangle,
        label: Text,
        size: Text,
    }

    for pass in passes {
        let mut layout = VerticalLayout::new(x, margin, node_width);

        for task_id in &pass.tasks {
            let task_index = task_id.index as usize;
            let task = &render_tasks.tasks[task_index];

            let rect = layout.push_rectangle(node_height);

            let tx = rect.x + rect.w / 2.0;
            let ty = rect.y + 10.0;

            let label = text(tx, ty, format!("{}", task.kind.as_str()));
            let size = text(tx, ty + 12.0, format!("{:?}", task.location.size()));

            nodes[task_index] = Some(Node { rect, label, size });

            layout.advance(vertical_spacing);
        }

        pass_rects.push(layout.total_rectangle());

        x += node_width + horizontal_spacing;
        max_y = max_y.max(layout.y + margin);
    }

    let mut links = Vec::new();
    for node_index in 0..nodes.len() {
        if nodes[node_index].is_none() {
            continue;
        }

        let task = &render_tasks.tasks[node_index];
        for dep in &task.children {
            let dep_index = dep.index as usize;

            if let (&Some(ref node), &Some(ref dep_node)) = (&nodes[node_index], &nodes[dep_index]) {
                links.push((
                    dep_node.rect.x + dep_node.rect.w,
                    dep_node.rect.y + dep_node.rect.h / 2.0,
                    node.rect.x,
                    node.rect.y + node.rect.h / 2.0,
                ));
            }
        }
    }

    let svg_w = x + margin;
    let svg_h = max_y + margin;
    writeln!(output, "{}", BeginSvg { w: svg_w, h: svg_h })?;

    // Background.
    writeln!(output,
        "    {}",
        rectangle(0.0, 0.0, svg_w, svg_h)
            .inflate(1.0, 1.0)
            .fill(rgb(50, 50, 50))
    )?;

    // Passes.
    for rect in pass_rects {
        writeln!(output,
            "    {}",
            rect.inflate(3.0, 3.0)
                .border_radius(4.0)
                .opacity(0.4)
                .fill(black())
        )?;
    }

    // Links.
    for (x1, y1, x2, y2) in links {
        dump_task_dependency_link(output, x1, y1, x2, y2);
    }

    // Tasks.
    for node in &nodes {
        if let Some(node) = node {
            writeln!(output,
                "    {}",
                node.rect
                    .clone()
                    .fill(black())
                    .border_radius(3.0)
                    .opacity(0.5)
                    .offset(0.0, 2.0)
            )?;
            writeln!(output,
                "    {}",
                node.rect
                    .clone()
                    .fill(rgb(200, 200, 200))
                    .border_radius(3.0)
                    .opacity(0.8)
            )?;

            writeln!(output,
                "    {}",
                node.label
                    .clone()
                    .size(text_size)
                    .align(Align::Center)
                    .color(rgb(50, 50, 50))
            )?;
            writeln!(output,
                "    {}",
                node.size
                    .clone()
                    .size(text_size * 0.7)
                    .align(Align::Center)
                    .color(rgb(50, 50, 50))
            )?;
        }
    }

    writeln!(output, "{}", EndSvg)
}

#[allow(dead_code)]
fn dump_task_dependency_link(
    output: &mut dyn std::io::Write,
    x1: f32, y1: f32,
    x2: f32, y2: f32,
) {
    use svg_fmt::*;

    // If the link is a straight horizontal line and spans over multiple passes, it
    // is likely to go straight though unrelated nodes in a way that makes it look like
    // they are connected, so we bend the line upward a bit to avoid that.
    let simple_path = (y1 - y2).abs() > 1.0 || (x2 - x1) < 45.0;

    let mid_x = (x1 + x2) / 2.0;
    if simple_path {
        write!(output, "    {}",
            path().move_to(x1, y1)
                .cubic_bezier_to(mid_x, y1, mid_x, y2, x2, y2)
                .fill(Fill::None)
                .stroke(Stroke::Color(rgb(100, 100, 100), 3.0))
        ).unwrap();
    } else {
        let ctrl1_x = (mid_x + x1) / 2.0;
        let ctrl2_x = (mid_x + x2) / 2.0;
        let ctrl_y = y1 - 25.0;
        write!(output, "    {}",
            path().move_to(x1, y1)
                .cubic_bezier_to(ctrl1_x, y1, ctrl1_x, ctrl_y, mid_x, ctrl_y)
                .cubic_bezier_to(ctrl2_x, ctrl_y, ctrl2_x, y2, x2, y2)
                .fill(Fill::None)
                .stroke(Stroke::Color(rgb(100, 100, 100), 3.0))
        ).unwrap();
    }
}

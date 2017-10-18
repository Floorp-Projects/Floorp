/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ClipAndScrollInfo, Shadow};
use prim_store::PrimitiveIndex;
use render_task::RenderTaskId;
use tiling::RenderTargetKind;

/*
 A picture represents a dynamically rendered image. It consists of:

 * A number of primitives that are drawn onto the picture.
 * A composite operation describing how to composite this
   picture into its parent.
 * A configuration describing how to draw the primitives on
   this picture (e.g. in screen space or local space).
 */

#[derive(Clone, Debug)]
pub struct PrimitiveRun {
    pub prim_index: PrimitiveIndex,
    pub count: usize,
    pub clip_and_scroll: ClipAndScrollInfo,
}

#[derive(Debug)]
pub enum CompositeOp {
    Shadow(Shadow),

    // TODO(gw): Support other composite ops, such
    //           as blur, blend etc.
}

#[derive(Debug)]
pub struct PicturePrimitive {
    pub prim_runs: Vec<PrimitiveRun>,
    pub composite_op: CompositeOp,
    pub render_task_id: Option<RenderTaskId>,
    pub kind: RenderTargetKind,

    // TODO(gw): Add a mode that specifies if this
    //           picture should be rasterized in
    //           screen-space or local-space.
}

impl PicturePrimitive {
    pub fn new_shadow(
        shadow: Shadow,
        kind: RenderTargetKind,
    ) -> PicturePrimitive {
        PicturePrimitive {
            prim_runs: Vec::new(),
            composite_op: CompositeOp::Shadow(shadow),
            render_task_id: None,
            kind,
        }
    }

    pub fn as_shadow(&self) -> &Shadow {
        match self.composite_op {
            CompositeOp::Shadow(ref shadow) => shadow,
        }
    }

    pub fn add_primitive(
        &mut self,
        prim_index: PrimitiveIndex,
        clip_and_scroll: ClipAndScrollInfo
    ) {
        if let Some(ref mut run) = self.prim_runs.last_mut() {
            if run.clip_and_scroll == clip_and_scroll &&
               run.prim_index.0 + run.count == prim_index.0 {
                run.count += 1;
                return;
            }
        }

        self.prim_runs.push(PrimitiveRun {
            prim_index,
            count: 1,
            clip_and_scroll,
        });
    }
}

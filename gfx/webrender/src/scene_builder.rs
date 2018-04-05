/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DocumentId, PipelineId, ApiMsg, FrameMsg, ResourceUpdates};
use api::channel::MsgSender;
use display_list_flattener::build_scene;
use frame_builder::{FrameBuilderConfig, FrameBuilder};
use clip_scroll_tree::ClipScrollTree;
use internal_types::FastHashSet;
use resource_cache::{FontInstanceMap, TiledImageMap};
use render_backend::DocumentView;
use scene::Scene;
use std::sync::mpsc::{channel, Receiver, Sender};

// Message from render backend to scene builder.
pub enum SceneBuilderRequest {
    Transaction {
        document_id: DocumentId,
        scene: Option<SceneRequest>,
        resource_updates: ResourceUpdates,
        frame_ops: Vec<FrameMsg>,
        render: bool,
    },
    Stop
}

// Message from scene builder to render backend.
pub enum SceneBuilderResult {
    Transaction {
        document_id: DocumentId,
        built_scene: Option<BuiltScene>,
        resource_updates: ResourceUpdates,
        frame_ops: Vec<FrameMsg>,
        render: bool,
    },
}

/// Contains the render backend data needed to build a scene.
pub struct SceneRequest {
    pub scene: Scene,
    pub view: DocumentView,
    pub font_instances: FontInstanceMap,
    pub tiled_image_map: TiledImageMap,
    pub output_pipelines: FastHashSet<PipelineId>,
    pub removed_pipelines: Vec<PipelineId>,
}

pub struct BuiltScene {
    pub scene: Scene,
    pub frame_builder: FrameBuilder,
    pub clip_scroll_tree: ClipScrollTree,
    pub removed_pipelines: Vec<PipelineId>,
}

pub struct SceneBuilder {
    rx: Receiver<SceneBuilderRequest>,
    tx: Sender<SceneBuilderResult>,
    api_tx: MsgSender<ApiMsg>,
    config: FrameBuilderConfig,
}

impl SceneBuilder {
    pub fn new(
        config: FrameBuilderConfig,
        api_tx: MsgSender<ApiMsg>
    ) -> (Self, Sender<SceneBuilderRequest>, Receiver<SceneBuilderResult>) {
        let (in_tx, in_rx) = channel();
        let (out_tx, out_rx) = channel();
        (
            SceneBuilder {
                rx: in_rx,
                tx: out_tx,
                api_tx,
                config,
            },
            in_tx,
            out_rx,
        )
    }

    pub fn run(&mut self) {
        loop {
            match self.rx.recv() {
                Ok(msg) => {
                    if !self.process_message(msg) {
                        return;
                    }
                }
                Err(_) => {
                    return;
                }
            }
        }
    }

    fn process_message(&mut self, msg: SceneBuilderRequest) -> bool {
        match msg {
            SceneBuilderRequest::Transaction {
                document_id,
                scene,
                resource_updates,
                frame_ops,
                render,
            } => {
                let built_scene = scene.map(|request|{
                    build_scene(&self.config, request)
                });

                // TODO: pre-rasterization.

                self.tx.send(SceneBuilderResult::Transaction {
                    document_id,
                    built_scene,
                    resource_updates,
                    frame_ops,
                    render,
                }).unwrap();

                let _ = self.api_tx.send(ApiMsg::WakeUp);
            }
            SceneBuilderRequest::Stop => { return false; }
        }

        true
    }
}

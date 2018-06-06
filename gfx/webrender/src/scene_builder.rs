/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{DocumentId, PipelineId, ApiMsg, FrameMsg, ResourceUpdate};
use api::channel::MsgSender;
use display_list_flattener::build_scene;
use frame_builder::{FrameBuilderConfig, FrameBuilder};
use clip_scroll_tree::ClipScrollTree;
use internal_types::FastHashSet;
use resource_cache::FontInstanceMap;
use render_backend::DocumentView;
use renderer::{PipelineInfo, SceneBuilderHooks};
use scene::Scene;
use std::sync::mpsc::{channel, Receiver, Sender};

// Message from render backend to scene builder.
pub enum SceneBuilderRequest {
    Transaction {
        document_id: DocumentId,
        scene: Option<SceneRequest>,
        resource_updates: Vec<ResourceUpdate>,
        frame_ops: Vec<FrameMsg>,
        render: bool,
    },
    WakeUp,
    Flush(MsgSender<()>),
    Stop
}

// Message from scene builder to render backend.
pub enum SceneBuilderResult {
    Transaction {
        document_id: DocumentId,
        built_scene: Option<BuiltScene>,
        resource_updates: Vec<ResourceUpdate>,
        frame_ops: Vec<FrameMsg>,
        render: bool,
        result_tx: Option<Sender<SceneSwapResult>>,
    },
    FlushComplete(MsgSender<()>),
    Stopped,
}

// Message from render backend to scene builder to indicate the
// scene swap was completed. We need a separate channel for this
// so that they don't get mixed with SceneBuilderRequest messages.
pub enum SceneSwapResult {
    Complete(Sender<()>),
    Aborted,
}

/// Contains the render backend data needed to build a scene.
pub struct SceneRequest {
    pub scene: Scene,
    pub view: DocumentView,
    pub font_instances: FontInstanceMap,
    pub output_pipelines: FastHashSet<PipelineId>,
    pub removed_pipelines: Vec<PipelineId>,
    pub scene_id: u64,
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
    hooks: Option<Box<SceneBuilderHooks + Send>>,
}

impl SceneBuilder {
    pub fn new(
        config: FrameBuilderConfig,
        api_tx: MsgSender<ApiMsg>,
        hooks: Option<Box<SceneBuilderHooks + Send>>,
    ) -> (Self, Sender<SceneBuilderRequest>, Receiver<SceneBuilderResult>) {
        let (in_tx, in_rx) = channel();
        let (out_tx, out_rx) = channel();
        (
            SceneBuilder {
                rx: in_rx,
                tx: out_tx,
                api_tx,
                config,
                hooks,
            },
            in_tx,
            out_rx,
        )
    }

    pub fn run(&mut self) {
        if let Some(ref hooks) = self.hooks {
            hooks.register();
        }

        loop {
            match self.rx.recv() {
                Ok(msg) => {
                    if !self.process_message(msg) {
                        break;
                    }
                }
                Err(_) => {
                    break;
                }
            }

            if let Some(ref hooks) = self.hooks {
                hooks.poke();
            }
        }

        if let Some(ref hooks) = self.hooks {
            hooks.deregister();
        }
    }

    fn process_message(&mut self, msg: SceneBuilderRequest) -> bool {
        match msg {
            SceneBuilderRequest::WakeUp => {}
            SceneBuilderRequest::Flush(tx) => {
                self.tx.send(SceneBuilderResult::FlushComplete(tx)).unwrap();
                let _ = self.api_tx.send(ApiMsg::WakeUp);
            }
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

                // We only need the pipeline info and the result channel if we
                // have a hook callback *and* if this transaction actually built
                // a new scene that is going to get swapped in. In other cases
                // pipeline_info can be None and we can avoid some overhead from
                // invoking the hooks and blocking on the channel.
                let (pipeline_info, result_tx, result_rx) = match (&self.hooks, &built_scene) {
                    (&Some(ref hooks), &Some(ref built)) => {
                        let info = PipelineInfo {
                            epochs: built.scene.pipeline_epochs.clone(),
                            removed_pipelines: built.removed_pipelines.clone(),
                        };
                        let (tx, rx) = channel();

                        hooks.pre_scene_swap();

                        (Some(info), Some(tx), Some(rx))
                    }
                    _ => (None, None, None),
                };

                let has_resources_updates = !resource_updates.is_empty();
                self.tx.send(SceneBuilderResult::Transaction {
                    document_id,
                    built_scene,
                    resource_updates,
                    frame_ops,
                    render,
                    result_tx,
                }).unwrap();

                let _ = self.api_tx.send(ApiMsg::WakeUp);

                if let Some(pipeline_info) = pipeline_info {
                    // Block until the swap is done, then invoke the hook.
                    let swap_result = result_rx.unwrap().recv();
                    self.hooks.as_ref().unwrap().post_scene_swap(pipeline_info);
                    // Once the hook is done, allow the RB thread to resume
                    match swap_result {
                        Ok(SceneSwapResult::Complete(resume_tx)) => {
                            resume_tx.send(()).ok();
                        },
                        _ => (),
                    };
                } else if has_resources_updates {
                    if let &Some(ref hooks) = &self.hooks {
                        hooks.post_resource_update();
                    }
                }
            }
            SceneBuilderRequest::Stop => {
                self.tx.send(SceneBuilderResult::Stopped).unwrap();
                // We don't need to send a WakeUp to api_tx because we only
                // get the Stop when the RenderBackend loop is exiting.
                return false;
            }
        }

        true
    }
}

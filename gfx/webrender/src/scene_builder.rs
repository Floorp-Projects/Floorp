/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{AsyncBlobImageRasterizer, BlobImageRequest, BlobImageParams, BlobImageResult};
use api::{DocumentId, PipelineId, ApiMsg, FrameMsg, ResourceUpdate, Epoch};
use api::{BuiltDisplayList, ColorF, LayoutSize, NotificationRequest, Checkpoint};
use api::channel::MsgSender;
use frame_builder::{FrameBuilderConfig, FrameBuilder};
use clip_scroll_tree::ClipScrollTree;
use display_list_flattener::DisplayListFlattener;
use internal_types::{FastHashMap, FastHashSet};
use picture::PictureIdGenerator;
use resource_cache::FontInstanceMap;
use render_backend::DocumentView;
use renderer::{PipelineInfo, SceneBuilderHooks};
use scene::Scene;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::mem::replace;
use time::precise_time_ns;
use util::drain_filter;

/// Represents the work associated to a transaction before scene building.
pub struct Transaction {
    pub document_id: DocumentId,
    pub display_list_updates: Vec<DisplayListUpdate>,
    pub removed_pipelines: Vec<PipelineId>,
    pub epoch_updates: Vec<(PipelineId, Epoch)>,
    pub request_scene_build: Option<SceneRequest>,
    pub blob_requests: Vec<BlobImageParams>,
    pub blob_rasterizer: Option<Box<AsyncBlobImageRasterizer>>,
    pub rasterized_blobs: Vec<(BlobImageRequest, BlobImageResult)>,
    pub resource_updates: Vec<ResourceUpdate>,
    pub frame_ops: Vec<FrameMsg>,
    pub notifications: Vec<NotificationRequest>,
    pub set_root_pipeline: Option<PipelineId>,
    pub build_frame: bool,
    pub render_frame: bool,
}

impl Transaction {
    pub fn can_skip_scene_builder(&self) -> bool {
        self.request_scene_build.is_none() &&
            self.display_list_updates.is_empty() &&
            self.epoch_updates.is_empty() &&
            self.removed_pipelines.is_empty() &&
            self.blob_requests.is_empty() &&
            self.set_root_pipeline.is_none()
    }

    pub fn should_build_scene(&self) -> bool {
        !self.display_list_updates.is_empty() ||
            self.set_root_pipeline.is_some()
    }
}

/// Represent the remaining work associated to a transaction after the scene building
/// phase as well as the result of scene building itself if applicable.
pub struct BuiltTransaction {
    pub document_id: DocumentId,
    pub built_scene: Option<BuiltScene>,
    pub resource_updates: Vec<ResourceUpdate>,
    pub rasterized_blobs: Vec<(BlobImageRequest, BlobImageResult)>,
    pub blob_rasterizer: Option<Box<AsyncBlobImageRasterizer>>,
    pub frame_ops: Vec<FrameMsg>,
    pub removed_pipelines: Vec<PipelineId>,
    pub notifications: Vec<NotificationRequest>,
    pub scene_build_start_time: u64,
    pub scene_build_end_time: u64,
    pub build_frame: bool,
    pub render_frame: bool,
}

pub struct DisplayListUpdate {
    pub pipeline_id: PipelineId,
    pub epoch: Epoch,
    pub built_display_list: BuiltDisplayList,
    pub background: Option<ColorF>,
    pub viewport_size: LayoutSize,
    pub content_size: LayoutSize,
}

/// Contains the render backend data needed to build a scene.
pub struct SceneRequest {
    pub view: DocumentView,
    pub font_instances: FontInstanceMap,
    pub output_pipelines: FastHashSet<PipelineId>,
    pub scene_id: u64,
}

#[cfg(feature = "replay")]
pub struct LoadScene {
    pub document_id: DocumentId,
    pub scene: Scene,
    pub scene_id: u64,
    pub output_pipelines: FastHashSet<PipelineId>,
    pub font_instances: FontInstanceMap,
    pub view: DocumentView,
    pub config: FrameBuilderConfig,
    pub build_frame: bool,
}

pub struct BuiltScene {
    pub scene: Scene,
    pub frame_builder: FrameBuilder,
    pub clip_scroll_tree: ClipScrollTree,
}

// Message from render backend to scene builder.
pub enum SceneBuilderRequest {
    Transaction(Box<Transaction>),
    DeleteDocument(DocumentId),
    WakeUp,
    Flush(MsgSender<()>),
    SetFrameBuilderConfig(FrameBuilderConfig),
    Stop,
    #[cfg(feature = "replay")]
    LoadScenes(Vec<LoadScene>),
}

// Message from scene builder to render backend.
pub enum SceneBuilderResult {
    Transaction(Box<BuiltTransaction>, Option<Sender<SceneSwapResult>>),
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

pub struct SceneBuilder {
    documents: FastHashMap<DocumentId, Scene>,
    rx: Receiver<SceneBuilderRequest>,
    tx: Sender<SceneBuilderResult>,
    api_tx: MsgSender<ApiMsg>,
    config: FrameBuilderConfig,
    hooks: Option<Box<SceneBuilderHooks + Send>>,
    picture_id_generator: PictureIdGenerator,
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
                documents: FastHashMap::default(),
                rx: in_rx,
                tx: out_tx,
                api_tx,
                config,
                hooks,
                picture_id_generator: PictureIdGenerator::new(),
            },
            in_tx,
            out_rx,
        )
    }

    /// The scene builder thread's event loop.
    pub fn run(&mut self) {
        if let Some(ref hooks) = self.hooks {
            hooks.register();
        }

        loop {
            match self.rx.recv() {
                Ok(SceneBuilderRequest::WakeUp) => {}
                Ok(SceneBuilderRequest::Flush(tx)) => {
                    self.tx.send(SceneBuilderResult::FlushComplete(tx)).unwrap();
                    let _ = self.api_tx.send(ApiMsg::WakeUp);
                }
                Ok(SceneBuilderRequest::Transaction(mut txn)) => {
                    let built_txn = self.process_transaction(&mut txn);
                    self.forward_built_transaction(built_txn);
                }
                Ok(SceneBuilderRequest::DeleteDocument(document_id)) => {
                    self.documents.remove(&document_id);
                }
                Ok(SceneBuilderRequest::SetFrameBuilderConfig(cfg)) => {
                    self.config = cfg;
                }
                #[cfg(feature = "replay")]
                Ok(SceneBuilderRequest::LoadScenes(msg)) => {
                    self.load_scenes(msg);
                }
                Ok(SceneBuilderRequest::Stop) => {
                    self.tx.send(SceneBuilderResult::Stopped).unwrap();
                    // We don't need to send a WakeUp to api_tx because we only
                    // get the Stop when the RenderBackend loop is exiting.
                    break;
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

    #[cfg(feature = "replay")]
    fn load_scenes(&mut self, scenes: Vec<LoadScene>) {
        for item in scenes {
            self.config = item.config;

            let scene_build_start_time = precise_time_ns();

            let mut built_scene = None;
            if item.scene.has_root_pipeline() {
                let mut clip_scroll_tree = ClipScrollTree::new();
                let mut new_scene = Scene::new();

                let frame_builder = DisplayListFlattener::create_frame_builder(
                    &item.scene,
                    &mut clip_scroll_tree,
                    item.font_instances,
                    &item.view,
                    &item.output_pipelines,
                    &self.config,
                    &mut new_scene,
                    item.scene_id,
                    &mut self.picture_id_generator,
                );

                built_scene = Some(BuiltScene {
                    scene: new_scene,
                    frame_builder,
                    clip_scroll_tree,
                });
            }

            self.documents.insert(item.document_id, item.scene);

            let txn = Box::new(BuiltTransaction {
                document_id: item.document_id,
                build_frame: true,
                render_frame: item.build_frame,
                built_scene,
                resource_updates: Vec::new(),
                rasterized_blobs: Vec::new(),
                blob_rasterizer: None,
                frame_ops: Vec::new(),
                removed_pipelines: Vec::new(),
                notifications: Vec::new(),
                scene_build_start_time,
                scene_build_end_time: precise_time_ns(),
            });

            self.forward_built_transaction(txn);
        }
    }

    /// Do the bulk of the work of the scene builder thread.
    fn process_transaction(&mut self, txn: &mut Transaction) -> Box<BuiltTransaction> {

        let scene_build_start_time = precise_time_ns();

        let scene = self.documents.entry(txn.document_id).or_insert(Scene::new());

        for update in txn.display_list_updates.drain(..) {
            scene.set_display_list(
                update.pipeline_id,
                update.epoch,
                update.built_display_list,
                update.background,
                update.viewport_size,
                update.content_size,
            );
        }

        for &(pipeline_id, epoch) in &txn.epoch_updates {
            scene.update_epoch(pipeline_id, epoch);
        }

        if let Some(id) = txn.set_root_pipeline {
            scene.set_root_pipeline_id(id);
        }

        for pipeline_id in &txn.removed_pipelines {
            scene.remove_pipeline(*pipeline_id)
        }

        let mut built_scene = None;
        if scene.has_root_pipeline() {
            if let Some(request) = txn.request_scene_build.take() {
                let mut clip_scroll_tree = ClipScrollTree::new();
                let mut new_scene = Scene::new();

                let frame_builder = DisplayListFlattener::create_frame_builder(
                    &scene,
                    &mut clip_scroll_tree,
                    request.font_instances,
                    &request.view,
                    &request.output_pipelines,
                    &self.config,
                    &mut new_scene,
                    request.scene_id,
                    &mut self.picture_id_generator,
                );

                built_scene = Some(BuiltScene {
                    scene: new_scene,
                    frame_builder,
                    clip_scroll_tree,
                });
            }
        }

        let blob_requests = replace(&mut txn.blob_requests, Vec::new());
        let mut rasterized_blobs = txn.blob_rasterizer.as_mut().map_or(
            Vec::new(),
            |rasterizer| rasterizer.rasterize(&blob_requests),
        );
        rasterized_blobs.append(&mut txn.rasterized_blobs);

        drain_filter(
            &mut txn.notifications,
            |n| { n.when() == Checkpoint::SceneBuilt },
            |n| { n.notify(); },
        );

        Box::new(BuiltTransaction {
            document_id: txn.document_id,
            build_frame: txn.build_frame || built_scene.is_some(),
            render_frame: txn.render_frame,
            built_scene,
            rasterized_blobs,
            resource_updates: replace(&mut txn.resource_updates, Vec::new()),
            blob_rasterizer: replace(&mut txn.blob_rasterizer, None),
            frame_ops: replace(&mut txn.frame_ops, Vec::new()),
            removed_pipelines: replace(&mut txn.removed_pipelines, Vec::new()),
            notifications: replace(&mut txn.notifications, Vec::new()),
            scene_build_start_time,
            scene_build_end_time: precise_time_ns(),
        })
    }

    /// Send the result of process_transaction back to the render backend.
    fn forward_built_transaction(&mut self, txn: Box<BuiltTransaction>) {
        // We only need the pipeline info and the result channel if we
        // have a hook callback *and* if this transaction actually built
        // a new scene that is going to get swapped in. In other cases
        // pipeline_info can be None and we can avoid some overhead from
        // invoking the hooks and blocking on the channel.
        let (pipeline_info, result_tx, result_rx) = match (&self.hooks, &txn.built_scene) {
            (&Some(ref hooks), &Some(ref built)) => {
                let info = PipelineInfo {
                    epochs: built.scene.pipeline_epochs.clone(),
                    removed_pipelines: txn.removed_pipelines.clone(),
                };
                let (tx, rx) = channel();

                hooks.pre_scene_swap(txn.scene_build_end_time - txn.scene_build_start_time);

                (Some(info), Some(tx), Some(rx))
            }
            _ => (None, None, None),
        };

        let scene_swap_start_time = precise_time_ns();
        let has_resources_updates = !txn.resource_updates.is_empty();

        self.tx.send(SceneBuilderResult::Transaction(txn, result_tx)).unwrap();

        let _ = self.api_tx.send(ApiMsg::WakeUp);

        if let Some(pipeline_info) = pipeline_info {
            // Block until the swap is done, then invoke the hook.
            let swap_result = result_rx.unwrap().recv();
            let scene_swap_time = precise_time_ns() - scene_swap_start_time;
            self.hooks.as_ref().unwrap().post_scene_swap(pipeline_info, scene_swap_time);
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
}

/// A scene builder thread which executes expensive operations such as blob rasterization
/// with a lower priority than the normal scene builder thread.
///
/// After rasterizing blobs, the secene building request is forwarded to the normal scene
/// builder where the FrameBuilder is generated.
pub struct LowPrioritySceneBuilder {
    pub rx: Receiver<SceneBuilderRequest>,
    pub tx: Sender<SceneBuilderRequest>,
}

impl LowPrioritySceneBuilder {
    pub fn run(&mut self) {
        loop {
            match self.rx.recv() {
                Ok(SceneBuilderRequest::Transaction(txn)) => {
                    let txn = self.process_transaction(txn);
                    self.tx.send(SceneBuilderRequest::Transaction(txn)).unwrap();
                }
                Ok(SceneBuilderRequest::DeleteDocument(document_id)) => {
                    self.tx.send(SceneBuilderRequest::DeleteDocument(document_id)).unwrap();
                }
                Ok(SceneBuilderRequest::Stop) => {
                    self.tx.send(SceneBuilderRequest::Stop).unwrap();
                    break;
                }
                Ok(other) => {
                    self.tx.send(other).unwrap();
                }
                Err(_) => {
                    break;
                }
            }
        }
    }

    fn process_transaction(&mut self, mut txn: Box<Transaction>) -> Box<Transaction> {
        let blob_requests = replace(&mut txn.blob_requests, Vec::new());
        let mut more_rasterized_blobs = txn.blob_rasterizer.as_mut().map_or(
            Vec::new(),
            |rasterizer| rasterizer.rasterize(&blob_requests),
        );
        txn.rasterized_blobs.append(&mut more_rasterized_blobs);

        txn
    }
}

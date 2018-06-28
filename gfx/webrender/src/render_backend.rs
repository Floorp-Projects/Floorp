/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ApiMsg, BuiltDisplayList, ClearCache, DebugCommand};
#[cfg(feature = "debugger")]
use api::{BuiltDisplayListIter, SpecificDisplayItem};
use api::{DeviceIntPoint, DevicePixelScale, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{DocumentId, DocumentLayer, ExternalScrollId, FrameMsg, HitTestFlags, HitTestResult};
use api::{IdNamespace, LayoutPoint, PipelineId, RenderNotifier, SceneMsg, ScrollClamping};
use api::{ScrollLocation, ScrollNodeState, TransactionMsg};
use api::channel::{MsgReceiver, Payload};
#[cfg(feature = "capture")]
use api::CaptureBits;
#[cfg(feature = "replay")]
use api::CapturedDocument;
use clip_scroll_tree::{ClipScrollNodeIndex, ClipScrollTree};
#[cfg(feature = "debugger")]
use debug_server;
use display_list_flattener::DisplayListFlattener;
use frame_builder::{FrameBuilder, FrameBuilderConfig};
use gpu_cache::GpuCache;
use hit_test::{HitTest, HitTester};
use internal_types::{DebugOutput, FastHashMap, FastHashSet, RenderedDocument, ResultMsg};
use profiler::{BackendProfileCounters, IpcProfileCounters, ResourceProfileCounters};
use record::ApiRecordingReceiver;
use renderer::{AsyncPropertySampler, PipelineInfo};
use resource_cache::ResourceCache;
#[cfg(feature = "replay")]
use resource_cache::PlainCacheOwn;
#[cfg(any(feature = "capture", feature = "replay"))]
use resource_cache::PlainResources;
use scene::{Scene, SceneProperties};
use scene_builder::*;
#[cfg(feature = "serialize")]
use serde::{Serialize, Deserialize};
#[cfg(feature = "debugger")]
use serde_json;
#[cfg(any(feature = "capture", feature = "replay"))]
use std::path::PathBuf;
use std::sync::atomic::{ATOMIC_USIZE_INIT, AtomicUsize, Ordering};
use std::mem::replace;
use std::sync::mpsc::{channel, Sender, Receiver};
use std::u32;
#[cfg(feature = "replay")]
use tiling::Frame;
use time::precise_time_ns;

#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone)]
pub struct DocumentView {
    pub window_size: DeviceUintSize,
    pub inner_rect: DeviceUintRect,
    pub layer: DocumentLayer,
    pub pan: DeviceIntPoint,
    pub device_pixel_ratio: f32,
    pub page_zoom_factor: f32,
    pub pinch_zoom_factor: f32,
}

impl DocumentView {
    pub fn accumulated_scale_factor(&self) -> DevicePixelScale {
        DevicePixelScale::new(
            self.device_pixel_ratio *
            self.page_zoom_factor *
            self.pinch_zoom_factor
        )
    }
}

struct SceneData {
    scene: Scene,
    removed_pipelines: Vec<PipelineId>,
}

#[derive(Copy, Clone, Hash, PartialEq, PartialOrd, Debug, Eq, Ord)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameId(pub u32);

struct Document {
    // The latest built scene, usable to build frames.
    // received from the scene builder thread.
    current: SceneData,
    // The scene with the latest transactions applied, not necessarily built yet.
    // what we will send to the scene builder.
    pending: SceneData,

    view: DocumentView,

    /// The ClipScrollTree for this document which tracks both ClipScrollNodes and ClipChains.
    /// This is stored here so that we are able to preserve scrolling positions between
    /// rendered frames.
    clip_scroll_tree: ClipScrollTree,

    /// The id of the current frame.
    frame_id: FrameId,

    /// A configuration object for the FrameBuilder that we produce.
    frame_builder_config: FrameBuilderConfig,

    // the `Option` here is only to deal with borrow checker
    frame_builder: Option<FrameBuilder>,
    // A set of pipelines that the caller has requested be
    // made available as output textures.
    output_pipelines: FastHashSet<PipelineId>,
    // A helper switch to prevent any frames rendering triggered by scrolling
    // messages between `SetDisplayList` and `GenerateFrame`.
    // If we allow them, then a reftest that scrolls a few layers before generating
    // the first frame would produce inconsistent rendering results, because
    // scroll events are not necessarily received in deterministic order.
    render_on_scroll: Option<bool>,

    /// A data structure to allow hit testing against rendered frames. This is updated
    /// every time we produce a fully rendered frame.
    hit_tester: Option<HitTester>,

    /// Properties that are resolved during frame building and can be changed at any time
    /// without requiring the scene to be re-built.
    dynamic_properties: SceneProperties,
}

impl Document {
    pub fn new(
        frame_builder_config: FrameBuilderConfig,
        window_size: DeviceUintSize,
        layer: DocumentLayer,
        enable_render_on_scroll: bool,
        default_device_pixel_ratio: f32,
    ) -> Self {
        let render_on_scroll = if enable_render_on_scroll {
            Some(false)
        } else {
            None
        };
        Document {
            current: SceneData {
                scene: Scene::new(),
                removed_pipelines: Vec::new(),
            },
            pending: SceneData {
                scene: Scene::new(),
                removed_pipelines: Vec::new(),
            },
            view: DocumentView {
                window_size,
                inner_rect: DeviceUintRect::new(DeviceUintPoint::zero(), window_size),
                layer,
                pan: DeviceIntPoint::zero(),
                page_zoom_factor: 1.0,
                pinch_zoom_factor: 1.0,
                device_pixel_ratio: default_device_pixel_ratio,
            },
            clip_scroll_tree: ClipScrollTree::new(),
            frame_id: FrameId(0),
            frame_builder_config,
            frame_builder: None,
            output_pipelines: FastHashSet::default(),
            render_on_scroll,
            hit_tester: None,
            dynamic_properties: SceneProperties::new(),
        }
    }

    fn can_render(&self) -> bool { self.frame_builder.is_some() }

    fn has_pixels(&self) -> bool {
        !self.view.window_size.is_empty_or_negative()
    }

    // TODO: We will probably get rid of this soon and always forward to the scene building thread.
    fn build_scene(&mut self, resource_cache: &mut ResourceCache, scene_id: u64) {
        let max_texture_size = resource_cache.max_texture_size();

        if self.view.window_size.width > max_texture_size ||
           self.view.window_size.height > max_texture_size {
            error!("ERROR: Invalid window dimensions {}x{}. Please call api.set_window_size()",
                self.view.window_size.width,
                self.view.window_size.height,
            );

            return;
        }

        let old_builder = self.frame_builder.take().unwrap_or_else(FrameBuilder::empty);
        let root_pipeline_id = match self.pending.scene.root_pipeline_id {
            Some(root_pipeline_id) => root_pipeline_id,
            None => return,
        };

        if !self.pending.scene.pipelines.contains_key(&root_pipeline_id) {
            return;
        }

        // The DisplayListFlattener  re-create the up-to-date current scene's pipeline epoch
        // map and clip scroll tree from the information in the pending scene.
        self.current.scene.pipeline_epochs.clear();
        let old_scrolling_states = self.clip_scroll_tree.drain();

        let frame_builder = DisplayListFlattener::create_frame_builder(
            old_builder,
            &self.pending.scene,
            &mut self.clip_scroll_tree,
            resource_cache.get_font_instances(),
            &self.view,
            &self.output_pipelines,
            &self.frame_builder_config,
            &mut self.current.scene,
            scene_id,
        );

        self.clip_scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);

        if !self.current.removed_pipelines.is_empty() {
            warn!("Built the scene several times without rendering it.");
        }

        self.current.removed_pipelines.extend(self.pending.removed_pipelines.drain(..));
        self.frame_builder = Some(frame_builder);

        // Advance to the next frame.
        self.frame_id.0 += 1;
    }

    fn forward_transaction_to_scene_builder(
        &mut self,
        transaction_msg: TransactionMsg,
        document_ops: &DocumentOps,
        document_id: DocumentId,
        scene_id: u64,
        resource_cache: &ResourceCache,
        scene_tx: &Sender<SceneBuilderRequest>,
    ) {
        // Do as much of the error handling as possible here before dispatching to
        // the scene builder thread.
        let build_scene: bool = document_ops.build
            && self.pending.scene.root_pipeline_id.map(
                |id| { self.pending.scene.pipelines.contains_key(&id) }
            ).unwrap_or(false);

        let scene_request = if build_scene {
            Some(SceneRequest {
                scene: self.pending.scene.clone(),
                removed_pipelines: replace(&mut self.pending.removed_pipelines, Vec::new()),
                view: self.view.clone(),
                font_instances: resource_cache.get_font_instances(),
                output_pipelines: self.output_pipelines.clone(),
                scene_id,
            })
        } else {
            None
        };

        scene_tx.send(SceneBuilderRequest::Transaction {
            scene: scene_request,
            resource_updates: transaction_msg.resource_updates,
            frame_ops: transaction_msg.frame_ops,
            render: transaction_msg.generate_frame,
            document_id,
        }).unwrap();
    }

    fn render(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        resource_profile: &mut ResourceProfileCounters,
        is_new_scene: bool,
    ) -> RenderedDocument {
        let accumulated_scale_factor = self.view.accumulated_scale_factor();
        let pan = self.view.pan.to_f32() / accumulated_scale_factor;

        let frame = {
            let frame_builder = self.frame_builder.as_mut().unwrap();
            let frame = frame_builder.build(
                resource_cache,
                gpu_cache,
                self.frame_id,
                &mut self.clip_scroll_tree,
                &self.current.scene.pipelines,
                accumulated_scale_factor,
                self.view.layer,
                pan,
                &mut resource_profile.texture_cache,
                &mut resource_profile.gpu_cache,
                &self.dynamic_properties,
            );
            self.hit_tester = Some(frame_builder.create_hit_tester(&self.clip_scroll_tree));
            frame
        };

        RenderedDocument {
            frame,
            is_new_scene,
        }
    }

    pub fn updated_pipeline_info(&mut self) -> PipelineInfo {
        let removed_pipelines = replace(&mut self.current.removed_pipelines, Vec::new());
        PipelineInfo {
            epochs: self.current.scene.pipeline_epochs.clone(),
            removed_pipelines,
        }
    }

    pub fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.clip_scroll_tree
            .discard_frame_state_for_pipeline(pipeline_id);
    }

    /// Returns true if any nodes actually changed position or false otherwise.
    pub fn scroll_nearest_scrolling_ancestor(
        &mut self,
        scroll_location: ScrollLocation,
        scroll_node_index: Option<ClipScrollNodeIndex>,
    ) -> bool {
        self.clip_scroll_tree.scroll_nearest_scrolling_ancestor(scroll_location, scroll_node_index)
    }

    /// Returns true if the node actually changed position or false otherwise.
    pub fn scroll_node(
        &mut self,
        origin: LayoutPoint,
        id: ExternalScrollId,
        clamp: ScrollClamping
    ) -> bool {
        self.clip_scroll_tree.scroll_node(origin, id, clamp)
    }

    pub fn get_scroll_node_state(&self) -> Vec<ScrollNodeState> {
        self.clip_scroll_tree.get_scroll_node_state()
    }

    pub fn new_async_scene_ready(&mut self, mut built_scene: BuiltScene) {
        self.current.scene = built_scene.scene;

        self.frame_builder = Some(built_scene.frame_builder);
        self.current.removed_pipelines.extend(built_scene.removed_pipelines.drain(..));

        let old_scrolling_states = self.clip_scroll_tree.drain();
        self.clip_scroll_tree = built_scene.clip_scroll_tree;
        self.clip_scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);

        // Advance to the next frame.
        self.frame_id.0 += 1;
    }
}

struct DocumentOps {
    scroll: bool,
    build: bool,
    render: bool,
    composite: bool,
}

impl DocumentOps {
    fn nop() -> Self {
        DocumentOps {
            scroll: false,
            build: false,
            render: false,
            composite: false,
        }
    }

    fn build() -> Self {
        DocumentOps {
            build: true,
            ..DocumentOps::nop()
        }
    }

    fn render() -> Self {
        DocumentOps {
            render: true,
            ..DocumentOps::nop()
        }
    }

    fn combine(&mut self, other: Self) {
        self.scroll = self.scroll || other.scroll;
        self.build = self.build || other.build;
        self.render = self.render || other.render;
        self.composite = self.composite || other.composite;
    }
}

/// The unique id for WR resource identification.
static NEXT_NAMESPACE_ID: AtomicUsize = ATOMIC_USIZE_INIT;

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PlainRenderBackend {
    default_device_pixel_ratio: f32,
    enable_render_on_scroll: bool,
    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, DocumentView>,
    resources: PlainResources,
    last_scene_id: u64,
}

/// The render backend is responsible for transforming high level display lists into
/// GPU-friendly work which is then submitted to the renderer in the form of a frame::Frame.
///
/// The render backend operates on its own thread.
pub struct RenderBackend {
    api_rx: MsgReceiver<ApiMsg>,
    payload_rx: Receiver<Payload>,
    result_tx: Sender<ResultMsg>,
    scene_tx: Sender<SceneBuilderRequest>,
    scene_rx: Receiver<SceneBuilderResult>,

    payload_buffer: Vec<Payload>,

    default_device_pixel_ratio: f32,

    gpu_cache: GpuCache,
    resource_cache: ResourceCache,

    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, Document>,

    notifier: Box<RenderNotifier>,
    recorder: Option<Box<ApiRecordingReceiver>>,
    sampler: Option<Box<AsyncPropertySampler + Send>>,

    last_scene_id: u64,
    enable_render_on_scroll: bool,
}

impl RenderBackend {
    pub fn new(
        api_rx: MsgReceiver<ApiMsg>,
        payload_rx: Receiver<Payload>,
        result_tx: Sender<ResultMsg>,
        scene_tx: Sender<SceneBuilderRequest>,
        scene_rx: Receiver<SceneBuilderResult>,
        default_device_pixel_ratio: f32,
        resource_cache: ResourceCache,
        notifier: Box<RenderNotifier>,
        frame_config: FrameBuilderConfig,
        recorder: Option<Box<ApiRecordingReceiver>>,
        sampler: Option<Box<AsyncPropertySampler + Send>>,
        enable_render_on_scroll: bool,
    ) -> RenderBackend {
        // The namespace_id should start from 1.
        NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed);

        RenderBackend {
            api_rx,
            payload_rx,
            result_tx,
            scene_tx,
            scene_rx,
            payload_buffer: Vec::new(),
            default_device_pixel_ratio,
            resource_cache,
            gpu_cache: GpuCache::new(),
            frame_config,
            documents: FastHashMap::default(),
            notifier,
            recorder,
            sampler,
            last_scene_id: 0,
            enable_render_on_scroll,
        }
    }

    fn process_scene_msg(
        &mut self,
        document_id: DocumentId,
        message: SceneMsg,
        frame_counter: u32,
        ipc_profile_counters: &mut IpcProfileCounters,
    ) -> DocumentOps {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            SceneMsg::UpdateEpoch(pipeline_id, epoch) => {
                doc.pending.scene.update_epoch(pipeline_id, epoch);

                DocumentOps::nop()
            }
            SceneMsg::SetPageZoom(factor) => {
                doc.view.page_zoom_factor = factor.get();
                DocumentOps::nop()
            }
            SceneMsg::SetPinchZoom(factor) => {
                doc.view.pinch_zoom_factor = factor.get();
                DocumentOps::nop()
            }
            SceneMsg::SetWindowParameters {
                window_size,
                inner_rect,
                device_pixel_ratio,
            } => {
                doc.view.window_size = window_size;
                doc.view.inner_rect = inner_rect;
                doc.view.device_pixel_ratio = device_pixel_ratio;
                DocumentOps::nop()
            }
            SceneMsg::SetDisplayList {
                epoch,
                pipeline_id,
                background,
                viewport_size,
                content_size,
                list_descriptor,
                preserve_frame_state,
            } => {
                profile_scope!("SetDisplayList");

                let data = if let Some(idx) = self.payload_buffer.iter().position(|data|
                    data.epoch == epoch && data.pipeline_id == pipeline_id
                ) {
                    self.payload_buffer.swap_remove(idx)
                } else {
                    loop {
                        let data = self.payload_rx.recv().unwrap();
                        if data.epoch == epoch && data.pipeline_id == pipeline_id {
                            break data;
                        } else {
                            self.payload_buffer.push(data);
                        }
                    }
                };

                if let Some(ref mut r) = self.recorder {
                    r.write_payload(frame_counter, &data.to_data());
                }

                let built_display_list =
                    BuiltDisplayList::from_data(data.display_list_data, list_descriptor);

                if !preserve_frame_state {
                    doc.discard_frame_state_for_pipeline(pipeline_id);
                }

                let display_list_len = built_display_list.data().len();
                let (builder_start_time, builder_finish_time, send_start_time) =
                    built_display_list.times();
                let display_list_received_time = precise_time_ns();

                {
                    doc.pending.scene.set_display_list(
                        pipeline_id,
                        epoch,
                        built_display_list,
                        background,
                        viewport_size,
                        content_size,
                    );
                }

                if let Some(ref mut ros) = doc.render_on_scroll {
                    *ros = false; //wait for `GenerateFrame`
                }

                // Note: this isn't quite right as auxiliary values will be
                // pulled out somewhere in the prim_store, but aux values are
                // really simple and cheap to access, so it's not a big deal.
                let display_list_consumed_time = precise_time_ns();

                ipc_profile_counters.set(
                    builder_start_time,
                    builder_finish_time,
                    send_start_time,
                    display_list_received_time,
                    display_list_consumed_time,
                    display_list_len,
                );

                DocumentOps::build()
            }
            SceneMsg::SetRootPipeline(pipeline_id) => {
                profile_scope!("SetRootPipeline");

                doc.pending.scene.set_root_pipeline_id(pipeline_id);
                if doc.pending.scene.pipelines.get(&pipeline_id).is_some() {
                    DocumentOps::build()
                } else {
                    DocumentOps::nop()
                }
            }
            SceneMsg::RemovePipeline(pipeline_id) => {
                profile_scope!("RemovePipeline");

                doc.pending.scene.remove_pipeline(pipeline_id);
                doc.pending.removed_pipelines.push(pipeline_id);
                DocumentOps::nop()
            }
        }
    }

    fn process_frame_msg(
        &mut self,
        document_id: DocumentId,
        message: FrameMsg,
    ) -> DocumentOps {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            FrameMsg::UpdateEpoch(pipeline_id, epoch) => {
                doc.current.scene.update_epoch(pipeline_id, epoch);

                DocumentOps::nop()
            }
            FrameMsg::EnableFrameOutput(pipeline_id, enable) => {
                if enable {
                    doc.output_pipelines.insert(pipeline_id);
                } else {
                    doc.output_pipelines.remove(&pipeline_id);
                }
                DocumentOps::nop()
            }
            FrameMsg::Scroll(delta, cursor) => {
                profile_scope!("Scroll");

                let mut should_render = true;
                let node_index = match doc.hit_tester {
                    Some(ref hit_tester) => {
                        // Ideally we would call doc.scroll_nearest_scrolling_ancestor here, but
                        // we need have to avoid a double-borrow.
                        let test = HitTest::new(None, cursor, HitTestFlags::empty());
                        hit_tester.find_node_under_point(test)
                    }
                    None => {
                        should_render = false;
                        None
                    }
                };

                let should_render =
                    should_render &&
                    doc.scroll_nearest_scrolling_ancestor(delta, node_index) &&
                    doc.render_on_scroll == Some(true);
                DocumentOps {
                    scroll: true,
                    render: should_render,
                    composite: should_render,
                    ..DocumentOps::nop()
                }
            }
            FrameMsg::HitTest(pipeline_id, point, flags, tx) => {

                let result = match doc.hit_tester {
                    Some(ref hit_tester) => {
                        hit_tester.hit_test(HitTest::new(pipeline_id, point, flags))
                    }
                    None => HitTestResult { items: Vec::new() },
                };

                tx.send(result).unwrap();
                DocumentOps::nop()
            }
            FrameMsg::SetPan(pan) => {
                doc.view.pan = pan;
                DocumentOps::nop()
            }
            FrameMsg::ScrollNodeWithId(origin, id, clamp) => {
                profile_scope!("ScrollNodeWithScrollId");

                let should_render = doc.scroll_node(origin, id, clamp)
                    && doc.render_on_scroll == Some(true);

                DocumentOps {
                    scroll: true,
                    render: should_render,
                    composite: should_render,
                    ..DocumentOps::nop()
                }
            }
            FrameMsg::GetScrollNodeState(tx) => {
                profile_scope!("GetScrollNodeState");
                tx.send(doc.get_scroll_node_state()).unwrap();
                DocumentOps::nop()
            }
            FrameMsg::UpdateDynamicProperties(property_bindings) => {
                doc.dynamic_properties.set_properties(property_bindings);
                DocumentOps::render()
            }
            FrameMsg::AppendDynamicProperties(property_bindings) => {
                doc.dynamic_properties.add_properties(property_bindings);
                DocumentOps::render()
            }
        }
    }

    fn next_namespace_id(&self) -> IdNamespace {
        IdNamespace(NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed) as u32)
    }

    pub fn make_unique_scene_id(&mut self) -> u64 {
        // 2^64 scenes ought to be enough for anybody!
        self.last_scene_id += 1;
        self.last_scene_id
    }

    pub fn run(&mut self, mut profile_counters: BackendProfileCounters) {
        let mut frame_counter: u32 = 0;
        let mut keep_going = true;

        if let Some(ref sampler) = self.sampler {
            sampler.register();
        }

        while keep_going {
            profile_scope!("handle_msg");

            while let Ok(msg) = self.scene_rx.try_recv() {
                match msg {
                    SceneBuilderResult::Transaction {
                        document_id,
                        mut built_scene,
                        resource_updates,
                        frame_ops,
                        render,
                        result_tx,
                    } => {
                        let mut ops = DocumentOps::nop();
                        if let Some(doc) = self.documents.get_mut(&document_id) {
                            if let Some(mut built_scene) = built_scene.take() {
                                doc.new_async_scene_ready(built_scene);
                                // After applying the new scene we need to
                                // rebuild the hit-tester, so we trigger a render
                                // step.
                                ops = DocumentOps::render();
                            }
                            if let Some(tx) = result_tx {
                                let (resume_tx, resume_rx) = channel();
                                tx.send(SceneSwapResult::Complete(resume_tx)).unwrap();
                                // Block until the post-swap hook has completed on
                                // the scene builder thread. We need to do this before
                                // we can sample from the sampler hook which might happen
                                // in the update_document call below.
                                resume_rx.recv().ok();
                            }
                        } else {
                            // The document was removed while we were building it, skip it.
                            // TODO: we might want to just ensure that removed documents are
                            // always forwarded to the scene builder thread to avoid this case.
                            if let Some(tx) = result_tx {
                                tx.send(SceneSwapResult::Aborted).unwrap();
                            }
                            continue;
                        }

                        let transaction_msg = TransactionMsg {
                            scene_ops: Vec::new(),
                            frame_ops,
                            resource_updates,
                            generate_frame: render,
                            use_scene_builder_thread: false,
                        };

                        if !transaction_msg.is_empty() || ops.render {
                            self.update_document(
                                document_id,
                                transaction_msg,
                                &mut frame_counter,
                                &mut profile_counters,
                                ops,
                                true,
                            );
                        }
                    },
                    SceneBuilderResult::FlushComplete(tx) => {
                        tx.send(()).ok();
                    }
                    SceneBuilderResult::Stopped => {
                        panic!("We haven't sent a Stop yet, how did we get a Stopped back?");
                    }
                }
            }

            keep_going = match self.api_rx.recv() {
                Ok(msg) => {
                    if let Some(ref mut r) = self.recorder {
                        r.write_msg(frame_counter, &msg);
                    }
                    self.process_api_msg(msg, &mut profile_counters, &mut frame_counter)
                }
                Err(..) => { false }
            };
        }

        let _ = self.scene_tx.send(SceneBuilderRequest::Stop);
        // Ensure we read everything the scene builder is sending us from
        // inflight messages, otherwise the scene builder might panic.
        while let Ok(msg) = self.scene_rx.recv() {
            match msg {
                SceneBuilderResult::FlushComplete(tx) => {
                    // If somebody's blocked waiting for a flush, how did they
                    // trigger the RB thread to shut down? This shouldn't happen
                    // but handle it gracefully anyway.
                    debug_assert!(false);
                    tx.send(()).ok();
                }
                SceneBuilderResult::Stopped => break,
                _ => continue,
            }
        }

        self.notifier.shut_down();

        if let Some(ref sampler) = self.sampler {
            sampler.deregister();
        }

    }

    fn process_api_msg(
        &mut self,
        msg: ApiMsg,
        profile_counters: &mut BackendProfileCounters,
        frame_counter: &mut u32,
    ) -> bool {
        match msg {
            ApiMsg::WakeUp => {}
            ApiMsg::WakeSceneBuilder => {
                self.scene_tx.send(SceneBuilderRequest::WakeUp).unwrap();
            }
            ApiMsg::FlushSceneBuilder(tx) => {
                self.scene_tx.send(SceneBuilderRequest::Flush(tx)).unwrap();
            }
            ApiMsg::UpdateResources(updates) => {
                self.resource_cache
                    .update_resources(updates, &mut profile_counters.resources);
            }
            ApiMsg::GetGlyphDimensions(instance_key, glyph_indices, tx) => {
                let mut glyph_dimensions = Vec::with_capacity(glyph_indices.len());
                if let Some(font) = self.resource_cache.get_font_instance(instance_key) {
                    for glyph_index in &glyph_indices {
                        let glyph_dim = self.resource_cache.get_glyph_dimensions(&font, *glyph_index);
                        glyph_dimensions.push(glyph_dim);
                    }
                }
                tx.send(glyph_dimensions).unwrap();
            }
            ApiMsg::GetGlyphIndices(font_key, text, tx) => {
                let mut glyph_indices = Vec::new();
                for ch in text.chars() {
                    let index = self.resource_cache.get_glyph_index(font_key, ch);
                    glyph_indices.push(index);
                }
                tx.send(glyph_indices).unwrap();
            }
            ApiMsg::CloneApi(sender) => {
                sender.send(self.next_namespace_id()).unwrap();
            }
            ApiMsg::AddDocument(document_id, initial_size, layer) => {
                let document = Document::new(
                    self.frame_config.clone(),
                    initial_size,
                    layer,
                    self.enable_render_on_scroll,
                    self.default_device_pixel_ratio,
                );
                self.documents.insert(document_id, document);
            }
            ApiMsg::DeleteDocument(document_id) => {
                self.documents.remove(&document_id);
            }
            ApiMsg::ExternalEvent(evt) => {
                self.notifier.external_event(evt);
            }
            ApiMsg::ClearNamespace(namespace_id) => {
                self.resource_cache.clear_namespace(namespace_id);
                let document_ids = self.documents
                    .keys()
                    .filter(|did| did.0 == namespace_id)
                    .cloned()
                    .collect::<Vec<_>>();
                for document in document_ids {
                    self.documents.remove(&document);
                }
            }
            ApiMsg::MemoryPressure => {
                // This is drastic. It will basically flush everything out of the cache,
                // and the next frame will have to rebuild all of its resources.
                // We may want to look into something less extreme, but on the other hand this
                // should only be used in situations where are running low enough on memory
                // that we risk crashing if we don't do something about it.
                // The advantage of clearing the cache completely is that it gets rid of any
                // remaining fragmentation that could have persisted if we kept around the most
                // recently used resources.
                self.resource_cache.clear(ClearCache::all());

                let pending_update = self.resource_cache.pending_updates();
                let msg = ResultMsg::UpdateResources {
                    updates: pending_update,
                    cancel_rendering: true,
                };
                self.result_tx.send(msg).unwrap();
                self.notifier.wake_up();
            }
            ApiMsg::DebugCommand(option) => {
                let msg = match option {
                    DebugCommand::EnableDualSourceBlending(enable) => {
                        // Set in the config used for any future documents
                        // that are created.
                        self.frame_config
                            .dual_source_blending_is_enabled = enable;

                        // Set for any existing documents.
                        for (_, doc) in &mut self.documents {
                            doc.frame_builder_config .dual_source_blending_is_enabled = enable;
                        }

                        // We don't want to forward this message to the renderer.
                        return true;
                    }
                    DebugCommand::FetchDocuments => {
                        let json = self.get_docs_for_debugger();
                        ResultMsg::DebugOutput(DebugOutput::FetchDocuments(json))
                    }
                    DebugCommand::FetchClipScrollTree => {
                        let json = self.get_clip_scroll_tree_for_debugger();
                        ResultMsg::DebugOutput(DebugOutput::FetchClipScrollTree(json))
                    }
                    #[cfg(feature = "capture")]
                    DebugCommand::SaveCapture(root, bits) => {
                        let output = self.save_capture(root, bits, profile_counters);
                        ResultMsg::DebugOutput(output)
                    },
                    #[cfg(feature = "replay")]
                    DebugCommand::LoadCapture(root, tx) => {
                        NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed);
                        *frame_counter += 1;

                        self.load_capture(&root, profile_counters);

                        for (id, doc) in &self.documents {
                            let captured = CapturedDocument {
                                document_id: *id,
                                root_pipeline_id: doc.current.scene.root_pipeline_id,
                                window_size: doc.view.window_size,
                            };
                            tx.send(captured).unwrap();
                        }
                        // Note: we can't pass `LoadCapture` here since it needs to arrive
                        // before the `PublishDocument` messages sent by `load_capture`.
                        return true;
                    }
                    DebugCommand::ClearCaches(mask) => {
                        self.resource_cache.clear(mask);
                        return true;
                    }
                    _ => ResultMsg::DebugCommand(option),
                };
                self.result_tx.send(msg).unwrap();
                self.notifier.wake_up();
            }
            ApiMsg::ShutDown => {
                return false;
            }
            ApiMsg::UpdateDocument(document_id, doc_msgs) => {
                self.update_document(
                    document_id,
                    doc_msgs,
                    frame_counter,
                    profile_counters,
                    DocumentOps::nop(),
                    false,
                )
            }
        }

        true
    }

    fn update_document(
        &mut self,
        document_id: DocumentId,
        mut transaction_msg: TransactionMsg,
        frame_counter: &mut u32,
        profile_counters: &mut BackendProfileCounters,
        initial_op: DocumentOps,
        has_built_scene: bool,
    ) {
        let mut op = initial_op;

        for scene_msg in transaction_msg.scene_ops.drain(..) {
            let _timer = profile_counters.total_time.timer();
            op.combine(
                self.process_scene_msg(
                    document_id,
                    scene_msg,
                    *frame_counter,
                    &mut profile_counters.ipc,
                )
            );
        }

        if transaction_msg.use_scene_builder_thread {
            let scene_id = self.make_unique_scene_id();
            let doc = self.documents.get_mut(&document_id).unwrap();

            doc.forward_transaction_to_scene_builder(
                transaction_msg,
                &op,
                document_id,
                scene_id,
                &self.resource_cache,
                &self.scene_tx,
            );

            return;
        }

        self.resource_cache.update_resources(
            transaction_msg.resource_updates,
            &mut profile_counters.resources,
        );

        if op.build {
            let scene_id = self.make_unique_scene_id();
            let doc = self.documents.get_mut(&document_id).unwrap();
            let _timer = profile_counters.total_time.timer();
            profile_scope!("build scene");

            doc.build_scene(&mut self.resource_cache, scene_id);
        }

        // If we have a sampler, get more frame ops from it and add them
        // to the transaction. This is a hook to allow the WR user code to
        // fiddle with things after a potentially long scene build, but just
        // before rendering. This is useful for rendering with the latest
        // async transforms.
        if op.render || transaction_msg.generate_frame {
            if let Some(ref sampler) = self.sampler {
                transaction_msg.frame_ops.append(&mut sampler.sample());
            }
        }

        for frame_msg in transaction_msg.frame_ops {
            let _timer = profile_counters.total_time.timer();
            op.combine(self.process_frame_msg(document_id, frame_msg));
        }

        let doc = self.documents.get_mut(&document_id).unwrap();

        if transaction_msg.generate_frame {
            if let Some(ref mut ros) = doc.render_on_scroll {
                *ros = true;
            }

            if doc.current.scene.root_pipeline_id.is_some() {
                op.render = true;
                op.composite = true;
            }
        }

        if !doc.can_render() {
            // TODO: this happens if we are building the first scene asynchronously and
            // scroll at the same time. we should keep track of the fact that we skipped
            // composition here and do it as soon as we receive the scene.
            op.render = false;
            op.composite = false;
        }

        debug_assert!(op.render || !op.composite);

        let mut render_time = None;
        if op.render && doc.has_pixels() {
            profile_scope!("generate frame");

            *frame_counter += 1;

            // borrow ck hack for profile_counters
            let (pending_update, rendered_document) = {
                let _timer = profile_counters.total_time.timer();
                let render_start_time = precise_time_ns();

                let rendered_document = doc.render(
                    &mut self.resource_cache,
                    &mut self.gpu_cache,
                    &mut profile_counters.resources,
                    op.build || has_built_scene,
                );

                debug!("generated frame for document {:?} with {} passes",
                    document_id, rendered_document.frame.passes.len());

                let msg = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
                self.result_tx.send(msg).unwrap();

                render_time = Some(precise_time_ns() - render_start_time);

                let pending_update = self.resource_cache.pending_updates();
                (pending_update, rendered_document)
            };

            let msg = ResultMsg::PublishPipelineInfo(doc.updated_pipeline_info());
            self.result_tx.send(msg).unwrap();

            // Publish the frame
            let msg = ResultMsg::PublishDocument(
                document_id,
                rendered_document,
                pending_update,
                profile_counters.clone()
            );
            self.result_tx.send(msg).unwrap();
            profile_counters.reset();
        } else if op.render {
            // WR-internal optimization to avoid doing a bunch of render work if
            // there's no pixels. We still want to pretend to render and request
            // a composite to make sure that the callbacks (particularly the
            // new_frame_ready callback below) has the right flags.
            let msg = ResultMsg::PublishPipelineInfo(doc.updated_pipeline_info());
            self.result_tx.send(msg).unwrap();
        }

        if transaction_msg.generate_frame {
            self.notifier.new_frame_ready(document_id, op.scroll, op.composite, render_time);
        }
    }

    #[cfg(not(feature = "debugger"))]
    fn get_docs_for_debugger(&self) -> String {
        String::new()
    }

    #[cfg(feature = "debugger")]
    fn traverse_items<'a>(
        &self,
        traversal: &mut BuiltDisplayListIter<'a>,
        node: &mut debug_server::TreeNode,
    ) {
        loop {
            let subtraversal = {
                let item = match traversal.next() {
                    Some(item) => item,
                    None => break,
                };

                match *item.item() {
                    display_item @ SpecificDisplayItem::PushStackingContext(..) => {
                        let mut subtraversal = item.sub_iter();
                        let mut child_node =
                            debug_server::TreeNode::new(&display_item.debug_string());
                        self.traverse_items(&mut subtraversal, &mut child_node);
                        node.add_child(child_node);
                        Some(subtraversal)
                    }
                    SpecificDisplayItem::PopStackingContext => {
                        return;
                    }
                    display_item => {
                        node.add_item(&display_item.debug_string());
                        None
                    }
                }
            };

            // If flatten_item created a sub-traversal, we need `traversal` to have the
            // same state as the completed subtraversal, so we reinitialize it here.
            if let Some(subtraversal) = subtraversal {
                *traversal = subtraversal;
            }
        }
    }

    #[cfg(feature = "debugger")]
    fn get_docs_for_debugger(&self) -> String {
        let mut docs = debug_server::DocumentList::new();

        for (_, doc) in &self.documents {
            let mut debug_doc = debug_server::TreeNode::new("document");

            for (_, pipeline) in &doc.current.scene.pipelines {
                let mut debug_dl = debug_server::TreeNode::new("display-list");
                self.traverse_items(&mut pipeline.display_list.iter(), &mut debug_dl);
                debug_doc.add_child(debug_dl);
            }

            docs.add(debug_doc);
        }

        serde_json::to_string(&docs).unwrap()
    }

    #[cfg(not(feature = "debugger"))]
    fn get_clip_scroll_tree_for_debugger(&self) -> String {
        String::new()
    }

    #[cfg(feature = "debugger")]
    fn get_clip_scroll_tree_for_debugger(&self) -> String {
        let mut debug_root = debug_server::ClipScrollTreeList::new();

        for (_, doc) in &self.documents {
            let debug_node = debug_server::TreeNode::new("document clip-scroll tree");
            let mut builder = debug_server::TreeNodeBuilder::new(debug_node);

            // TODO(gw): Restructure the storage of clip-scroll tree, clip store
            //           etc so this isn't so untidy.
            let clip_store = &doc.frame_builder.as_ref().unwrap().clip_store;
            doc.clip_scroll_tree.print_with(clip_store, &mut builder);

            debug_root.add(builder.build());
        }

        serde_json::to_string(&debug_root).unwrap()
    }
}

#[cfg(feature = "debugger")]
trait ToDebugString {
    fn debug_string(&self) -> String;
}

#[cfg(feature = "debugger")]
impl ToDebugString for SpecificDisplayItem {
    fn debug_string(&self) -> String {
        match *self {
            SpecificDisplayItem::Border(..) => String::from("border"),
            SpecificDisplayItem::BoxShadow(..) => String::from("box_shadow"),
            SpecificDisplayItem::ClearRectangle => String::from("clear_rectangle"),
            SpecificDisplayItem::Clip(..) => String::from("clip"),
            SpecificDisplayItem::ClipChain(..) => String::from("clip_chain"),
            SpecificDisplayItem::Gradient(..) => String::from("gradient"),
            SpecificDisplayItem::Iframe(..) => String::from("iframe"),
            SpecificDisplayItem::Image(..) => String::from("image"),
            SpecificDisplayItem::Line(..) => String::from("line"),
            SpecificDisplayItem::PopAllShadows => String::from("pop_all_shadows"),
            SpecificDisplayItem::PopReferenceFrame => String::from("pop_reference_frame"),
            SpecificDisplayItem::PopStackingContext => String::from("pop_stacking_context"),
            SpecificDisplayItem::PushReferenceFrame(..) => String::from("push_reference_frame"),
            SpecificDisplayItem::PushShadow(..) => String::from("push_shadow"),
            SpecificDisplayItem::PushStackingContext(..) => String::from("push_stacking_context"),
            SpecificDisplayItem::RadialGradient(..) => String::from("radial_gradient"),
            SpecificDisplayItem::Rectangle(..) => String::from("rectangle"),
            SpecificDisplayItem::ScrollFrame(..) => String::from("scroll_frame"),
            SpecificDisplayItem::SetGradientStops => String::from("set_gradient_stops"),
            SpecificDisplayItem::StickyFrame(..) => String::from("sticky_frame"),
            SpecificDisplayItem::Text(..) => String::from("text"),
            SpecificDisplayItem::YuvImage(..) => String::from("yuv_image"),
        }
    }
}

impl RenderBackend {
    #[cfg(feature = "capture")]
    // Note: the mutable `self` is only needed here for resolving blob images
    fn save_capture(
        &mut self,
        root: PathBuf,
        bits: CaptureBits,
        profile_counters: &mut BackendProfileCounters,
    ) -> DebugOutput {
        use std::fs;
        use capture::CaptureConfig;

        debug!("capture: saving {:?}", root);
        if !root.is_dir() {
            if let Err(e) = fs::create_dir_all(&root) {
                panic!("Unable to create capture dir: {:?}", e);
            }
        }
        let config = CaptureConfig::new(root, bits);

        for (&id, doc) in &mut self.documents {
            debug!("\tdocument {:?}", id);
            if config.bits.contains(CaptureBits::SCENE) {
                let file_name = format!("scene-{}-{}", (id.0).0, id.1);
                config.serialize(&doc.current.scene, file_name);
            }
            if config.bits.contains(CaptureBits::FRAME) {
                let rendered_document = doc.render(
                    &mut self.resource_cache,
                    &mut self.gpu_cache,
                    &mut profile_counters.resources,
                    true,
                );
                //TODO: write down doc's pipeline info?
                // it has `pipeline_epoch_map`,
                // which may capture necessary details for some cases.
                let file_name = format!("frame-{}-{}", (id.0).0, id.1);
                config.serialize(&rendered_document.frame, file_name);
            }
        }

        debug!("\tresource cache");
        let (resources, deferred) = self.resource_cache.save_capture(&config.root);

        info!("\tbackend");
        let backend = PlainRenderBackend {
            default_device_pixel_ratio: self.default_device_pixel_ratio,
            enable_render_on_scroll: self.enable_render_on_scroll,
            frame_config: self.frame_config.clone(),
            documents: self.documents
                .iter()
                .map(|(id, doc)| (*id, doc.view.clone()))
                .collect(),
            resources,
            last_scene_id: self.last_scene_id,
        };

        config.serialize(&backend, "backend");

        if config.bits.contains(CaptureBits::FRAME) {
            // After we rendered the frames, there are pending updates to both
            // GPU cache and resources. Instead of serializing them, we are going to make sure
            // they are applied on the `Renderer` side.
            let msg_update_gpu_cache = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
            self.result_tx.send(msg_update_gpu_cache).unwrap();
            let msg_update_resources = ResultMsg::UpdateResources {
                updates: self.resource_cache.pending_updates(),
                cancel_rendering: false,
            };
            self.result_tx.send(msg_update_resources).unwrap();
            // Save the texture/glyph/image caches.
            info!("\tresource cache");
            let caches = self.resource_cache.save_caches(&config.root);
            config.serialize(&caches, "resource_cache");
            info!("\tgpu cache");
            config.serialize(&self.gpu_cache, "gpu_cache");
        }

        DebugOutput::SaveCapture(config, deferred)
    }

    #[cfg(feature = "replay")]
    fn load_capture(
        &mut self,
        root: &PathBuf,
        profile_counters: &mut BackendProfileCounters,
    ) {
        use capture::CaptureConfig;

        debug!("capture: loading {:?}", root);
        let backend = CaptureConfig::deserialize::<PlainRenderBackend, _>(root, "backend")
            .expect("Unable to open backend.ron");
        let caches_maybe = CaptureConfig::deserialize::<PlainCacheOwn, _>(root, "resource_cache");

        // Note: it would be great to have `RenderBackend` to be split
        // rather explicitly on what's used before and after scene building
        // so that, for example, we never miss anything in the code below:

        let plain_externals = self.resource_cache.load_capture(backend.resources, caches_maybe, root);
        let msg_load = ResultMsg::DebugOutput(
            DebugOutput::LoadCapture(root.clone(), plain_externals)
        );
        self.result_tx.send(msg_load).unwrap();

        self.gpu_cache = match CaptureConfig::deserialize::<GpuCache, _>(root, "gpu_cache") {
            Some(gpu_cache) => gpu_cache,
            None => GpuCache::new(),
        };

        self.documents.clear();
        self.default_device_pixel_ratio = backend.default_device_pixel_ratio;
        self.frame_config = backend.frame_config;
        self.enable_render_on_scroll = backend.enable_render_on_scroll;

        let mut last_scene_id = backend.last_scene_id;
        for (id, view) in backend.documents {
            debug!("\tdocument {:?}", id);
            let scene_name = format!("scene-{}-{}", (id.0).0, id.1);
            let scene = CaptureConfig::deserialize::<Scene, _>(root, &scene_name)
                .expect(&format!("Unable to open {}.ron", scene_name));

            let mut doc = Document {
                current: SceneData {
                    scene: scene.clone(),
                    removed_pipelines: Vec::new(),
                },
                pending: SceneData {
                    scene,
                    removed_pipelines: Vec::new(),
                },
                view,
                clip_scroll_tree: ClipScrollTree::new(),
                frame_id: FrameId(0),
                frame_builder_config: self.frame_config.clone(),
                frame_builder: Some(FrameBuilder::empty()),
                output_pipelines: FastHashSet::default(),
                render_on_scroll: None,
                dynamic_properties: SceneProperties::new(),
                hit_tester: None,
            };

            let frame_name = format!("frame-{}-{}", (id.0).0, id.1);
            let render_doc = match CaptureConfig::deserialize::<Frame, _>(root, frame_name) {
                Some(frame) => {
                    info!("\tloaded a built frame with {} passes", frame.passes.len());
                    RenderedDocument { frame, is_new_scene: true }
                }
                None => {
                    last_scene_id += 1;
                    doc.build_scene(&mut self.resource_cache, last_scene_id);
                    doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                        true,
                    )
                }
            };

            let msg_update = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
            self.result_tx.send(msg_update).unwrap();

            let msg_publish = ResultMsg::PublishDocument(
                id,
                render_doc,
                self.resource_cache.pending_updates(),
                profile_counters.clone(),
            );
            self.result_tx.send(msg_publish).unwrap();
            profile_counters.reset();

            self.notifier.new_frame_ready(id, false, true, None);
            self.documents.insert(id, doc);
        }
    }
}


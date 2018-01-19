/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ApiMsg, BlobImageRenderer, BuiltDisplayList, DebugCommand, DeviceIntPoint};
#[cfg(feature = "debugger")]
use api::{BuiltDisplayListIter, SpecificDisplayItem};
use api::{DevicePixelScale, DeviceUintPoint, DeviceUintRect, DeviceUintSize};
use api::{DocumentId, DocumentLayer, DocumentMsg};
use api::{IdNamespace, PipelineId, RenderNotifier};
use api::channel::{MsgReceiver, PayloadReceiver, PayloadReceiverHelperMethods};
use api::channel::{PayloadSender, PayloadSenderHelperMethods};
#[cfg(feature = "capture")]
use api::CapturedDocument;
#[cfg(feature = "capture")]
use capture::{CaptureConfig, ExternalCaptureImage};
#[cfg(feature = "debugger")]
use debug_server;
use frame::FrameContext;
use frame_builder::{FrameBuilder, FrameBuilderConfig};
use gpu_cache::GpuCache;
use internal_types::{DebugOutput, FastHashMap, FastHashSet, RenderedDocument, ResultMsg};
use profiler::{BackendProfileCounters, ResourceProfileCounters};
use rayon::ThreadPool;
use record::ApiRecordingReceiver;
use resource_cache::ResourceCache;
#[cfg(feature = "capture")]
use resource_cache::{PlainCacheOwn, PlainResources};
use scene::Scene;
#[cfg(feature = "serialize")]
use serde::{Serialize, Deserialize};
#[cfg(feature = "debugger")]
use serde_json;
#[cfg(feature = "capture")]
use std::path::PathBuf;
use std::sync::atomic::{ATOMIC_USIZE_INIT, AtomicUsize, Ordering};
use std::sync::Arc;
use std::sync::mpsc::Sender;
use std::u32;
use texture_cache::TextureCache;
use time::precise_time_ns;


#[cfg_attr(feature = "capture", derive(Clone, Serialize, Deserialize))]
struct DocumentView {
    window_size: DeviceUintSize,
    inner_rect: DeviceUintRect,
    layer: DocumentLayer,
    pan: DeviceIntPoint,
    device_pixel_ratio: f32,
    page_zoom_factor: f32,
    pinch_zoom_factor: f32,
}

impl DocumentView {
    fn accumulated_scale_factor(&self) -> DevicePixelScale {
        DevicePixelScale::new(
            self.device_pixel_ratio *
            self.page_zoom_factor *
            self.pinch_zoom_factor
        )
    }
}

struct Document {
    scene: Scene,
    view: DocumentView,
    frame_ctx: FrameContext,
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
}

impl Document {
    pub fn new(
        config: FrameBuilderConfig,
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
            scene: Scene::new(),
            view: DocumentView {
                window_size,
                inner_rect: DeviceUintRect::new(DeviceUintPoint::zero(), window_size),
                layer,
                pan: DeviceIntPoint::zero(),
                page_zoom_factor: 1.0,
                pinch_zoom_factor: 1.0,
                device_pixel_ratio: default_device_pixel_ratio,
            },
            frame_ctx: FrameContext::new(config),
            frame_builder: Some(FrameBuilder::empty()),
            render_on_scroll,
            output_pipelines: FastHashSet::default(),
        }
    }

    fn build_scene(&mut self, resource_cache: &mut ResourceCache) {
        // this code is why we have `Option`, which is never `None`
        let frame_builder = self.frame_ctx.create(
            self.frame_builder.take().unwrap(),
            &self.scene,
            resource_cache,
            self.view.window_size,
            self.view.inner_rect,
            self.view.accumulated_scale_factor(),
            &self.output_pipelines,
        );
        self.frame_builder = Some(frame_builder);
    }

    fn render(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        resource_profile: &mut ResourceProfileCounters,
    ) -> RenderedDocument {
        let accumulated_scale_factor = self.view.accumulated_scale_factor();
        let pan = self.view.pan.to_f32() / accumulated_scale_factor;
        self.frame_ctx.build_rendered_document(
            self.frame_builder.as_mut().unwrap(),
            resource_cache,
            gpu_cache,
            &self.scene.pipelines,
            accumulated_scale_factor,
            self.view.layer,
            pan,
            &mut resource_profile.texture_cache,
            &mut resource_profile.gpu_cache,
            &self.scene.properties,
        )
    }
}

struct DocumentOps {
    scroll: bool,
    build: bool,
    render: bool,
}

impl DocumentOps {
    fn nop() -> Self {
        DocumentOps {
            scroll: false,
            build: false,
            render: false,
        }
    }

    fn build() -> Self {
        DocumentOps {
            build: true,
            ..DocumentOps::nop()
        }
    }

    fn combine(&mut self, other: Self) {
        self.scroll = self.scroll || other.scroll;
        self.build = self.build || other.build;
        self.render = self.render || other.render;
    }
}

/// The unique id for WR resource identification.
static NEXT_NAMESPACE_ID: AtomicUsize = ATOMIC_USIZE_INIT;

#[cfg(feature = "capture")]
#[derive(Serialize, Deserialize)]
struct PlainRenderBackend {
    default_device_pixel_ratio: f32,
    enable_render_on_scroll: bool,
    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, DocumentView>,
    resources: PlainResources,
}

/// The render backend is responsible for transforming high level display lists into
/// GPU-friendly work which is then submitted to the renderer in the form of a frame::Frame.
///
/// The render backend operates on its own thread.
pub struct RenderBackend {
    api_rx: MsgReceiver<ApiMsg>,
    payload_rx: PayloadReceiver,
    payload_tx: PayloadSender,
    result_tx: Sender<ResultMsg>,
    default_device_pixel_ratio: f32,

    gpu_cache: GpuCache,
    resource_cache: ResourceCache,

    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, Document>,

    notifier: Box<RenderNotifier>,
    recorder: Option<Box<ApiRecordingReceiver>>,

    enable_render_on_scroll: bool,
}

impl RenderBackend {
    pub fn new(
        api_rx: MsgReceiver<ApiMsg>,
        payload_rx: PayloadReceiver,
        payload_tx: PayloadSender,
        result_tx: Sender<ResultMsg>,
        default_device_pixel_ratio: f32,
        texture_cache: TextureCache,
        workers: Arc<ThreadPool>,
        notifier: Box<RenderNotifier>,
        frame_config: FrameBuilderConfig,
        recorder: Option<Box<ApiRecordingReceiver>>,
        blob_image_renderer: Option<Box<BlobImageRenderer>>,
        enable_render_on_scroll: bool,
    ) -> RenderBackend {
        // The namespace_id should start from 1.
        NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed);

        let resource_cache = ResourceCache::new(texture_cache, workers, blob_image_renderer);

        RenderBackend {
            api_rx,
            payload_rx,
            payload_tx,
            result_tx,
            default_device_pixel_ratio,
            resource_cache,
            gpu_cache: GpuCache::new(),
            frame_config,
            documents: FastHashMap::default(),
            notifier,
            recorder,

            enable_render_on_scroll,
        }
    }

    fn process_document(
        &mut self,
        document_id: DocumentId,
        message: DocumentMsg,
        frame_counter: u32,
        profile_counters: &mut BackendProfileCounters,
    ) -> DocumentOps {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            //TODO: move view-related messages in a separate enum?
            DocumentMsg::SetPageZoom(factor) => {
                doc.view.page_zoom_factor = factor.get();
                DocumentOps::nop()
            }
            DocumentMsg::EnableFrameOutput(pipeline_id, enable) => {
                if enable {
                    doc.output_pipelines.insert(pipeline_id);
                } else {
                    doc.output_pipelines.remove(&pipeline_id);
                }
                DocumentOps::nop()
            }
            DocumentMsg::SetPinchZoom(factor) => {
                doc.view.pinch_zoom_factor = factor.get();
                DocumentOps::nop()
            }
            DocumentMsg::SetPan(pan) => {
                doc.view.pan = pan;
                DocumentOps::nop()
            }
            DocumentMsg::SetWindowParameters {
                window_size,
                inner_rect,
                device_pixel_ratio,
            } => {
                doc.view.window_size = window_size;
                doc.view.inner_rect = inner_rect;
                doc.view.device_pixel_ratio = device_pixel_ratio;
                DocumentOps::nop()
            }
            DocumentMsg::SetDisplayList {
                epoch,
                pipeline_id,
                background,
                viewport_size,
                content_size,
                list_descriptor,
                preserve_frame_state,
            } => {
                profile_scope!("SetDisplayList");

                let mut data;
                while {
                    data = self.payload_rx.recv_payload().unwrap();
                    data.epoch != epoch || data.pipeline_id != pipeline_id
                } {
                    self.payload_tx.send_payload(data).unwrap()
                }

                if let Some(ref mut r) = self.recorder {
                    r.write_payload(frame_counter, &data.to_data());
                }

                let built_display_list =
                    BuiltDisplayList::from_data(data.display_list_data, list_descriptor);

                if !preserve_frame_state {
                    doc.frame_ctx.discard_frame_state_for_pipeline(pipeline_id);
                }

                let display_list_len = built_display_list.data().len();
                let (builder_start_time, builder_finish_time, send_start_time) =
                    built_display_list.times();
                let display_list_received_time = precise_time_ns();

                {
                    let _timer = profile_counters.total_time.timer();
                    doc.scene.set_display_list(
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

                profile_counters.ipc.set(
                    builder_start_time,
                    builder_finish_time,
                    send_start_time,
                    display_list_received_time,
                    display_list_consumed_time,
                    display_list_len,
                );

                DocumentOps::build()
            }
            DocumentMsg::UpdateResources(updates) => {
                profile_scope!("UpdateResources");

                self.resource_cache.update_resources(
                    updates,
                    &mut profile_counters.resources
                );

                DocumentOps::nop()
            }
            DocumentMsg::UpdateEpoch(pipeline_id, epoch) => {
                doc.scene.update_epoch(pipeline_id, epoch);
                doc.frame_ctx.update_epoch(pipeline_id, epoch);
                DocumentOps::nop()
            }
            DocumentMsg::SetRootPipeline(pipeline_id) => {
                profile_scope!("SetRootPipeline");

                doc.scene.set_root_pipeline_id(pipeline_id);
                if doc.scene.pipelines.get(&pipeline_id).is_some() {
                    DocumentOps::build()
                } else {
                    DocumentOps::nop()
                }
            }
            DocumentMsg::RemovePipeline(pipeline_id) => {
                profile_scope!("RemovePipeline");

                doc.scene.remove_pipeline(pipeline_id);
                DocumentOps::nop()
            }
            DocumentMsg::Scroll(delta, cursor, move_phase) => {
                profile_scope!("Scroll");
                let _timer = profile_counters.total_time.timer();

                let should_render = doc.frame_ctx.scroll(delta, cursor, move_phase)
                    && doc.render_on_scroll == Some(true);

                DocumentOps {
                    scroll: true,
                    build: false,
                    render: should_render,
                }
            }
            DocumentMsg::HitTest(pipeline_id, point, flags, tx) => {
                profile_scope!("HitTest");
                let cst = doc.frame_ctx.get_clip_scroll_tree();
                let result = doc.frame_builder
                    .as_ref()
                    .unwrap()
                    .hit_test(cst, pipeline_id, point, flags);
                tx.send(result).unwrap();
                DocumentOps::nop()
            }
            DocumentMsg::ScrollNodeWithId(origin, id, clamp) => {
                profile_scope!("ScrollNodeWithScrollId");
                let _timer = profile_counters.total_time.timer();

                let should_render = doc.frame_ctx.scroll_node(origin, id, clamp)
                    && doc.render_on_scroll == Some(true);

                DocumentOps {
                    scroll: true,
                    build: false,
                    render: should_render,
                }
            }
            DocumentMsg::TickScrollingBounce => {
                profile_scope!("TickScrollingBounce");
                let _timer = profile_counters.total_time.timer();

                doc.frame_ctx.tick_scrolling_bounce_animations();

                DocumentOps {
                    scroll: true,
                    build: false,
                    render: doc.render_on_scroll == Some(true),
                }
            }
            DocumentMsg::GetScrollNodeState(tx) => {
                profile_scope!("GetScrollNodeState");
                tx.send(doc.frame_ctx.get_scroll_node_state()).unwrap();
                DocumentOps::nop()
            }
            DocumentMsg::UpdateDynamicProperties(property_bindings) => {
                // Ideally, when there are property bindings present,
                // we won't need to rebuild the entire frame here.
                // However, to avoid conflicts with the ongoing work to
                // refactor how scroll roots + transforms work, this
                // just rebuilds the frame if there are animated property
                // bindings present for now.
                // TODO(gw): Once the scrolling / reference frame changes
                //           are completed, optimize the internals of
                //           animated properties to not require a full
                //           rebuild of the frame!
                doc.scene.properties.set_properties(property_bindings);
                DocumentOps::build()
            }
            DocumentMsg::GenerateFrame => {
                let _timer = profile_counters.total_time.timer();

                let mut op = DocumentOps::nop();

                if let Some(ref mut ros) = doc.render_on_scroll {
                    *ros = true;
                }

                if doc.scene.root_pipeline_id.is_some() {
                    op.render = true;
                }

                op
            }
        }
    }

    fn next_namespace_id(&self) -> IdNamespace {
        IdNamespace(NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed) as u32)
    }

    pub fn run(&mut self, mut profile_counters: BackendProfileCounters) {
        let mut frame_counter: u32 = 0;

        loop {
            profile_scope!("handle_msg");

            let msg = match self.api_rx.recv() {
                Ok(msg) => {
                    if let Some(ref mut r) = self.recorder {
                        r.write_msg(frame_counter, &msg);
                    }
                    msg
                }
                Err(..) => {
                    self.notifier.shut_down();
                    break;
                }
            };

            match msg {
                ApiMsg::UpdateResources(updates) => {
                    self.resource_cache
                        .update_resources(updates, &mut profile_counters.resources);
                }
                ApiMsg::GetGlyphDimensions(instance_key, glyph_keys, tx) => {
                    let mut glyph_dimensions = Vec::with_capacity(glyph_keys.len());
                    if let Some(font) = self.resource_cache.get_font_instance(instance_key) {
                        for glyph_key in &glyph_keys {
                            let glyph_dim = self.resource_cache.get_glyph_dimensions(&font, glyph_key);
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
                    self.resource_cache.on_memory_pressure();

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
                            let config = CaptureConfig::new(root, bits);
                            let deferred = self.save_capture(&config, &mut profile_counters);
                            ResultMsg::DebugOutput(DebugOutput::SaveCapture(config, deferred))
                        },
                        #[cfg(feature = "capture")]
                        DebugCommand::LoadCapture(root, tx) => {
                            NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed);
                            frame_counter += 1;
                            let msg = ResultMsg::DebugOutput(
                                DebugOutput::LoadCapture(root.clone())
                            );
                            self.result_tx.send(msg).unwrap();
                            self.load_capture(&root, &mut profile_counters);

                            for (id, doc) in &self.documents {
                                let captured = CapturedDocument {
                                    document_id: *id,
                                    root_pipeline_id: doc.scene.root_pipeline_id,
                                };
                                tx.send(captured).unwrap();
                            }
                            // Note: we can't pass `LoadCapture` here since it needs to arrive
                            // before the `PublishDocument` messages sent by `load_capture`.
                            continue
                        },
                        DebugCommand::EnableDualSourceBlending(enable) => {
                            // Set in the config used for any future documents
                            // that are created.
                            self.frame_config
                                .dual_source_blending_is_enabled = enable;

                            // Set for any existing documents.
                            for (_, doc) in &mut self.documents {
                                doc.frame_ctx
                                   .frame_builder_config
                                   .dual_source_blending_is_enabled = enable;
                            }

                            // We don't want to forward this message to the renderer.
                            continue;
                        }
                        _ => ResultMsg::DebugCommand(option),
                    };
                    self.result_tx.send(msg).unwrap();
                    self.notifier.wake_up();
                }
                ApiMsg::ShutDown => {
                    self.notifier.shut_down();
                    break;
                }
                ApiMsg::UpdateDocument(document_id, doc_msgs) => {
                    self.update_document(
                        document_id,
                        doc_msgs,
                        &mut frame_counter,
                        &mut profile_counters
                    )
                }
            }
        }
    }

    fn update_document(
        &mut self,
        document_id: DocumentId,
        doc_msgs: Vec<DocumentMsg>,
        frame_counter: &mut u32,
        profile_counters: &mut BackendProfileCounters,
    ) {
        let mut op = DocumentOps::nop();
        for doc_msg in doc_msgs {
            op.combine(
                self.process_document(
                    document_id,
                    doc_msg,
                    *frame_counter,
                    profile_counters,
                )
            );
        }

        let doc = self.documents.get_mut(&document_id).unwrap();

        if op.build {
            profile_scope!("build scene");
            doc.build_scene(&mut self.resource_cache);
        }

        if op.render {
            profile_scope!("generate frame");

            *frame_counter += 1;
            let rendered_document = doc.render(
                &mut self.resource_cache,
                &mut self.gpu_cache,
                &mut profile_counters.resources,
            );

            info!("generated frame for document {:?} with {} passes",
                document_id, rendered_document.frame.passes.len());

            // Publish the frame
            let pending_update = self.resource_cache.pending_updates();
            let msg = ResultMsg::PublishDocument(
                document_id,
                rendered_document,
                pending_update,
                profile_counters.clone()
            );
            self.result_tx.send(msg).unwrap();
            profile_counters.reset();
        }

        if op.render || op.scroll {
            self.notifier.new_document_ready(document_id, op.scroll, op.render);
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

            for (_, pipeline) in &doc.scene.pipelines {
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
            doc.frame_ctx
                .get_clip_scroll_tree()
                .print_with(clip_store, &mut builder);

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
            SpecificDisplayItem::Image(..) => String::from("image"),
            SpecificDisplayItem::YuvImage(..) => String::from("yuv_image"),
            SpecificDisplayItem::Text(..) => String::from("text"),
            SpecificDisplayItem::Rectangle(..) => String::from("rectangle"),
            SpecificDisplayItem::ClearRectangle => String::from("clear_rectangle"),
            SpecificDisplayItem::Line(..) => String::from("line"),
            SpecificDisplayItem::Gradient(..) => String::from("gradient"),
            SpecificDisplayItem::RadialGradient(..) => String::from("radial_gradient"),
            SpecificDisplayItem::BoxShadow(..) => String::from("box_shadow"),
            SpecificDisplayItem::Border(..) => String::from("border"),
            SpecificDisplayItem::PushStackingContext(..) => String::from("push_stacking_context"),
            SpecificDisplayItem::Iframe(..) => String::from("iframe"),
            SpecificDisplayItem::Clip(..) => String::from("clip"),
            SpecificDisplayItem::ClipChain(..) => String::from("clip_chain"),
            SpecificDisplayItem::ScrollFrame(..) => String::from("scroll_frame"),
            SpecificDisplayItem::StickyFrame(..) => String::from("sticky_frame"),
            SpecificDisplayItem::SetGradientStops => String::from("set_gradient_stops"),
            SpecificDisplayItem::PopStackingContext => String::from("pop_stacking_context"),
            SpecificDisplayItem::PushShadow(..) => String::from("push_shadow"),
            SpecificDisplayItem::PopAllShadows => String::from("pop_all_shadows"),
        }
    }
}

#[cfg(feature = "capture")]
impl RenderBackend {
    // Note: the mutable `self` is only needed here for resolving blob images
    fn save_capture(
        &mut self,
        config: &CaptureConfig,
        profile_counters: &mut BackendProfileCounters,
    ) -> Vec<ExternalCaptureImage> {
        use api::CaptureBits;

        info!("capture: saving {:?}", config.root);
        let (resources, deferred) = self.resource_cache.save_capture(&config.root);

        for (&id, doc) in &mut self.documents {
            info!("\tdocument {:?}", id);
            if config.bits.contains(CaptureBits::SCENE) {
                let file_name = format!("scene-{}-{}", (id.0).0, id.1);
                config.serialize(&doc.scene, file_name);
            }
            if config.bits.contains(CaptureBits::FRAME) {
                let rendered_document = doc.render(
                    &mut self.resource_cache,
                    &mut self.gpu_cache,
                    &mut profile_counters.resources,
                );
                //TODO: write down full `RenderedDocument`?
                // it has `pipeline_epoch_map` and `layers_bouncing_back`,
                // which may capture necessary details for some cases.
                let file_name = format!("frame-{}-{}", (id.0).0, id.1);
                config.serialize(&rendered_document.frame, file_name);
            }
        }

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
        };

        config.serialize(&backend, "backend");

        if config.bits.contains(CaptureBits::FRAME) {
            info!("\tresource cache");
            let caches = self.resource_cache.save_caches(&config.root);
            config.serialize(&caches, "resource_cache");
            info!("\tgpu cache");
            config.serialize(&self.gpu_cache, "gpu_cache");
        }

        deferred
    }

    fn load_capture(
        &mut self,
        root: &PathBuf,
        profile_counters: &mut BackendProfileCounters,
    ) {
        use tiling::Frame;

        info!("capture: loading {:?}", root);
        let backend = CaptureConfig::deserialize::<PlainRenderBackend, _>(root, "backend")
            .expect("Unable to open backend.ron");
        let caches_maybe = CaptureConfig::deserialize::<PlainCacheOwn, _>(root, "resource_cache");

        // Note: it would be great to have RenderBackend to be split
        // rather explicitly on what's used before and after scene building
        // so that, for example, we never miss anything in the code below:

        self.resource_cache.load_capture(backend.resources, caches_maybe,root);

        self.gpu_cache = match CaptureConfig::deserialize::<GpuCache, _>(root, "gpu_cache") {
            Some(gpu_cache) => gpu_cache,
            None => GpuCache::new(),
        };

        self.documents.clear();
        self.default_device_pixel_ratio = backend.default_device_pixel_ratio;
        self.frame_config = backend.frame_config;
        self.enable_render_on_scroll = backend.enable_render_on_scroll;

        for (id, view) in backend.documents {
            info!("\tdocument {:?}", id);
            let scene_name = format!("scene-{}-{}", (id.0).0, id.1);
            let scene = CaptureConfig::deserialize::<Scene, _>(root, &scene_name)
                .expect(&format!("Unable to open {}.ron", scene_name));

            let mut doc = Document {
                scene,
                view,
                frame_ctx: FrameContext::new(self.frame_config.clone()),
                frame_builder: Some(FrameBuilder::empty()),
                output_pipelines: FastHashSet::default(),
                render_on_scroll: None,
            };

            let frame_name = format!("frame-{}-{}", (id.0).0, id.1);
            let render_doc = match CaptureConfig::deserialize::<Frame, _>(root, frame_name) {
                Some(frame) => {
                    info!("\tloaded a built frame with {} passes", frame.passes.len());
                    doc.frame_ctx.make_rendered_document(frame)
                }
                None => {
                    doc.build_scene(&mut self.resource_cache);
                    doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                    )
                }
            };

            let msg = ResultMsg::PublishDocument(
                id,
                render_doc,
                self.resource_cache.pending_updates(),
                profile_counters.clone(),
            );
            self.result_tx.send(msg).unwrap();
            profile_counters.reset();

            self.notifier.new_document_ready(id, false, true);
            self.documents.insert(id, doc);
        }
    }
}

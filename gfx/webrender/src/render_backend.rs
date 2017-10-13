/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use api::{ApiMsg, BlobImageRenderer, BuiltDisplayList, DebugCommand, DeviceIntPoint};
#[cfg(feature = "debugger")]
use api::{BuiltDisplayListIter, SpecificDisplayItem};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize, DocumentId, DocumentMsg};
use api::{IdNamespace, LayerPoint, PipelineId, RenderNotifier};
use api::channel::{MsgReceiver, PayloadReceiver, PayloadReceiverHelperMethods};
use api::channel::{PayloadSender, PayloadSenderHelperMethods};
#[cfg(feature = "debugger")]
use debug_server;
use frame::Frame;
use frame_builder::FrameBuilderConfig;
use gpu_cache::GpuCache;
use internal_types::{DebugOutput, FastHashMap, FastHashSet, RendererFrame, ResultMsg};
use profiler::{BackendProfileCounters, ResourceProfileCounters};
use rayon::ThreadPool;
use record::ApiRecordingReceiver;
use resource_cache::ResourceCache;
use scene::Scene;
#[cfg(feature = "debugger")]
use serde_json;
use std::sync::{Arc, Mutex};
use std::sync::mpsc::Sender;
use std::u32;
use texture_cache::TextureCache;
use thread_profiler::register_thread_with_profiler;
use time::precise_time_ns;

struct Document {
    scene: Scene,
    frame: Frame,
    window_size: DeviceUintSize,
    inner_rect: DeviceUintRect,
    pan: DeviceIntPoint,
    device_pixel_ratio: f32,
    page_zoom_factor: f32,
    pinch_zoom_factor: f32,
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
        initial_size: DeviceUintSize,
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
            frame: Frame::new(config),
            window_size: initial_size,
            inner_rect: DeviceUintRect::new(DeviceUintPoint::zero(), initial_size),
            pan: DeviceIntPoint::zero(),
            page_zoom_factor: 1.0,
            pinch_zoom_factor: 1.0,
            device_pixel_ratio: default_device_pixel_ratio,
            render_on_scroll,
            output_pipelines: FastHashSet::default(),
        }
    }

    fn accumulated_scale_factor(&self) -> f32 {
        self.device_pixel_ratio *
        self.page_zoom_factor *
        self.pinch_zoom_factor
    }

    fn build_scene(&mut self, resource_cache: &mut ResourceCache) {
        let accumulated_scale_factor = self.accumulated_scale_factor();
        self.frame.create(
            &self.scene,
            resource_cache,
            self.window_size,
            self.inner_rect,
            accumulated_scale_factor,
        );
    }

    fn render(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        resource_profile: &mut ResourceProfileCounters,
    ) -> RendererFrame {
        let accumulated_scale_factor = self.accumulated_scale_factor();
        let pan = LayerPoint::new(
            self.pan.x as f32 / accumulated_scale_factor,
            self.pan.y as f32 / accumulated_scale_factor,
        );
        self.frame.build(
            resource_cache,
            gpu_cache,
            &self.scene.pipelines,
            accumulated_scale_factor,
            pan,
            &self.output_pipelines,
            &mut resource_profile.texture_cache,
            &mut resource_profile.gpu_cache,
        )
    }
}

enum DocumentOp {
    Nop,
    Built,
    ScrolledNop,
    Scrolled(RendererFrame),
    Rendered(RendererFrame),
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
    next_namespace_id: IdNamespace,
    default_device_pixel_ratio: f32,

    gpu_cache: GpuCache,
    resource_cache: ResourceCache,

    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, Document>,

    notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
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
        notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
        frame_config: FrameBuilderConfig,
        recorder: Option<Box<ApiRecordingReceiver>>,
        blob_image_renderer: Option<Box<BlobImageRenderer>>,
        enable_render_on_scroll: bool,
    ) -> RenderBackend {
        let resource_cache = ResourceCache::new(texture_cache, workers, blob_image_renderer);

        register_thread_with_profiler("Backend".to_string());

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
            next_namespace_id: IdNamespace(1),
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
    ) -> DocumentOp {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            DocumentMsg::SetPageZoom(factor) => {
                doc.page_zoom_factor = factor.get();
                DocumentOp::Nop
            }
            DocumentMsg::EnableFrameOutput(pipeline_id, enable) => {
                if enable {
                    doc.output_pipelines.insert(pipeline_id);
                } else {
                    doc.output_pipelines.remove(&pipeline_id);
                }
                DocumentOp::Nop
            }
            DocumentMsg::SetPinchZoom(factor) => {
                doc.pinch_zoom_factor = factor.get();
                DocumentOp::Nop
            }
            DocumentMsg::SetPan(pan) => {
                doc.pan = pan;
                DocumentOp::Nop
            }
            DocumentMsg::SetWindowParameters {
                window_size,
                inner_rect,
                device_pixel_ratio,
            } => {
                doc.window_size = window_size;
                doc.inner_rect = inner_rect;
                doc.device_pixel_ratio = device_pixel_ratio;
                DocumentOp::Nop
            }
            DocumentMsg::SetDisplayList {
                epoch,
                pipeline_id,
                background,
                viewport_size,
                content_size,
                list_descriptor,
                preserve_frame_state,
                resources,
            } => {
                profile_scope!("SetDisplayList");

                self.resource_cache
                    .update_resources(resources, &mut profile_counters.resources);

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
                    doc.frame.discard_frame_state_for_pipeline(pipeline_id);
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
                    doc.build_scene(&mut self.resource_cache);
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

                DocumentOp::Built
            }
            DocumentMsg::SetRootPipeline(pipeline_id) => {
                profile_scope!("SetRootPipeline");

                doc.scene.set_root_pipeline_id(pipeline_id);
                if doc.scene.pipelines.get(&pipeline_id).is_some() {
                    let _timer = profile_counters.total_time.timer();
                    doc.build_scene(&mut self.resource_cache);
                    DocumentOp::Built
                } else {
                    DocumentOp::Nop
                }
            }
            DocumentMsg::RemovePipeline(pipeline_id) => {
                profile_scope!("RemovePipeline");

                doc.scene.remove_pipeline(pipeline_id);
                DocumentOp::Nop
            }
            DocumentMsg::Scroll(delta, cursor, move_phase) => {
                profile_scope!("Scroll");
                let _timer = profile_counters.total_time.timer();

                if doc.frame.scroll(delta, cursor, move_phase) && doc.render_on_scroll == Some(true)
                {
                    let frame = doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                    );
                    DocumentOp::Scrolled(frame)
                } else {
                    DocumentOp::ScrolledNop
                }
            }
            DocumentMsg::HitTest(pipeline_id, point, flags, tx) => {
                profile_scope!("HitTest");
                let result = doc.frame.hit_test(pipeline_id, point, flags);
                tx.send(result).unwrap();
                DocumentOp::Nop
            }
            DocumentMsg::ScrollNodeWithId(origin, id, clamp) => {
                profile_scope!("ScrollNodeWithScrollId");
                let _timer = profile_counters.total_time.timer();

                if doc.frame.scroll_node(origin, id, clamp) && doc.render_on_scroll == Some(true) {
                    let frame = doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                    );
                    DocumentOp::Scrolled(frame)
                } else {
                    DocumentOp::ScrolledNop
                }
            }
            DocumentMsg::TickScrollingBounce => {
                profile_scope!("TickScrollingBounce");
                let _timer = profile_counters.total_time.timer();

                doc.frame.tick_scrolling_bounce_animations();
                if doc.render_on_scroll == Some(true) {
                    let frame = doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                    );
                    DocumentOp::Scrolled(frame)
                } else {
                    DocumentOp::ScrolledNop
                }
            }
            DocumentMsg::GetScrollNodeState(tx) => {
                profile_scope!("GetScrollNodeState");
                tx.send(doc.frame.get_scroll_node_state()).unwrap();
                DocumentOp::Nop
            }
            DocumentMsg::GenerateFrame(property_bindings) => {
                profile_scope!("GenerateFrame");
                let _timer = profile_counters.total_time.timer();

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
                if let Some(property_bindings) = property_bindings {
                    doc.scene.properties.set_properties(property_bindings);
                    doc.build_scene(&mut self.resource_cache);
                }

                if let Some(ref mut ros) = doc.render_on_scroll {
                    *ros = true;
                }

                if doc.scene.root_pipeline_id.is_some() {
                    let frame = doc.render(
                        &mut self.resource_cache,
                        &mut self.gpu_cache,
                        &mut profile_counters.resources,
                    );
                    DocumentOp::Rendered(frame)
                } else {
                    DocumentOp::ScrolledNop
                }
            }
        }
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
                    let notifier = self.notifier.lock();
                    notifier.unwrap().as_mut().unwrap().shut_down();
                    break;
                }
            };

            match msg {
                ApiMsg::UpdateResources(updates) => {
                    self.resource_cache
                        .update_resources(updates, &mut profile_counters.resources);
                }
                ApiMsg::GetGlyphDimensions(font, glyph_keys, tx) => {
                    let mut glyph_dimensions = Vec::with_capacity(glyph_keys.len());
                    for glyph_key in &glyph_keys {
                        let glyph_dim = self.resource_cache.get_glyph_dimensions(&font, glyph_key);
                        glyph_dimensions.push(glyph_dim);
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
                    let namespace = self.next_namespace_id;
                    self.next_namespace_id = IdNamespace(namespace.0 + 1);
                    sender.send(namespace).unwrap();
                }
                ApiMsg::AddDocument(document_id, initial_size) => {
                    let document = Document::new(
                        self.frame_config.clone(),
                        initial_size,
                        self.enable_render_on_scroll,
                        self.default_device_pixel_ratio,
                    );
                    self.documents.insert(document_id, document);
                }
                ApiMsg::UpdateDocument(document_id, doc_msg) => match self.process_document(
                    document_id,
                    doc_msg,
                    frame_counter,
                    &mut profile_counters,
                ) {
                    DocumentOp::Nop => {}
                    DocumentOp::Built => {}
                    DocumentOp::ScrolledNop => {
                        self.notify_compositor_of_new_scroll_frame(false);
                    }
                    DocumentOp::Scrolled(frame) => {
                        self.publish_frame(document_id, frame, &mut profile_counters);
                        self.notify_compositor_of_new_scroll_frame(true);
                    }
                    DocumentOp::Rendered(frame) => {
                        frame_counter += 1;
                        self.publish_frame_and_notify_compositor(
                            document_id,
                            frame,
                            &mut profile_counters,
                        );
                    }
                },
                ApiMsg::DeleteDocument(document_id) => {
                    self.documents.remove(&document_id);
                }
                ApiMsg::ExternalEvent(evt) => {
                    let notifier = self.notifier.lock();
                    notifier.unwrap().as_mut().unwrap().external_event(evt);
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
                    // We use new_frame_ready to wake up the renderer and get the
                    // resource updates processed, but the UpdateResources message
                    // will cancel rendering the frame.
                    self.notifier
                        .lock()
                        .unwrap()
                        .as_mut()
                        .unwrap()
                        .new_frame_ready();
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
                        _ => ResultMsg::DebugCommand(option),
                    };
                    self.result_tx.send(msg).unwrap();
                    let notifier = self.notifier.lock();
                    notifier.unwrap().as_mut().unwrap().new_frame_ready();
                }
                ApiMsg::ShutDown => {
                    let notifier = self.notifier.lock();
                    notifier.unwrap().as_mut().unwrap().shut_down();
                    break;
                }
            }
        }
    }

    fn publish_frame(
        &mut self,
        document_id: DocumentId,
        frame: RendererFrame,
        profile_counters: &mut BackendProfileCounters,
    ) {
        let pending_update = self.resource_cache.pending_updates();
        let msg = ResultMsg::NewFrame(document_id, frame, pending_update, profile_counters.clone());
        self.result_tx.send(msg).unwrap();
        profile_counters.reset();
    }

    fn publish_frame_and_notify_compositor(
        &mut self,
        document_id: DocumentId,
        frame: RendererFrame,
        profile_counters: &mut BackendProfileCounters,
    ) {
        self.publish_frame(document_id, frame, profile_counters);

        // TODO(gw): This is kindof bogus to have to lock the notifier
        //           each time it's used. This is due to some nastiness
        //           in initialization order for Servo. Perhaps find a
        //           cleaner way to do this, or use the OnceMutex on crates.io?
        let mut notifier = self.notifier.lock();
        notifier
            .as_mut()
            .unwrap()
            .as_mut()
            .unwrap()
            .new_frame_ready();
    }

    fn notify_compositor_of_new_scroll_frame(&mut self, composite_needed: bool) {
        // TODO(gw): This is kindof bogus to have to lock the notifier
        //           each time it's used. This is due to some nastiness
        //           in initialization order for Servo. Perhaps find a
        //           cleaner way to do this, or use the OnceMutex on crates.io?
        let mut notifier = self.notifier.lock();
        notifier
            .as_mut()
            .unwrap()
            .as_mut()
            .unwrap()
            .new_scroll_frame_ready(composite_needed);
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
                let mut debug_dl = debug_server::TreeNode::new("display_list");
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
            let debug_node = debug_server::TreeNode::new("document clip_scroll tree");
            let mut builder = debug_server::TreeNodeBuilder::new(debug_node);
            // TODO(gw): Restructure the storage of clip-scroll tree, clip store
            //           etc so this isn't so untidy.
            let clip_store = &doc.frame.frame_builder.as_ref().unwrap().clip_store;
            doc.frame
                .clip_scroll_tree
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
            SpecificDisplayItem::Line(..) => String::from("line"),
            SpecificDisplayItem::Gradient(..) => String::from("gradient"),
            SpecificDisplayItem::RadialGradient(..) => String::from("radial_gradient"),
            SpecificDisplayItem::BoxShadow(..) => String::from("box_shadow"),
            SpecificDisplayItem::Border(..) => String::from("border"),
            SpecificDisplayItem::PushStackingContext(..) => String::from("push_stacking_context"),
            SpecificDisplayItem::Iframe(..) => String::from("iframe"),
            SpecificDisplayItem::Clip(..) => String::from("clip"),
            SpecificDisplayItem::ScrollFrame(..) => String::from("scroll_frame"),
            SpecificDisplayItem::StickyFrame(..) => String::from("sticky_frame"),
            SpecificDisplayItem::SetGradientStops => String::from("set_gradient_stops"),
            SpecificDisplayItem::PopStackingContext => String::from("pop_stacking_context"),
            SpecificDisplayItem::PushShadow(..) => String::from("push_shadow"),
            SpecificDisplayItem::PopAllShadows => String::from("pop_all_shadows"),
        }
    }
}

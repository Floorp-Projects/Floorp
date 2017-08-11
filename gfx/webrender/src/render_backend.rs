/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use frame::Frame;
use frame_builder::FrameBuilderConfig;
use gpu_cache::GpuCache;
use internal_types::{FastHashMap, SourceTexture, ResultMsg, RendererFrame};
use profiler::{BackendProfileCounters, ResourceProfileCounters};
use record::ApiRecordingReceiver;
use resource_cache::ResourceCache;
use scene::Scene;
use std::sync::{Arc, Mutex};
use std::sync::mpsc::Sender;
use std::u32;
use texture_cache::TextureCache;
use time::precise_time_ns;
use thread_profiler::register_thread_with_profiler;
use rayon::ThreadPool;
use webgl_types::{GLContextHandleWrapper, GLContextWrapper};
use api::channel::{MsgReceiver, PayloadReceiver, PayloadReceiverHelperMethods};
use api::channel::{PayloadSender, PayloadSenderHelperMethods};
use api::{ApiMsg, BlobImageRenderer, BuiltDisplayList, DeviceIntPoint};
use api::{DeviceUintPoint, DeviceUintRect, DeviceUintSize, DocumentId, DocumentMsg};
use api::{IdNamespace, LayerPoint, RenderDispatcher, RenderNotifier};
use api::{VRCompositorCommand, VRCompositorHandler, WebGLCommand, WebGLContextId};

#[cfg(feature = "webgl")]
use offscreen_gl_context::GLContextDispatcher;

#[cfg(not(feature = "webgl"))]
use webgl_types::GLContextDispatcher;

struct Document {
    scene: Scene,
    frame: Frame,
    window_size: DeviceUintSize,
    inner_rect: DeviceUintRect,
    pan: DeviceIntPoint,
    page_zoom_factor: f32,
    pinch_zoom_factor: f32,
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
            render_on_scroll,
        }
    }

    fn accumulated_scale_factor(&self, hidpi_factor: f32) -> f32 {
        hidpi_factor * self.page_zoom_factor * self.pinch_zoom_factor
    }

    fn build_scene(&mut self, resource_cache: &mut ResourceCache, hidpi_factor: f32) {
        let accumulated_scale_factor = self.accumulated_scale_factor(hidpi_factor);
        self.frame.create(&self.scene,
                          resource_cache,
                          self.window_size,
                          self.inner_rect,
                          accumulated_scale_factor);
    }

    fn render(&mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        resource_profile: &mut ResourceProfileCounters,
        hidpi_factor: f32,
    )-> RendererFrame {
        let accumulated_scale_factor = self.accumulated_scale_factor(hidpi_factor);
        let pan = LayerPoint::new(self.pan.x as f32 / accumulated_scale_factor,
                                  self.pan.y as f32 / accumulated_scale_factor);
        self.frame.build(resource_cache,
                         gpu_cache,
                         &self.scene.display_lists,
                         accumulated_scale_factor,
                         pan,
                         &mut resource_profile.texture_cache,
                         &mut resource_profile.gpu_cache)
    }
}

struct WebGL {
    last_id: WebGLContextId,
    contexts: FastHashMap<WebGLContextId, GLContextWrapper>,
    active_id: Option<WebGLContextId>,
}

impl WebGL {
    fn new() -> Self {
        WebGL {
            last_id: WebGLContextId(0),
            contexts: FastHashMap::default(),
            active_id: None,
        }
    }

    fn register(&mut self, context: GLContextWrapper) -> WebGLContextId {
        // Creating a new GLContext may make the current bound context_id dirty.
        // Clear it to ensure that  make_current() is called in subsequent commands.
        self.active_id = None;
        self.last_id.0 += 1;
        self.contexts.insert(self.last_id, context);
        self.last_id
    }

    fn activate(&mut self, id: WebGLContextId) -> &mut GLContextWrapper {
        let ctx = self.contexts.get_mut(&id).unwrap();
        if Some(id) != self.active_id {
            ctx.make_current();
            self.active_id = Some(id);
        }
        ctx
    }

    fn flush(&mut self) {
        if let Some(id) = self.active_id.take() {
            self.contexts[&id].unbind();
        }

        // When running in OSMesa mode with texture sharing,
        // a flush is required on any GL contexts to ensure
        // that read-back from the shared texture returns
        // valid data! This should be fine to have run on all
        // implementations - a single flush for each webgl
        // context at the start of a render frame should
        // incur minimal cost.
        for (_, context) in &self.contexts {
            context.make_current();
            context.apply_command(WebGLCommand::Flush);
            context.unbind();
        }
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

    // TODO(gw): Consider using strongly typed units here.
    hidpi_factor: f32,
    next_namespace_id: IdNamespace,

    gpu_cache: GpuCache,
    resource_cache: ResourceCache,

    frame_config: FrameBuilderConfig,
    documents: FastHashMap<DocumentId, Document>,

    notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
    webrender_context_handle: Option<GLContextHandleWrapper>,
    recorder: Option<Box<ApiRecordingReceiver>>,
    main_thread_dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>,

    vr_compositor_handler: Arc<Mutex<Option<Box<VRCompositorHandler>>>>,
    webgl: WebGL,

    enable_render_on_scroll: bool,
}

impl RenderBackend {
    pub fn new(
        api_rx: MsgReceiver<ApiMsg>,
        payload_rx: PayloadReceiver,
        payload_tx: PayloadSender,
        result_tx: Sender<ResultMsg>,
        hidpi_factor: f32,
        texture_cache: TextureCache,
        workers: Arc<ThreadPool>,
        notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
        webrender_context_handle: Option<GLContextHandleWrapper>,
        frame_config: FrameBuilderConfig,
        recorder: Option<Box<ApiRecordingReceiver>>,
        main_thread_dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>,
        blob_image_renderer: Option<Box<BlobImageRenderer>>,
        vr_compositor_handler: Arc<Mutex<Option<Box<VRCompositorHandler>>>>,
        enable_render_on_scroll: bool,
    ) -> RenderBackend {

        let resource_cache = ResourceCache::new(texture_cache,
                                                workers,
                                                blob_image_renderer,
                                                frame_config.cache_expiry_frames);

        register_thread_with_profiler("Backend".to_string());

        RenderBackend {
            api_rx,
            payload_rx,
            payload_tx,
            result_tx,
            hidpi_factor,

            resource_cache,
            gpu_cache: GpuCache::new(),
            frame_config,
            documents: FastHashMap::default(),
            next_namespace_id: IdNamespace(1),
            notifier,
            webrender_context_handle,
            recorder,
            main_thread_dispatcher,

            vr_compositor_handler,
            webgl: WebGL::new(),

            enable_render_on_scroll,
        }
    }

    fn process_document(&mut self, document_id: DocumentId, message: DocumentMsg,
                        frame_counter: u32, mut profile_counters: &mut BackendProfileCounters)
                        -> DocumentOp
    {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            DocumentMsg::SetPageZoom(factor) => {
                doc.page_zoom_factor = factor.get();
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
            DocumentMsg::SetWindowParameters{ window_size, inner_rect } => {
                doc.window_size = window_size;
                doc.inner_rect = inner_rect;
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

                self.resource_cache.update_resources(resources, &mut profile_counters.resources);

                let mut data;
                while {
                    data = self.payload_rx.recv_payload().unwrap();
                    data.epoch != epoch || data.pipeline_id != pipeline_id
                }{
                    self.payload_tx.send_payload(data).unwrap()
                }

                if let Some(ref mut r) = self.recorder {
                    r.write_payload(frame_counter, &data.to_data());
                }

                let built_display_list = BuiltDisplayList::from_data(
                    data.display_list_data,
                    list_descriptor
                );

                if !preserve_frame_state {
                    doc.frame.discard_frame_state_for_pipeline(pipeline_id);
                }

                let display_list_len = built_display_list.data().len();
                let (builder_start_time, builder_finish_time, send_start_time) = built_display_list.times();
                let display_list_received_time = precise_time_ns();

                {
                    self.webgl.flush();
                    let _timer = profile_counters.total_time.timer();
                    doc.scene.set_display_list(
                        pipeline_id,
                        epoch,
                        built_display_list,
                        background,
                        viewport_size,
                        content_size
                    );
                    doc.build_scene(&mut self.resource_cache, self.hidpi_factor);
                }

                if let Some(ref mut ros) = doc.render_on_scroll {
                    *ros = false; //wait for `GenerateFrame`
                }

                // Note: this isn't quite right as auxiliary values will be
                // pulled out somewhere in the prim_store, but aux values are
                // really simple and cheap to access, so it's not a big deal.
                let display_list_consumed_time = precise_time_ns();

                profile_counters.ipc.set(builder_start_time,
                                         builder_finish_time,
                                         send_start_time,
                                         display_list_received_time,
                                         display_list_consumed_time,
                                         display_list_len);

                DocumentOp::Built
            }
            DocumentMsg::SetRootPipeline(pipeline_id) => {
                profile_scope!("SetRootPipeline");

                doc.scene.set_root_pipeline_id(pipeline_id);
                if doc.scene.display_lists.get(&pipeline_id).is_some() {
                    self.webgl.flush();
                    let _timer = profile_counters.total_time.timer();
                    doc.build_scene(&mut self.resource_cache, self.hidpi_factor);
                    DocumentOp::Built
                } else {
                    DocumentOp::Nop
                }
            }
            DocumentMsg::Scroll(delta, cursor, move_phase) => {
                profile_scope!("Scroll");
                let _timer = profile_counters.total_time.timer();

                if doc.frame.scroll(delta, cursor, move_phase) && doc.render_on_scroll == Some(true) {
                    let frame = doc.render(&mut self.resource_cache,
                                           &mut self.gpu_cache,
                                           &mut profile_counters.resources,
                                           self.hidpi_factor);
                    DocumentOp::Scrolled(frame)
                } else {
                    DocumentOp::ScrolledNop
                }
            }
            DocumentMsg::ScrollNodeWithId(origin, id, clamp) => {
                profile_scope!("ScrollNodeWithScrollId");
                let _timer = profile_counters.total_time.timer();

                if doc.frame.scroll_node(origin, id, clamp) && doc.render_on_scroll == Some(true) {
                    let frame = doc.render(&mut self.resource_cache,
                                           &mut self.gpu_cache,
                                           &mut profile_counters.resources,
                                           self.hidpi_factor);
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
                    let frame = doc.render(&mut self.resource_cache,
                                           &mut self.gpu_cache,
                                           &mut profile_counters.resources,
                                           self.hidpi_factor);
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
                    self.webgl.flush();
                    doc.scene.properties.set_properties(property_bindings);
                    doc.build_scene(&mut self.resource_cache, self.hidpi_factor);
                }

                if let Some(ref mut ros) = doc.render_on_scroll {
                    *ros = true;
                }

                if doc.scene.root_pipeline_id.is_some() {
                    let frame = doc.render(&mut self.resource_cache,
                                           &mut self.gpu_cache,
                                           &mut profile_counters.resources,
                                           self.hidpi_factor);
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
                    notifier.unwrap()
                            .as_mut()
                            .unwrap()
                            .shut_down();
                    break;
                }
            };

            match msg {
                ApiMsg::UpdateResources(updates) => {
                    self.resource_cache.update_resources(updates, &mut profile_counters.resources);
                }
                ApiMsg::GetGlyphDimensions(font, glyph_keys, tx) => {
                    let mut glyph_dimensions = Vec::with_capacity(glyph_keys.len());
                    for glyph_key in &glyph_keys {
                        let glyph_dim = self.resource_cache.get_glyph_dimensions(&font, glyph_key);
                        glyph_dimensions.push(glyph_dim);
                    };
                    tx.send(glyph_dimensions).unwrap();
                }
                ApiMsg::GetGlyphIndices(font_key, text, tx) => {
                    let mut glyph_indices = Vec::new();
                    for ch in text.chars() {
                        let index = self.resource_cache.get_glyph_index(font_key, ch);
                        glyph_indices.push(index);
                    };
                    tx.send(glyph_indices).unwrap();
                }
                ApiMsg::CloneApi(sender) => {
                    let namespace = self.next_namespace_id;
                    self.next_namespace_id = IdNamespace(namespace.0 + 1);
                    sender.send(namespace).unwrap();
                }
                ApiMsg::AddDocument(document_id, initial_size) => {
                    let document = Document::new(self.frame_config.clone(),
                                                 initial_size,
                                                 self.enable_render_on_scroll);
                    self.documents.insert(document_id, document);
                }
                ApiMsg::UpdateDocument(document_id, doc_msg) => {
                    match self.process_document(document_id, doc_msg, frame_counter, &mut profile_counters) {
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
                            self.publish_frame_and_notify_compositor(document_id, frame, &mut profile_counters);
                        }
                    }
                }
                ApiMsg::DeleteDocument(document_id) => {
                    self.documents.remove(&document_id);
                }
                ApiMsg::RequestWebGLContext(size, attributes, tx) => {
                    if let Some(ref wrapper) = self.webrender_context_handle {
                        let dispatcher: Option<Box<GLContextDispatcher>> = if cfg!(target_os = "windows") {
                            Some(Box::new(WebRenderGLDispatcher {
                                dispatcher: Arc::clone(&self.main_thread_dispatcher)
                            }))
                        } else {
                            None
                        };

                        let result = wrapper.new_context(size, attributes, dispatcher);

                        match result {
                            Ok(ctx) => {
                                let (real_size, texture_id, limits) = ctx.get_info();
                                let id = self.webgl.register(ctx);

                                self.resource_cache
                                    .add_webgl_texture(id, SourceTexture::WebGL(texture_id),
                                                       real_size);

                                tx.send(Ok((id, limits))).unwrap();
                            },
                            Err(msg) => {
                                tx.send(Err(msg.to_owned())).unwrap();
                            }
                        }
                    } else {
                        tx.send(Err("Not implemented yet".to_owned())).unwrap();
                    }
                }
                ApiMsg::ResizeWebGLContext(context_id, size) => {
                    let ctx = self.webgl.activate(context_id);
                    match ctx.resize(&size) {
                        Ok(_) => {
                            // Update webgl texture size. Texture id may change too.
                            let (real_size, texture_id, _) = ctx.get_info();
                            self.resource_cache
                                .update_webgl_texture(context_id, SourceTexture::WebGL(texture_id),
                                                      real_size);
                        },
                        Err(msg) => {
                            error!("Error resizing WebGLContext: {}", msg);
                        }
                    }
                }
                ApiMsg::WebGLCommand(context_id, command) => {
                    // TODO: Buffer the commands and only apply them here if they need to
                    // be synchronous.
                    let ctx = self.webgl.activate(context_id);
                    ctx.apply_command(command);
                },

                ApiMsg::VRCompositorCommand(context_id, command) => {
                    self.webgl.activate(context_id);
                    self.handle_vr_compositor_command(context_id, command);
                }
                ApiMsg::ExternalEvent(evt) => {
                    let notifier = self.notifier.lock();
                    notifier.unwrap()
                            .as_mut()
                            .unwrap()
                            .external_event(evt);
                }
                ApiMsg::ClearNamespace(namespace_id) => {
                    self.resource_cache.clear_namespace(namespace_id);
                    let document_ids = self.documents.keys()
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
                    let msg = ResultMsg::UpdateResources { updates: pending_update, cancel_rendering: true };
                    self.result_tx.send(msg).unwrap();
                    // We use new_frame_ready to wake up the renderer and get the
                    // resource updates processed, but the UpdateResources message
                    // will cancel rendering the frame.
                    self.notifier.lock().unwrap().as_mut().unwrap().new_frame_ready();
                }
                ApiMsg::ShutDown => {
                    let notifier = self.notifier.lock();
                    notifier.unwrap()
                            .as_mut()
                            .unwrap()
                            .shut_down();
                    break;
                }
            }
        }
    }

    fn publish_frame(&mut self,
                     document_id: DocumentId,
                     frame: RendererFrame,
                     profile_counters: &mut BackendProfileCounters) {
        let pending_update = self.resource_cache.pending_updates();
        let msg = ResultMsg::NewFrame(document_id, frame, pending_update, profile_counters.clone());
        self.result_tx.send(msg).unwrap();
        profile_counters.reset();
    }

    fn publish_frame_and_notify_compositor(&mut self,
                                           document_id: DocumentId,
                                           frame: RendererFrame,
                                           profile_counters: &mut BackendProfileCounters) {
        self.publish_frame(document_id, frame, profile_counters);

        // TODO(gw): This is kindof bogus to have to lock the notifier
        //           each time it's used. This is due to some nastiness
        //           in initialization order for Servo. Perhaps find a
        //           cleaner way to do this, or use the OnceMutex on crates.io?
        let mut notifier = self.notifier.lock();
        notifier.as_mut().unwrap().as_mut().unwrap().new_frame_ready();
    }

    fn notify_compositor_of_new_scroll_frame(&mut self, composite_needed: bool) {
        // TODO(gw): This is kindof bogus to have to lock the notifier
        //           each time it's used. This is due to some nastiness
        //           in initialization order for Servo. Perhaps find a
        //           cleaner way to do this, or use the OnceMutex on crates.io?
        let mut notifier = self.notifier.lock();
        notifier.as_mut().unwrap().as_mut().unwrap().new_scroll_frame_ready(composite_needed);
    }

    fn handle_vr_compositor_command(&mut self, ctx_id: WebGLContextId, cmd: VRCompositorCommand) {
        let texture = match cmd {
            VRCompositorCommand::SubmitFrame(..) => {
                    match self.resource_cache.get_webgl_texture(&ctx_id).id {
                        SourceTexture::WebGL(texture_id) => {
                            let size = self.resource_cache.get_webgl_texture_size(&ctx_id);
                            Some((texture_id, size))
                        },
                        _=> None
                    }
            },
            _ => None
        };
        let mut handler = self.vr_compositor_handler.lock();
        handler.as_mut().unwrap().as_mut().unwrap().handle(cmd, texture);
    }
}

struct WebRenderGLDispatcher {
    dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>
}

impl GLContextDispatcher for WebRenderGLDispatcher {
    fn dispatch(&self, f: Box<Fn() + Send>) {
        let mut dispatcher = self.dispatcher.lock();
        dispatcher.as_mut().unwrap().as_mut().unwrap().dispatch(f);
    }
}

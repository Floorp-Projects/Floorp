/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use frame::Frame;
use frame_builder::FrameBuilderConfig;
use internal_types::{FontTemplate, SourceTexture, ResultMsg, RendererFrame};
use profiler::{BackendProfileCounters, TextureCacheProfileCounters};
use record::ApiRecordingReceiver;
use resource_cache::ResourceCache;
use scene::Scene;
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::sync::mpsc::Sender;
use texture_cache::TextureCache;
use thread_profiler::register_thread_with_profiler;
use threadpool::ThreadPool;
use webgl_types::{GLContextHandleWrapper, GLContextWrapper};
use webrender_traits::{DeviceIntPoint, DeviceUintPoint, DeviceUintRect, DeviceUintSize, LayerPoint};
use webrender_traits::{ApiMsg, AuxiliaryLists, BuiltDisplayList, IdNamespace, ImageData};
use webrender_traits::{PipelineId, RenderNotifier, RenderDispatcher, WebGLCommand, WebGLContextId};
use webrender_traits::channel::{PayloadSenderHelperMethods, PayloadReceiverHelperMethods, PayloadReceiver, PayloadSender, MsgReceiver};
use webrender_traits::{BlobImageRenderer, VRCompositorCommand, VRCompositorHandler};
#[cfg(feature = "webgl")]
use offscreen_gl_context::GLContextDispatcher;
#[cfg(not(feature = "webgl"))]
use webgl_types::GLContextDispatcher;

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
    page_zoom_factor: f32,
    pinch_zoom_factor: f32,
    pan: DeviceIntPoint,
    window_size: DeviceUintSize,
    inner_rect: DeviceUintRect,
    next_namespace_id: IdNamespace,

    resource_cache: ResourceCache,

    scene: Scene,
    frame: Frame,

    notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
    webrender_context_handle: Option<GLContextHandleWrapper>,
    webgl_contexts: HashMap<WebGLContextId, GLContextWrapper>,
    current_bound_webgl_context_id: Option<WebGLContextId>,
    recorder: Option<Box<ApiRecordingReceiver>>,
    main_thread_dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>,

    next_webgl_id: usize,

    vr_compositor_handler: Arc<Mutex<Option<Box<VRCompositorHandler>>>>
}

impl RenderBackend {
    pub fn new(api_rx: MsgReceiver<ApiMsg>,
               payload_rx: PayloadReceiver,
               payload_tx: PayloadSender,
               result_tx: Sender<ResultMsg>,
               hidpi_factor: f32,
               texture_cache: TextureCache,
               enable_aa: bool,
               workers: Arc<Mutex<ThreadPool>>,
               notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
               webrender_context_handle: Option<GLContextHandleWrapper>,
               config: FrameBuilderConfig,
               recorder: Option<Box<ApiRecordingReceiver>>,
               main_thread_dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>,
               blob_image_renderer: Option<Box<BlobImageRenderer>>,
               vr_compositor_handler: Arc<Mutex<Option<Box<VRCompositorHandler>>>>,
               initial_window_size: DeviceUintSize) -> RenderBackend {

        let resource_cache = ResourceCache::new(texture_cache, workers, blob_image_renderer, enable_aa);

        register_thread_with_profiler("Backend".to_string());

        RenderBackend {
            api_rx: api_rx,
            payload_rx: payload_rx,
            payload_tx: payload_tx,
            result_tx: result_tx,
            hidpi_factor: hidpi_factor,
            page_zoom_factor: 1.0,
            pinch_zoom_factor: 1.0,
            pan: DeviceIntPoint::zero(),
            resource_cache: resource_cache,
            scene: Scene::new(),
            frame: Frame::new(config),
            next_namespace_id: IdNamespace(1),
            notifier: notifier,
            webrender_context_handle: webrender_context_handle,
            webgl_contexts: HashMap::new(),
            current_bound_webgl_context_id: None,
            recorder: recorder,
            main_thread_dispatcher: main_thread_dispatcher,
            next_webgl_id: 0,
            vr_compositor_handler: vr_compositor_handler,
            window_size: initial_window_size,
            inner_rect: DeviceUintRect::new(DeviceUintPoint::zero(), initial_window_size),
        }
    }

    pub fn run(&mut self, mut profile_counters: BackendProfileCounters) {
        let mut frame_counter: u32 = 0;

        loop {
            let msg = self.api_rx.recv();
            profile_scope!("handle_msg");
            match msg {
                Ok(msg) => {
                    if let Some(ref mut r) = self.recorder {
                        r.write_msg(frame_counter, &msg);
                    }
                    match msg {
                        ApiMsg::AddRawFont(id, bytes, index) => {
                            profile_counters.resources.font_templates.inc(bytes.len());
                            self.resource_cache
                                .add_font_template(id, FontTemplate::Raw(Arc::new(bytes), index));
                        }
                        ApiMsg::AddNativeFont(id, native_font_handle) => {
                            self.resource_cache
                                .add_font_template(id, FontTemplate::Native(native_font_handle));
                        }
                        ApiMsg::DeleteFont(id) => {
                            self.resource_cache.delete_font_template(id);
                        }
                        ApiMsg::GetGlyphDimensions(glyph_keys, tx) => {
                            let mut glyph_dimensions = Vec::with_capacity(glyph_keys.len());
                            for glyph_key in &glyph_keys {
                                let glyph_dim = self.resource_cache.get_glyph_dimensions(glyph_key);
                                glyph_dimensions.push(glyph_dim);
                            };
                            tx.send(glyph_dimensions).unwrap();
                        }
                        ApiMsg::AddImage(id, descriptor, data, tiling) => {
                            if let ImageData::Raw(ref bytes) = data {
                                profile_counters.resources.image_templates.inc(bytes.len());
                            }
                            self.resource_cache.add_image_template(id, descriptor, data, tiling);
                        }
                        ApiMsg::UpdateImage(id, descriptor, bytes, dirty_rect) => {
                            self.resource_cache.update_image_template(id, descriptor, bytes, dirty_rect);
                        }
                        ApiMsg::DeleteImage(id) => {
                            self.resource_cache.delete_image_template(id);
                        }
                        ApiMsg::SetPageZoom(factor) => {
                            self.page_zoom_factor = factor.get();
                        }
                        ApiMsg::SetPinchZoom(factor) => {
                            self.pinch_zoom_factor = factor.get();
                        }
                        ApiMsg::SetPan(pan) => {
                            self.pan = pan;
                        }
                        ApiMsg::SetWindowParameters(window_size, inner_rect) => {
                            self.window_size = window_size;
                            self.inner_rect = inner_rect;
                        }
                        ApiMsg::CloneApi(sender) => {
                            let result = self.next_namespace_id;

                            let IdNamespace(id_namespace) = self.next_namespace_id;
                            self.next_namespace_id = IdNamespace(id_namespace + 1);

                            sender.send(result).unwrap();
                        }
                        ApiMsg::SetDisplayList(background_color,
                                               epoch,
                                               pipeline_id,
                                               viewport_size,
                                               display_list_descriptor,
                                               auxiliary_lists_descriptor,
                                               preserve_frame_state) => {
                            profile_scope!("SetDisplayList");
                            let mut leftover_auxiliary_data = vec![];
                            let mut auxiliary_data;
                            loop {
                                auxiliary_data = self.payload_rx.recv_payload().unwrap();
                                {
                                    if auxiliary_data.epoch == epoch &&
                                       auxiliary_data.pipeline_id == pipeline_id {
                                        break
                                    }
                                }
                                leftover_auxiliary_data.push(auxiliary_data)
                            }
                            for leftover_auxiliary_data in leftover_auxiliary_data {
                                self.payload_tx.send_payload(leftover_auxiliary_data).unwrap()
                            }
                            if let Some(ref mut r) = self.recorder {
                                r.write_payload(frame_counter, &auxiliary_data.to_data());
                            }

                            let built_display_list =
                                BuiltDisplayList::from_data(auxiliary_data.display_list_data,
                                                            display_list_descriptor);
                            let auxiliary_lists =
                                AuxiliaryLists::from_data(auxiliary_data.auxiliary_lists_data,
                                                          auxiliary_lists_descriptor);

                            if !preserve_frame_state {
                                self.discard_frame_state_for_pipeline(pipeline_id);
                            }
                            
                            let counters = &mut profile_counters.ipc;
                            profile_counters.total_time.profile(|| {
                                self.scene.set_display_list(pipeline_id,
                                                            epoch,
                                                            built_display_list,
                                                            background_color,
                                                            viewport_size,
                                                            auxiliary_lists,
                                                            counters);
                                self.build_scene();
                            });
                        }
                        ApiMsg::SetRootPipeline(pipeline_id) => {
                            profile_scope!("SetRootPipeline");
                            self.scene.set_root_pipeline_id(pipeline_id);

                            if self.scene.display_lists.get(&pipeline_id).is_none() {
                                continue;
                            }

                            profile_counters.total_time.profile(|| {
                                self.build_scene();
                            })
                        }
                        ApiMsg::Scroll(delta, cursor, move_phase) => {
                            profile_scope!("Scroll");
                            let frame = {
                                let counters = &mut profile_counters.resources.texture_cache;
                                profile_counters.total_time.profile(|| {
                                    if self.frame.scroll(delta, cursor, move_phase) {
                                        Some(self.render(counters))
                                    } else {
                                        None
                                    }
                                })
                            };

                            match frame {
                                Some(frame) => {
                                    self.publish_frame(frame, &mut profile_counters);
                                    self.notify_compositor_of_new_scroll_frame(true)
                                }
                                None => self.notify_compositor_of_new_scroll_frame(false),
                            }
                        }
                        ApiMsg::ScrollNodeWithId(origin, id) => {
                            profile_scope!("ScrollNodeWithScrollId");
                            let frame = {
                                let counters = &mut profile_counters.resources.texture_cache;
                                profile_counters.total_time.profile(|| {
                                    if self.frame.scroll_nodes(origin, id) {
                                        Some(self.render(counters))
                                    } else {
                                        None
                                    }
                                })
                            };

                            match frame {
                                Some(frame) => {
                                    self.publish_frame(frame, &mut profile_counters);
                                    self.notify_compositor_of_new_scroll_frame(true)
                                }
                                None => self.notify_compositor_of_new_scroll_frame(false),
                            }

                        }
                        ApiMsg::TickScrollingBounce => {
                            profile_scope!("TickScrollingBounce");
                            let frame = {
                                let counters = &mut profile_counters.resources.texture_cache;
                                profile_counters.total_time.profile(|| {
                                    self.frame.tick_scrolling_bounce_animations();
                                    self.render(counters)
                                })
                            };

                            self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                        }
                        ApiMsg::TranslatePointToLayerSpace(..) => {
                            panic!("unused api - remove from webrender_traits");
                        }
                        ApiMsg::GetScrollNodeState(tx) => {
                            profile_scope!("GetScrollNodeState");
                            tx.send(self.frame.get_scroll_node_state()).unwrap()
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
                                        let id = WebGLContextId(self.next_webgl_id);
                                        self.next_webgl_id += 1;

                                        let (real_size, texture_id, limits) = ctx.get_info();

                                        self.webgl_contexts.insert(id, ctx);

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
                            let ctx = self.webgl_contexts.get_mut(&context_id).unwrap();
                            ctx.make_current();
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
                            let ctx = &self.webgl_contexts[&context_id];
                            ctx.make_current();
                            ctx.apply_command(command);
                            self.current_bound_webgl_context_id = Some(context_id);
                        },

                        ApiMsg::VRCompositorCommand(context_id, command) => {
                            self.handle_vr_compositor_command(context_id, command);
                        }
                        ApiMsg::GenerateFrame(property_bindings) => {
                            profile_scope!("GenerateFrame");

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
                                self.scene.properties.set_properties(property_bindings);
                                profile_counters.total_time.profile(|| {
                                    self.build_scene();
                                });
                            }

                            let frame = {
                                let counters = &mut profile_counters.resources.texture_cache;
                                profile_counters.total_time.profile(|| {
                                    self.render(counters)
                                })
                            };
                            if self.scene.root_pipeline_id.is_some() {
                                self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                                frame_counter += 1;
                            }
                        }
                        ApiMsg::ExternalEvent(evt) => {
                            let notifier = self.notifier.lock();
                            notifier.unwrap()
                                    .as_mut()
                                    .unwrap()
                                    .external_event(evt);
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
                Err(..) => {
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

    fn discard_frame_state_for_pipeline(&mut self, pipeline_id: PipelineId) {
        self.frame.discard_frame_state_for_pipeline(pipeline_id);
    }

    fn accumulated_scale_factor(&self) -> f32 {
        self.hidpi_factor * self.page_zoom_factor * self.pinch_zoom_factor
    }

    fn build_scene(&mut self) {
        // Flatten the stacking context hierarchy
        if let Some(id) = self.current_bound_webgl_context_id {
            self.webgl_contexts[&id].unbind();
            self.current_bound_webgl_context_id = None;
        }

        // When running in OSMesa mode with texture sharing,
        // a flush is required on any GL contexts to ensure
        // that read-back from the shared texture returns
        // valid data! This should be fine to have run on all
        // implementations - a single flush for each webgl
        // context at the start of a render frame should
        // incur minimal cost.
        for (_, webgl_context) in &self.webgl_contexts {
            webgl_context.make_current();
            webgl_context.apply_command(WebGLCommand::Flush);
            webgl_context.unbind();
        }

        let accumulated_scale_factor = self.accumulated_scale_factor();
        self.frame.create(&self.scene,
                          &mut self.resource_cache,
                          self.window_size,
                          self.inner_rect,
                          accumulated_scale_factor);
    }

    fn render(&mut self,
              texture_cache_profile: &mut TextureCacheProfileCounters)
              -> RendererFrame {
        let accumulated_scale_factor = self.accumulated_scale_factor();
        let pan = LayerPoint::new(self.pan.x as f32 / accumulated_scale_factor,
                                  self.pan.y as f32 / accumulated_scale_factor);
        let frame = self.frame.build(&mut self.resource_cache,
                                     &self.scene.pipeline_auxiliary_lists,
                                     accumulated_scale_factor,
                                     pan,
                                     texture_cache_profile);
        frame
    }

    fn publish_frame(&mut self,
                     frame: RendererFrame,
                     profile_counters: &mut BackendProfileCounters) {
        let pending_update = self.resource_cache.pending_updates();
        let msg = ResultMsg::NewFrame(frame, pending_update, profile_counters.clone());
        self.result_tx.send(msg).unwrap();
        profile_counters.reset();
    }

    fn publish_frame_and_notify_compositor(&mut self,
                                           frame: RendererFrame,
                                           profile_counters: &mut BackendProfileCounters) {
        self.publish_frame(frame, profile_counters);

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
                    match self.resource_cache.get_webgl_texture(&ctx_id).texture_id {
                        SourceTexture::WebGL(texture_id) => Some(texture_id),
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

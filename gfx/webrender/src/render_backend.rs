/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use byteorder::{LittleEndian, ReadBytesExt};
use frame::Frame;
use internal_types::{FontTemplate, GLContextHandleWrapper, GLContextWrapper, ResultMsg, RendererFrame};
use ipc_channel::ipc::{IpcBytesReceiver, IpcBytesSender, IpcReceiver};
use profiler::BackendProfileCounters;
use resource_cache::{DummyResources, ResourceCache};
use scene::Scene;
use std::collections::HashMap;
use std::fs;
use std::io::{Cursor, Read};
use std::sync::{Arc, Mutex};
use std::sync::mpsc::Sender;
use texture_cache::TextureCache;
use webrender_traits::{ApiMsg, AuxiliaryLists, BuiltDisplayList, IdNamespace};
use webrender_traits::{RenderNotifier, RenderDispatcher, WebGLCommand, WebGLContextId};
use device::TextureId;
use record;
use tiling::FrameBuilderConfig;
use offscreen_gl_context::GLContextDispatcher;

/// The render backend is responsible for transforming high level display lists into
/// GPU-friendly work which is then submitted to the renderer in the form of a frame::Frame.
///
/// The render backend operates on its own thread.
pub struct RenderBackend {
    api_rx: IpcReceiver<ApiMsg>,
    payload_rx: IpcBytesReceiver,
    payload_tx: IpcBytesSender,
    result_tx: Sender<ResultMsg>,

    device_pixel_ratio: f32,
    next_namespace_id: IdNamespace,

    resource_cache: ResourceCache,
    dummy_resources: DummyResources,

    scene: Scene,
    frame: Frame,

    notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
    webrender_context_handle: Option<GLContextHandleWrapper>,
    webgl_contexts: HashMap<WebGLContextId, GLContextWrapper>,
    current_bound_webgl_context_id: Option<WebGLContextId>,
    enable_recording: bool,
    main_thread_dispatcher: Arc<Mutex<Option<Box<RenderDispatcher>>>>,

    next_webgl_id: usize,
}

impl RenderBackend {
    pub fn new(api_rx: IpcReceiver<ApiMsg>,
               payload_rx: IpcBytesReceiver,
               payload_tx: IpcBytesSender,
               result_tx: Sender<ResultMsg>,
               device_pixel_ratio: f32,
               texture_cache: TextureCache,
               dummy_resources: DummyResources,
               enable_aa: bool,
               notifier: Arc<Mutex<Option<Box<RenderNotifier>>>>,
               webrender_context_handle: Option<GLContextHandleWrapper>,
               config: FrameBuilderConfig,
               debug: bool,
               enable_recording:bool,
               main_thread_dispatcher:  Arc<Mutex<Option<Box<RenderDispatcher>>>>) -> RenderBackend {

        let resource_cache = ResourceCache::new(texture_cache,
                                                device_pixel_ratio,
                                                enable_aa);

        RenderBackend {
            api_rx: api_rx,
            payload_rx: payload_rx,
            payload_tx: payload_tx,
            result_tx: result_tx,
            device_pixel_ratio: device_pixel_ratio,
            resource_cache: resource_cache,
            dummy_resources: dummy_resources,
            scene: Scene::new(),
            frame: Frame::new(debug, config),
            next_namespace_id: IdNamespace(1),
            notifier: notifier,
            webrender_context_handle: webrender_context_handle,
            webgl_contexts: HashMap::new(),
            current_bound_webgl_context_id: None,
            enable_recording:enable_recording,
            main_thread_dispatcher: main_thread_dispatcher,
            next_webgl_id: 0,
        }
    }

    pub fn run(&mut self) {
        let mut profile_counters = BackendProfileCounters::new();
        let mut frame_counter: u32 = 0;
        if self.enable_recording {
            fs::create_dir("record").ok();
        }
        loop {
            let msg = self.api_rx.recv();
            match msg {
                Ok(msg) => {
                    if self.enable_recording {
                        record::write_msg(frame_counter, &msg);
                    }
                    match msg {
                        ApiMsg::AddRawFont(id, bytes) => {
                            profile_counters.font_templates.inc(bytes.len());
                            self.resource_cache
                                .add_font_template(id, FontTemplate::Raw(Arc::new(bytes)));
                        }
                        ApiMsg::AddNativeFont(id, native_font_handle) => {
                            self.resource_cache
                                .add_font_template(id, FontTemplate::Native(native_font_handle));
                        }
                        ApiMsg::GetGlyphDimensions(glyph_keys, tx) => {
                            let mut glyph_dimensions = Vec::with_capacity(glyph_keys.len());
                            for glyph_key in &glyph_keys {
                                let glyph_dim = self.resource_cache.get_glyph_dimensions(glyph_key);
                                glyph_dimensions.push(glyph_dim);
                            };
                            tx.send(glyph_dimensions).unwrap();
                        }
                        ApiMsg::AddImage(id, width, height, stride, format, bytes) => {
                            profile_counters.image_templates.inc(bytes.len());
                            self.resource_cache.add_image_template(id,
                                                                   width,
                                                                   height,
                                                                   stride,
                                                                   format,
                                                                   bytes);
                        }
                        ApiMsg::UpdateImage(id, width, height, format, bytes) => {
                            self.resource_cache.update_image_template(id,
                                                                      width,
                                                                      height,
                                                                      format,
                                                                      bytes);
                        }
                        ApiMsg::DeleteImage(id) => {
                            self.resource_cache.delete_image_template(id);
                        }
                        ApiMsg::CloneApi(sender) => {
                            let result = self.next_namespace_id;

                            let IdNamespace(id_namespace) = self.next_namespace_id;
                            self.next_namespace_id = IdNamespace(id_namespace + 1);

                            sender.send(result).unwrap();
                        }
                        ApiMsg::SetRootStackingContext(stacking_context_id,
                                                       background_color,
                                                       epoch,
                                                       pipeline_id,
                                                       viewport_size,
                                                       stacking_contexts,
                                                       display_lists,
                                                       auxiliary_lists_descriptor) => {
                            for (id, stacking_context) in stacking_contexts.into_iter() {
                                self.scene.add_stacking_context(id,
                                                                pipeline_id,
                                                                epoch,
                                                                stacking_context);
                            }

                            let mut leftover_auxiliary_data = vec![];
                            let mut auxiliary_data;
                            loop {
                                auxiliary_data = self.payload_rx.recv().unwrap();
                                {
                                    let mut payload_reader = Cursor::new(&auxiliary_data[..]);
                                    let payload_stacking_context_id =
                                        payload_reader.read_u32::<LittleEndian>().unwrap();
                                    let payload_epoch =
                                        payload_reader.read_u32::<LittleEndian>().unwrap();
                                    if payload_epoch == epoch.0 &&
                                            payload_stacking_context_id == stacking_context_id.0 {
                                        break
                                    }
                                }
                                leftover_auxiliary_data.push(auxiliary_data)
                            }
                            for leftover_auxiliary_data in leftover_auxiliary_data {
                                self.payload_tx.send(&leftover_auxiliary_data[..]).unwrap()
                            }
                            if self.enable_recording {
                                record::write_payload(frame_counter, &auxiliary_data);
                            }
                            let mut auxiliary_data = Cursor::new(&mut auxiliary_data[8..]);
                            for (display_list_id,
                                 display_list_descriptor) in display_lists.into_iter() {
                                let mut built_display_list_data =
                                    vec![0; display_list_descriptor.size()];
                                auxiliary_data.read_exact(&mut built_display_list_data[..])
                                              .unwrap();
                                let built_display_list =
                                    BuiltDisplayList::from_data(built_display_list_data,
                                                                display_list_descriptor);
                                self.scene.add_display_list(display_list_id,
                                                            pipeline_id,
                                                            epoch,
                                                            built_display_list,
                                                            &mut self.resource_cache);
                            }

                            let mut auxiliary_lists_data =
                                vec![0; auxiliary_lists_descriptor.size()];
                            auxiliary_data.read_exact(&mut auxiliary_lists_data[..]).unwrap();
                            let auxiliary_lists =
                                AuxiliaryLists::from_data(auxiliary_lists_data,
                                                          auxiliary_lists_descriptor);
                            let frame = profile_counters.total_time.profile(|| {
                                self.scene.set_root_stacking_context(pipeline_id,
                                                                     epoch,
                                                                     stacking_context_id,
                                                                     background_color,
                                                                     viewport_size,
                                                                     &mut self.resource_cache,
                                                                     auxiliary_lists);

                                self.build_scene();
                                self.render()
                            });

                            if self.scene.root_pipeline_id.is_some() {
                                self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                                frame_counter += 1;
                            }
                        }
                        ApiMsg::SetRootPipeline(pipeline_id) => {
                            let frame = profile_counters.total_time.profile(|| {
                                self.scene.set_root_pipeline_id(pipeline_id);

                                self.build_scene();
                                self.render()
                            });

                            // the root pipeline is guaranteed to be Some() at this point
                            self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                            frame_counter += 1;
                        }
                        ApiMsg::Scroll(delta, cursor, move_phase) => {
                            let frame = profile_counters.total_time.profile(|| {
                                if self.frame.scroll(delta, cursor, move_phase) {
                                    Some(self.render())
                                } else {
                                    None
                                }
                            });

                            match frame {
                                Some(frame) => {
                                    self.publish_frame(frame, &mut profile_counters);
                                    self.notify_compositor_of_new_scroll_frame(true)
                                }
                                None => self.notify_compositor_of_new_scroll_frame(false),
                            }
                        }
                        ApiMsg::TickScrollingBounce => {
                            let frame = profile_counters.total_time.profile(|| {
                                self.frame.tick_scrolling_bounce_animations();
                                self.render()
                            });

                            self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                        }
                        ApiMsg::SetScrollOffset(scroll_id, offset) => {
                            if let Some(ref mut layer) = self.frame.layers.get_mut(&scroll_id) {
                                layer.scrolling.offset.x = offset.x;
                                layer.scrolling.offset.y = offset.y;
                            }
                        }
                        ApiMsg::GenerateFrame => {
                            let frame = profile_counters.total_time.profile(|| {
                                self.render()
                            });
                            self.publish_frame_and_notify_compositor(frame, &mut profile_counters);
                        }
                        ApiMsg::TranslatePointToLayerSpace(..) => {
                            panic!("unused api - remove from webrender_traits");
                        }
                        ApiMsg::GetScrollLayerState(tx) => {
                            tx.send(self.frame.get_scroll_layer_state())
                              .unwrap()
                        }
                        ApiMsg::RequestWebGLContext(size, attributes, tx) => {
                            if let Some(ref wrapper) = self.webrender_context_handle {
                                let dispatcher: Option<Box<GLContextDispatcher>> = if cfg!(target_os = "windows") {
                                    Some(Box::new(WebRenderGLDispatcher {
                                        dispatcher: self.main_thread_dispatcher.clone()
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
                                            .add_webgl_texture(id, TextureId::new(texture_id), real_size);

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
                                        .update_webgl_texture(context_id, TextureId::new(texture_id), real_size);
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
                        }
                    }
                }
                Err(..) => {
                    break;
                }
            }
        }
    }

    fn build_scene(&mut self) {
        // Flatten the stacking context hierarchy
        let mut new_pipeline_sizes = HashMap::new();

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

        self.frame.create(&self.scene,
                          &mut self.resource_cache,
                          &self.dummy_resources,
                          &mut new_pipeline_sizes,
                          self.device_pixel_ratio);

        let mut updated_pipeline_sizes = HashMap::new();

        for (pipeline_id, old_size) in self.scene.pipeline_sizes.drain() {
            let new_size = new_pipeline_sizes.remove(&pipeline_id);

            match new_size {
                Some(new_size) => {
                    // Exists in both old and new -> check if size changed
                    if new_size != old_size {
                        let mut notifier = self.notifier.lock();
                        notifier.as_mut()
                                .unwrap()
                                .as_mut()
                                .unwrap()
                                .pipeline_size_changed(pipeline_id, Some(new_size));
                    }

                    // Re-insert
                    updated_pipeline_sizes.insert(pipeline_id, new_size);
                }
                None => {
                    // Was existing, not in current frame anymore
                        let mut notifier = self.notifier.lock();
                        notifier.as_mut()
                                .unwrap()
                                .as_mut()
                                .unwrap()
                                .pipeline_size_changed(pipeline_id, None);
                }
            }
        }

        // Any remaining items are new pipelines
        for (pipeline_id, new_size) in new_pipeline_sizes.drain() {
            let mut notifier = self.notifier.lock();
            notifier.as_mut()
                    .unwrap()
                    .as_mut()
                    .unwrap()
                    .pipeline_size_changed(pipeline_id, Some(new_size));
            updated_pipeline_sizes.insert(pipeline_id, new_size);
        }

        self.scene.pipeline_sizes = updated_pipeline_sizes;
    }

    fn render(&mut self) -> RendererFrame {
        let frame = self.frame.build(&mut self.resource_cache,
                                     &self.scene.pipeline_auxiliary_lists,
                                     self.device_pixel_ratio);

        let pending_update = self.resource_cache.pending_updates();
        if !pending_update.updates.is_empty() {
            self.result_tx.send(ResultMsg::UpdateTextureCache(pending_update)).unwrap();
        }

        frame
    }

    fn publish_frame(&mut self,
                     frame: RendererFrame,
                     profile_counters: &mut BackendProfileCounters) {
        let msg = ResultMsg::NewFrame(frame, profile_counters.clone());
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


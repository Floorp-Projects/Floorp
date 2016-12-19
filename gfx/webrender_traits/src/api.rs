/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use byteorder::{LittleEndian, WriteBytesExt};
use channel::{self, MsgSender, PayloadHelperMethods, PayloadSender};
use offscreen_gl_context::{GLContextAttributes, GLLimits};
use std::cell::Cell;
use {ApiMsg, ColorF, DisplayListBuilder, Epoch};
use {FontKey, IdNamespace, ImageFormat, ImageKey, NativeFontHandle, PipelineId};
use {RenderApiSender, ResourceId, ScrollEventPhase, ScrollLayerState, ScrollLocation, ServoScrollRootId};
use {GlyphKey, GlyphDimensions, ImageData, WebGLContextId, WebGLCommand};
use {DeviceIntSize, LayoutPoint, LayoutSize, WorldPoint};
use VRCompositorCommand;

impl RenderApiSender {
    pub fn new(api_sender: MsgSender<ApiMsg>,
               payload_sender: PayloadSender)
               -> RenderApiSender {
        RenderApiSender {
            api_sender: api_sender,
            payload_sender: payload_sender,
        }
    }

    pub fn create_api(&self) -> RenderApi {
        let RenderApiSender {
            ref api_sender,
            ref payload_sender
        } = *self;
        let (sync_tx, sync_rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::CloneApi(sync_tx);
        api_sender.send(msg).unwrap();
        RenderApi {
            api_sender: api_sender.clone(),
            payload_sender: payload_sender.clone(),
            id_namespace: sync_rx.recv().unwrap(),
            next_id: Cell::new(ResourceId(0)),
        }
    }
}

pub struct RenderApi {
    pub api_sender: MsgSender<ApiMsg>,
    pub payload_sender: PayloadSender,
    pub id_namespace: IdNamespace,
    pub next_id: Cell<ResourceId>,
}

impl RenderApi {
    pub fn clone_sender(&self) -> RenderApiSender {
        RenderApiSender::new(self.api_sender.clone(), self.payload_sender.clone())
    }

    pub fn add_raw_font(&self, bytes: Vec<u8>) -> FontKey {
        let new_id = self.next_unique_id();
        let key = FontKey::new(new_id.0, new_id.1);
        let msg = ApiMsg::AddRawFont(key, bytes);
        self.api_sender.send(msg).unwrap();
        key
    }

    pub fn add_native_font(&self, native_font_handle: NativeFontHandle) -> FontKey {
        let new_id = self.next_unique_id();
        let key = FontKey::new(new_id.0, new_id.1);
        let msg = ApiMsg::AddNativeFont(key, native_font_handle);
        self.api_sender.send(msg).unwrap();
        key
    }

    /// Gets the dimensions for the supplied glyph keys
    ///
    /// Note: Internally, the internal texture cache doesn't store
    /// 'empty' textures (height or width = 0)
    /// This means that glyph dimensions e.g. for spaces (' ') will mostly be None.
    pub fn get_glyph_dimensions(&self, glyph_keys: Vec<GlyphKey>)
                                -> Vec<Option<GlyphDimensions>> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::GetGlyphDimensions(glyph_keys, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    /// Creates an `ImageKey`.
    pub fn alloc_image(&self) -> ImageKey {
        let new_id = self.next_unique_id();
        ImageKey::new(new_id.0, new_id.1)
    }

    /// Adds an image and returns the corresponding `ImageKey`.
    pub fn add_image(&self,
                     width: u32,
                     height: u32,
                     stride: Option<u32>,
                     format: ImageFormat,
                     data: ImageData) -> ImageKey {
        let key = self.alloc_image();
        let msg = ApiMsg::AddImage(key, width, height, stride, format, data);
        self.api_sender.send(msg).unwrap();
        key
    }

    /// Updates a specific image.
    ///
    /// Currently doesn't support changing dimensions or format by updating.
    // TODO: Support changing dimensions (and format) during image update?
    pub fn update_image(&self,
                        key: ImageKey,
                        width: u32,
                        height: u32,
                        format: ImageFormat,
                        bytes: Vec<u8>) {
        let msg = ApiMsg::UpdateImage(key, width, height, format, bytes);
        self.api_sender.send(msg).unwrap();
    }

    /// Deletes the specific image.
    pub fn delete_image(&self, key: ImageKey) {
        let msg = ApiMsg::DeleteImage(key);
        self.api_sender.send(msg).unwrap();
    }

    /// Sets the root pipeline.
    ///
    /// # Examples
    ///
    /// ```ignore
    /// let (mut renderer, sender) = webrender::renderer::Renderer::new(opts);
    /// let api = sender.create_api();
    /// ...
    /// let pipeline_id = PipelineId(0,0);
    /// api.set_root_pipeline(pipeline_id);
    /// ```
    pub fn set_root_pipeline(&self, pipeline_id: PipelineId) {
        let msg = ApiMsg::SetRootPipeline(pipeline_id);
        self.api_sender.send(msg).unwrap();
    }

    /// Supplies a new frame to WebRender.
    ///
    /// Non-blocking, it notifies a worker process which processes the display list.
    /// When it's done and a RenderNotifier has been set in `webrender::renderer::Renderer`,
    /// [new_frame_ready()][notifier] gets called.
    ///
    /// Note: Scrolling doesn't require an own Frame.
    ///
    /// Arguments:
    ///
    /// * `background_color`: The background color of this pipeline.
    /// * `epoch`: The unique Frame ID, monotonically increasing.
    /// * `pipeline_id`: The ID of the pipeline that is supplying this display list.
    /// * `viewport_size`: The size of the viewport for this frame.
    /// * `display_list`: The root Display list used in this frame.
    /// * `auxiliary_lists`: Various items that the display lists and stacking contexts reference.
    ///
    /// [notifier]: trait.RenderNotifier.html#tymethod.new_frame_ready
    pub fn set_root_display_list(&self,
                                 background_color: Option<ColorF>,
                                 epoch: Epoch,
                                 viewport_size: LayoutSize,
                                 builder: DisplayListBuilder) {
        let pipeline_id = builder.pipeline_id;
        let (display_list, auxiliary_lists) = builder.finalize();
        let msg = ApiMsg::SetRootDisplayList(background_color,
                                             epoch,
                                             pipeline_id,
                                             viewport_size,
                                             display_list.descriptor().clone(),
                                             *auxiliary_lists.descriptor());
        self.api_sender.send(msg).unwrap();

        let mut payload = vec![];
        payload.write_u32::<LittleEndian>(epoch.0).unwrap();
        payload.extend_from_slice(display_list.data());
        payload.extend_from_slice(auxiliary_lists.data());
        self.payload_sender.send_vec(payload).unwrap();
    }

    /// Scrolls the scrolling layer under the `cursor`
    ///
    /// Webrender looks for the layer closest to the user
    /// which has `ScrollPolicy::Scrollable` set.
    pub fn scroll(&self, scroll_location: ScrollLocation, cursor: WorldPoint, phase: ScrollEventPhase) {
        let msg = ApiMsg::Scroll(scroll_location, cursor, phase);
        self.api_sender.send(msg).unwrap();
    }

    pub fn scroll_layers_with_scroll_root_id(&self,
                                             new_scroll_origin: LayoutPoint,
                                             pipeline_id: PipelineId,
                                             scroll_root_id: ServoScrollRootId) {
        let msg = ApiMsg::ScrollLayersWithScrollId(new_scroll_origin, pipeline_id, scroll_root_id);
        self.api_sender.send(msg).unwrap();
    }

    pub fn tick_scrolling_bounce_animations(&self) {
        let msg = ApiMsg::TickScrollingBounce;
        self.api_sender.send(msg).unwrap();
    }

    /// Translates a point from viewport coordinates to layer space
    pub fn translate_point_to_layer_space(&self, point: &WorldPoint)
                                          -> (LayoutPoint, PipelineId) {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::TranslatePointToLayerSpace(*point, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn get_scroll_layer_state(&self) -> Vec<ScrollLayerState> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::GetScrollLayerState(tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn request_webgl_context(&self, size: &DeviceIntSize, attributes: GLContextAttributes)
                                 -> Result<(WebGLContextId, GLLimits), String> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::RequestWebGLContext(*size, attributes, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn resize_webgl_context(&self, context_id: WebGLContextId, size: &DeviceIntSize) {
        let msg = ApiMsg::ResizeWebGLContext(context_id, *size);
        self.api_sender.send(msg).unwrap();
    }

    pub fn send_webgl_command(&self, context_id: WebGLContextId, command: WebGLCommand) {
        let msg = ApiMsg::WebGLCommand(context_id, command);
        self.api_sender.send(msg).unwrap();
    }

    pub fn generate_frame(&self) {
        let msg = ApiMsg::GenerateFrame;
        self.api_sender.send(msg).unwrap();
    }

    pub fn send_vr_compositor_command(&self, context_id: WebGLContextId, command: VRCompositorCommand) {
        let msg = ApiMsg::VRCompositorCommand(context_id, command);
        self.api_sender.send(msg).unwrap();
    }

    #[inline]
    fn next_unique_id(&self) -> (u32, u32) {
        let IdNamespace(namespace) = self.id_namespace;
        let ResourceId(id) = self.next_id.get();
        self.next_id.set(ResourceId(id + 1));
        (namespace, id)
    }
}


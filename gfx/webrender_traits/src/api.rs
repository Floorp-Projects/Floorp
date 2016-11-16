/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use byteorder::{LittleEndian, WriteBytesExt};
use euclid::{Point2D, Size2D};
use ipc_channel::ipc::{self, IpcBytesSender, IpcSender};
use offscreen_gl_context::{GLContextAttributes, GLLimits};
use std::cell::Cell;
use {ApiMsg, AuxiliaryLists, BuiltDisplayList, ColorF, DisplayListId, Epoch};
use {FontKey, IdNamespace, ImageFormat, ImageKey, NativeFontHandle, PipelineId};
use {RenderApiSender, ResourceId, ScrollEventPhase, ScrollLayerId, ScrollLayerState};
use {StackingContext, StackingContextId, WebGLContextId, WebGLCommand};
use {GlyphKey, GlyphDimensions};

impl RenderApiSender {
    pub fn new(api_sender: IpcSender<ApiMsg>,
               payload_sender: IpcBytesSender)
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
        let (sync_tx, sync_rx) = ipc::channel().unwrap();
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
    pub api_sender: IpcSender<ApiMsg>,
    pub payload_sender: IpcBytesSender,
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
        let (tx, rx) = ipc::channel().unwrap();
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
                     bytes: Vec<u8>) -> ImageKey {
        let new_id = self.next_unique_id();
        let key = ImageKey::new(new_id.0, new_id.1);
        let msg = ApiMsg::AddImage(key, width, height, stride, format, bytes);
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
    /// Non-blocking, it notifies a worker process which processes the stacking context.
    /// When it's done and a RenderNotifier has been set in `webrender::renderer::Renderer`,
    /// [new_frame_ready()][notifier] gets called.
    ///
    /// Note: Scrolling doesn't require an own Frame.
    ///
    /// Arguments:
    ///
    /// * `stacking_context_id`: The ID of the root stacking context.
    /// * `background_color`: The background color of this pipeline.
    /// * `epoch`: The unique Frame ID, monotonically increasing.
    /// * `pipeline_id`: The ID of the pipeline that is supplying this display list.
    /// * `viewport_size`: The size of the viewport for this frame.
    /// * `stacking_contexts`: Stacking contexts used in this frame.
    /// * `display_lists`: Display lists used in this frame.
    /// * `auxiliary_lists`: Various items that the display lists and stacking contexts reference.
    ///
    /// [notifier]: trait.RenderNotifier.html#tymethod.new_frame_ready
    pub fn set_root_stacking_context(&self,
                                     stacking_context_id: StackingContextId,
                                     background_color: ColorF,
                                     epoch: Epoch,
                                     pipeline_id: PipelineId,
                                     viewport_size: Size2D<f32>,
                                     stacking_contexts: Vec<(StackingContextId, StackingContext)>,
                                     display_lists: Vec<(DisplayListId, BuiltDisplayList)>,
                                     auxiliary_lists: AuxiliaryLists) {
        let display_list_descriptors = display_lists.iter().map(|&(display_list_id,
                                                                   ref built_display_list)| {
            (display_list_id, (*built_display_list.descriptor()).clone())
        }).collect();
        let msg = ApiMsg::SetRootStackingContext(stacking_context_id,
                                                 background_color,
                                                 epoch,
                                                 pipeline_id,
                                                 viewport_size,
                                                 stacking_contexts,
                                                 display_list_descriptors,
                                                 *auxiliary_lists.descriptor());
        self.api_sender.send(msg).unwrap();

        let mut payload = vec![];
        payload.write_u32::<LittleEndian>(stacking_context_id.0).unwrap();
        payload.write_u32::<LittleEndian>(epoch.0).unwrap();

        for &(_, ref built_display_list) in &display_lists {
            payload.extend_from_slice(built_display_list.data());
        }
        payload.extend_from_slice(auxiliary_lists.data());

        self.payload_sender.send(&payload[..]).unwrap();
    }

    /// Scrolls the scrolling layer under the `cursor`
    ///
    /// Webrender looks for the layer closest to the user
    /// which has `ScrollPolicy::Scrollable` set.
    pub fn scroll(&self, delta: Point2D<f32>, cursor: Point2D<f32>, phase: ScrollEventPhase) {
        let msg = ApiMsg::Scroll(delta, cursor, phase);
        self.api_sender.send(msg).unwrap();
    }

    pub fn tick_scrolling_bounce_animations(&self) {
        let msg = ApiMsg::TickScrollingBounce;
        self.api_sender.send(msg).unwrap();
    }

    pub fn set_scroll_offset(&self, scroll_id: ScrollLayerId, offset: Point2D<f32>) {
        let msg = ApiMsg::SetScrollOffset(scroll_id, offset);
        self.api_sender.send(msg).unwrap();
    }

    pub fn generate_frame(&self) {
        let msg = ApiMsg::GenerateFrame;
        self.api_sender.send(msg).unwrap();
    }

    /// Translates a point from viewport coordinates to layer space
    pub fn translate_point_to_layer_space(&self, point: &Point2D<f32>)
                                          -> (Point2D<f32>, PipelineId) {
        let (tx, rx) = ipc::channel().unwrap();
        let msg = ApiMsg::TranslatePointToLayerSpace(*point, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn get_scroll_layer_state(&self) -> Vec<ScrollLayerState> {
        let (tx, rx) = ipc::channel().unwrap();
        let msg = ApiMsg::GetScrollLayerState(tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn request_webgl_context(&self, size: &Size2D<i32>, attributes: GLContextAttributes)
                                 -> Result<(WebGLContextId, GLLimits), String> {
        let (tx, rx) = ipc::channel().unwrap();
        let msg = ApiMsg::RequestWebGLContext(*size, attributes, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    pub fn resize_webgl_context(&self, context_id: WebGLContextId, size: &Size2D<i32>) {
        let msg = ApiMsg::ResizeWebGLContext(context_id, *size);
        self.api_sender.send(msg).unwrap();
    }

    pub fn send_webgl_command(&self, context_id: WebGLContextId, command: WebGLCommand) {
        let msg = ApiMsg::WebGLCommand(context_id, command);
        self.api_sender.send(msg).unwrap();
    }

    #[inline]
    pub fn next_stacking_context_id(&self) -> StackingContextId {
        let new_id = self.next_unique_id();
        StackingContextId(new_id.0, new_id.1)
    }

    #[inline]
    pub fn next_display_list_id(&self) -> DisplayListId {
        let new_id = self.next_unique_id();
        DisplayListId(new_id.0, new_id.1)
    }

    #[inline]
    fn next_unique_id(&self) -> (u32, u32) {
        let IdNamespace(namespace) = self.id_namespace;
        let ResourceId(id) = self.next_id.get();
        self.next_id.set(ResourceId(id + 1));
        (namespace, id)
    }
}


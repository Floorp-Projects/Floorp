/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use channel::{self, MsgSender, Payload, PayloadSenderHelperMethods, PayloadSender};
#[cfg(feature = "webgl")]
use offscreen_gl_context::{GLContextAttributes, GLLimits};
use std::cell::Cell;
use std::fmt;
use std::marker::PhantomData;
use {AuxiliaryLists, AuxiliaryListsDescriptor, BuiltDisplayList, BuiltDisplayListDescriptor};
use {ClipId, ColorF, DeviceIntPoint, DeviceIntSize, DeviceUintRect, DeviceUintSize, FontKey};
use {GlyphDimensions, GlyphKey, ImageData, ImageDescriptor, ImageKey, LayoutPoint, LayoutSize};
use {LayoutTransform, NativeFontHandle, WorldPoint};
#[cfg(feature = "webgl")]
use {WebGLCommand, WebGLContextId};

pub type TileSize = u16;

#[derive(Clone, Deserialize, Serialize)]
pub enum ApiMsg {
    AddRawFont(FontKey, Vec<u8>, u32),
    AddNativeFont(FontKey, NativeFontHandle),
    DeleteFont(FontKey),
    /// Gets the glyph dimensions
    GetGlyphDimensions(Vec<GlyphKey>, MsgSender<Vec<Option<GlyphDimensions>>>),
    /// Adds an image from the resource cache.
    AddImage(ImageKey, ImageDescriptor, ImageData, Option<TileSize>),
    /// Updates the the resource cache with the new image data.
    UpdateImage(ImageKey, ImageDescriptor, ImageData, Option<DeviceUintRect>),
    /// Drops an image from the resource cache.
    DeleteImage(ImageKey),
    CloneApi(MsgSender<IdNamespace>),
    /// Supplies a new frame to WebRender.
    ///
    /// After receiving this message, WebRender will read the display list, followed by the
    /// auxiliary lists, from the payload channel.
    SetDisplayList(Option<ColorF>,
                   Epoch,
                   PipelineId,
                   LayoutSize,
                   BuiltDisplayListDescriptor,
                   AuxiliaryListsDescriptor,
                   bool),
    SetPageZoom(ZoomFactor),
    SetPinchZoom(ZoomFactor),
    SetPan(DeviceIntPoint),
    SetRootPipeline(PipelineId),
    SetWindowParameters(DeviceUintSize, DeviceUintRect),
    Scroll(ScrollLocation, WorldPoint, ScrollEventPhase),
    ScrollNodeWithId(LayoutPoint, ClipId),
    TickScrollingBounce,
    TranslatePointToLayerSpace(WorldPoint, MsgSender<(LayoutPoint, PipelineId)>),
    GetScrollNodeState(MsgSender<Vec<ScrollLayerState>>),
    RequestWebGLContext(DeviceIntSize, GLContextAttributes, MsgSender<Result<(WebGLContextId, GLLimits), String>>),
    ResizeWebGLContext(WebGLContextId, DeviceIntSize),
    WebGLCommand(WebGLContextId, WebGLCommand),
    GenerateFrame(Option<DynamicProperties>),
    // WebVR commands that must be called in the WebGL render thread.
    VRCompositorCommand(WebGLContextId, VRCompositorCommand),
    /// An opaque handle that must be passed to the render notifier. It is used by Gecko
    /// to forward gecko-specific messages to the render thread preserving the ordering
    /// within the other messages.
    ExternalEvent(ExternalEvent),
    ShutDown,
}

impl fmt::Debug for ApiMsg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match *self {
            ApiMsg::AddRawFont(..) => "ApiMsg::AddRawFont",
            ApiMsg::AddNativeFont(..) => "ApiMsg::AddNativeFont",
            ApiMsg::DeleteFont(..) => "ApiMsg::DeleteFont",
            ApiMsg::GetGlyphDimensions(..) => "ApiMsg::GetGlyphDimensions",
            ApiMsg::AddImage(..) => "ApiMsg::AddImage",
            ApiMsg::UpdateImage(..) => "ApiMsg::UpdateImage",
            ApiMsg::DeleteImage(..) => "ApiMsg::DeleteImage",
            ApiMsg::CloneApi(..) => "ApiMsg::CloneApi",
            ApiMsg::SetDisplayList(..) => "ApiMsg::SetDisplayList",
            ApiMsg::SetRootPipeline(..) => "ApiMsg::SetRootPipeline",
            ApiMsg::Scroll(..) => "ApiMsg::Scroll",
            ApiMsg::ScrollNodeWithId(..) => "ApiMsg::ScrollNodeWithId",
            ApiMsg::TickScrollingBounce => "ApiMsg::TickScrollingBounce",
            ApiMsg::TranslatePointToLayerSpace(..) => "ApiMsg::TranslatePointToLayerSpace",
            ApiMsg::GetScrollNodeState(..) => "ApiMsg::GetScrollNodeState",
            ApiMsg::RequestWebGLContext(..) => "ApiMsg::RequestWebGLContext",
            ApiMsg::ResizeWebGLContext(..) => "ApiMsg::ResizeWebGLContext",
            ApiMsg::WebGLCommand(..) => "ApiMsg::WebGLCommand",
            ApiMsg::GenerateFrame(..) => "ApiMsg::GenerateFrame",
            ApiMsg::VRCompositorCommand(..) => "ApiMsg::VRCompositorCommand",
            ApiMsg::ExternalEvent(..) => "ApiMsg::ExternalEvent",
            ApiMsg::ShutDown => "ApiMsg::ShutDown",
            ApiMsg::SetPageZoom(..) => "ApiMsg::SetPageZoom",
            ApiMsg::SetPinchZoom(..) => "ApiMsg::SetPinchZoom",
            ApiMsg::SetPan(..) => "ApiMsg::SetPan",
            ApiMsg::SetWindowParameters(..) => "ApiMsg::SetWindowParameters",
        })
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Epoch(pub u32);

#[cfg(not(feature = "webgl"))]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct WebGLContextId(pub usize);

#[cfg(not(feature = "webgl"))]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GLContextAttributes([u8; 0]);

#[cfg(not(feature = "webgl"))]
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GLLimits([u8; 0]);

#[cfg(not(feature = "webgl"))]
#[derive(Clone, Deserialize, Serialize)]
pub enum WebGLCommand {
    Flush,
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, PartialEq, Serialize)]
pub struct PipelineId(pub u32, pub u32);

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct IdNamespace(pub u32);

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct ResourceId(pub u32);

/// An opaque pointer-sized value.
#[repr(C)]
#[derive(Clone, Deserialize, Serialize)]
pub struct ExternalEvent {
    raw: usize,
}

unsafe impl Send for ExternalEvent {}

impl ExternalEvent {
    pub fn from_raw(raw: usize) -> Self { ExternalEvent { raw: raw } }
    /// Consumes self to make it obvious that the event should be forwarded only once.
    pub fn unwrap(self) -> usize { self.raw }
}


#[derive(Clone, Deserialize, Serialize)]
pub struct RenderApiSender {
    api_sender: MsgSender<ApiMsg>,
    payload_sender: PayloadSender,
}

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

    pub fn generate_font_key(&self) -> FontKey {
        let new_id = self.next_unique_id();
        FontKey::new(new_id.0, new_id.1)
    }

    pub fn add_raw_font(&self, key: FontKey, bytes: Vec<u8>, index: u32) {
        let msg = ApiMsg::AddRawFont(key, bytes, index);
        self.api_sender.send(msg).unwrap();
    }

    pub fn add_native_font(&self, key: FontKey, native_font_handle: NativeFontHandle) {
        let msg = ApiMsg::AddNativeFont(key, native_font_handle);
        self.api_sender.send(msg).unwrap();
    }

    pub fn delete_font(&self, key: FontKey) {
        let msg = ApiMsg::DeleteFont(key);
        self.api_sender.send(msg).unwrap();
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
    pub fn generate_image_key(&self) -> ImageKey {
        let new_id = self.next_unique_id();
        ImageKey::new(new_id.0, new_id.1)
    }

    /// Adds an image identified by the `ImageKey`.
    pub fn add_image(&self,
                     key: ImageKey,
                     descriptor: ImageDescriptor,
                     data: ImageData,
                     tiling: Option<TileSize>) {
        let msg = ApiMsg::AddImage(key, descriptor, data, tiling);
        self.api_sender.send(msg).unwrap();
    }

    /// Updates a specific image.
    ///
    /// Currently doesn't support changing dimensions or format by updating.
    // TODO: Support changing dimensions (and format) during image update?
    pub fn update_image(&self,
                        key: ImageKey,
                        descriptor: ImageDescriptor,
                        data: ImageData,
                        dirty_rect: Option<DeviceUintRect>) {
        let msg = ApiMsg::UpdateImage(key, descriptor, data, dirty_rect);
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
    /// ```
    /// # use webrender_traits::{PipelineId, RenderApiSender};
    /// # fn example(sender: RenderApiSender) {
    /// let api = sender.create_api();
    /// // ...
    /// let pipeline_id = PipelineId(0, 0);
    /// api.set_root_pipeline(pipeline_id);
    /// # }
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
    /// * `viewport_size`: The size of the viewport for this frame.
    /// * `pipeline_id`: The ID of the pipeline that is supplying this display list.
    /// * `display_list`: The root Display list used in this frame.
    /// * `auxiliary_lists`: Various items that the display lists and stacking contexts reference.
    /// * `preserve_frame_state`: If a previous frame exists which matches this pipeline
    ///                           id, this setting determines if frame state (such as scrolling
    ///                           position) should be preserved for this new display list.
    ///
    /// [notifier]: trait.RenderNotifier.html#tymethod.new_frame_ready
    pub fn set_display_list(&self,
                            background_color: Option<ColorF>,
                            epoch: Epoch,
                            viewport_size: LayoutSize,
                            (pipeline_id, display_list, auxiliary_lists): (PipelineId, BuiltDisplayList, AuxiliaryLists),
                            preserve_frame_state: bool) {
        let (dl_data, dl_desc) = display_list.into_data();
        let (aux_data, aux_desc) = auxiliary_lists.into_data();
        let msg = ApiMsg::SetDisplayList(background_color,
                                             epoch,
                                             pipeline_id,
                                             viewport_size,
                                             dl_desc,
                                             aux_desc,
                                             preserve_frame_state);
        self.api_sender.send(msg).unwrap();

        self.payload_sender.send_payload(Payload {
            epoch: epoch,
            pipeline_id: pipeline_id,
            display_list_data: dl_data,
            auxiliary_lists_data: aux_data
        }).unwrap();
    }

    /// Scrolls the scrolling layer under the `cursor`
    ///
    /// WebRender looks for the layer closest to the user
    /// which has `ScrollPolicy::Scrollable` set.
    pub fn scroll(&self, scroll_location: ScrollLocation, cursor: WorldPoint, phase: ScrollEventPhase) {
        let msg = ApiMsg::Scroll(scroll_location, cursor, phase);
        self.api_sender.send(msg).unwrap();
    }

    pub fn scroll_node_with_id(&self, new_scroll_origin: LayoutPoint, id: ClipId) {
        let msg = ApiMsg::ScrollNodeWithId(new_scroll_origin, id);
        self.api_sender.send(msg).unwrap();
    }

    pub fn set_page_zoom(&self, page_zoom: ZoomFactor) {
        let msg = ApiMsg::SetPageZoom(page_zoom);
        self.api_sender.send(msg).unwrap();
    }

    pub fn set_pinch_zoom(&self, pinch_zoom: ZoomFactor) {
        let msg = ApiMsg::SetPinchZoom(pinch_zoom);
        self.api_sender.send(msg).unwrap();
    }

    pub fn set_pan(&self, pan: DeviceIntPoint) {
        let msg = ApiMsg::SetPan(pan);
        self.api_sender.send(msg).unwrap();
    }

    pub fn set_window_parameters(&self,
                                 window_size: DeviceUintSize,
                                 inner_rect: DeviceUintRect) {
        let msg = ApiMsg::SetWindowParameters(window_size, inner_rect);
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

    pub fn get_scroll_node_state(&self) -> Vec<ScrollLayerState> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::GetScrollNodeState(tx);
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

    /// Generate a new frame. Optionally, supply a list of animated
    /// property bindings that should be used to resolve bindings
    /// in the current display list.
    pub fn generate_frame(&self, property_bindings: Option<DynamicProperties>) {
        let msg = ApiMsg::GenerateFrame(property_bindings);
        self.api_sender.send(msg).unwrap();
    }

    pub fn send_vr_compositor_command(&self, context_id: WebGLContextId, command: VRCompositorCommand) {
        let msg = ApiMsg::VRCompositorCommand(context_id, command);
        self.api_sender.send(msg).unwrap();
    }

    pub fn send_external_event(&self, evt: ExternalEvent) {
        let msg = ApiMsg::ExternalEvent(evt);
        self.api_sender.send(msg).unwrap();
    }

    pub fn shut_down(&self) {
        self.api_sender.send(ApiMsg::ShutDown).unwrap();
    }

    /// Create a new unique key that can be used for
    /// animated property bindings.
    pub fn generate_property_binding_key<T: Copy>(&self) -> PropertyBindingKey<T> {
        let new_id = self.next_unique_id();
        PropertyBindingKey {
            id: PropertyBindingId {
                namespace: new_id.0,
                uid: new_id.1,
            },
            _phantom: PhantomData,
        }
    }

    #[inline]
    fn next_unique_id(&self) -> (u32, u32) {
        let IdNamespace(namespace) = self.id_namespace;
        let ResourceId(id) = self.next_id.get();
        self.next_id.set(ResourceId(id + 1));
        (namespace, id)
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum ScrollEventPhase {
    /// The user started scrolling.
    Start,
    /// The user performed a scroll. The Boolean flag indicates whether the user's fingers are
    /// down, if a touchpad is in use. (If false, the event is a touchpad fling.)
    Move(bool),
    /// The user ended scrolling.
    End,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct ScrollLayerState {
    pub id: ClipId,
    pub scroll_offset: LayoutPoint,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub enum ScrollLocation {
    /// Scroll by a certain amount.
    Delta(LayoutPoint),
    /// Scroll to very top of element.
    Start,
    /// Scroll to very bottom of element.
    End
}

/// Represents a zoom factor.
#[derive(Clone, Copy, Serialize, Deserialize, Debug)]
pub struct ZoomFactor(f32);

impl ZoomFactor {
    /// Construct a new zoom factor.
    pub fn new(scale: f32) -> ZoomFactor {
        ZoomFactor(scale)
    }

    /// Get the zoom factor as an untyped float.
    pub fn get(&self) -> f32 {
        self.0
    }
}

#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize, Eq, Hash)]
pub struct PropertyBindingId {
    namespace: u32,
    uid: u32,
}

impl PropertyBindingId {
    pub fn new(value: u64) -> Self {
        PropertyBindingId {
            namespace: (value>>32) as u32,
            uid: value as u32,
        }
    }
}

/// A unique key that is used for connecting animated property
/// values to bindings in the display list.
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub struct PropertyBindingKey<T> {
    pub id: PropertyBindingId,
    _phantom: PhantomData<T>,
}

/// Construct a property value from a given key and value.
impl<T: Copy> PropertyBindingKey<T> {
    pub fn with(&self, value: T) -> PropertyValue<T> {
        PropertyValue {
            key: *self,
            value: value,
        }
    }
}

impl<T> PropertyBindingKey<T> {
    pub fn new(value: u64) -> Self {
        PropertyBindingKey {
            id: PropertyBindingId::new(value),
            _phantom: PhantomData,
        }
    }
}

/// A binding property can either be a specific value
/// (the normal, non-animated case) or point to a binding location
/// to fetch the current value from.
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum PropertyBinding<T> {
    Value(T),
    Binding(PropertyBindingKey<T>),
}

impl<T> From<T> for PropertyBinding<T> {
    fn from(value: T) -> PropertyBinding<T> {
        PropertyBinding::Value(value)
    }
}

impl<T> From<PropertyBindingKey<T>> for PropertyBinding<T> {
    fn from(key: PropertyBindingKey<T>) -> PropertyBinding<T> {
        PropertyBinding::Binding(key)
    }
}

/// The current value of an animated property. This is
/// supplied by the calling code.
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub struct PropertyValue<T> {
    pub key: PropertyBindingKey<T>,
    pub value: T,
}

/// When using `generate_frame()`, a list of `PropertyValue` structures
/// can optionally be supplied to provide the current value of any
/// animated properties.
#[derive(Clone, Deserialize, Serialize, Debug)]
pub struct DynamicProperties {
    pub transforms: Vec<PropertyValue<LayoutTransform>>,
    pub floats: Vec<PropertyValue<f32>>,
}

pub type VRCompositorId = u64;

// WebVR commands that must be called in the WebGL render thread.
#[derive(Clone, Deserialize, Serialize)]
pub enum VRCompositorCommand {
    Create(VRCompositorId),
    SyncPoses(VRCompositorId, f64, f64, MsgSender<Result<Vec<u8>,()>>),
    SubmitFrame(VRCompositorId, [f32; 4], [f32; 4]),
    Release(VRCompositorId)
}

// Trait object that handles WebVR commands.
// Receives the texture id and size associated to the WebGLContext.
pub trait VRCompositorHandler: Send {
    fn handle(&mut self, command: VRCompositorCommand, texture: Option<(u32, DeviceIntSize)>);
}

pub trait RenderNotifier: Send {
    fn new_frame_ready(&mut self);
    fn new_scroll_frame_ready(&mut self, composite_needed: bool);
    fn external_event(&mut self, _evt: ExternalEvent) { unimplemented!() }
    fn shut_down(&mut self) {}
}

/// Trait to allow dispatching functions to a specific thread or event loop.
pub trait RenderDispatcher: Send {
    fn dispatch(&self, Box<Fn() + Send>);
}

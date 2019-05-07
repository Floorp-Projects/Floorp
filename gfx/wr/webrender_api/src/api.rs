/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

extern crate serde_bytes;

use crate::channel::{self, MsgSender, Payload, PayloadSender, PayloadSenderHelperMethods};
use std::cell::Cell;
use std::fmt;
use std::marker::PhantomData;
use std::os::raw::c_void;
use std::path::PathBuf;
use std::sync::Arc;
use std::u32;
// local imports
use crate::{display_item as di, font};
use crate::color::{ColorU, ColorF};
use crate::display_list::{BuiltDisplayList, BuiltDisplayListDescriptor};
use crate::image::{BlobImageData, BlobImageKey, ImageData, ImageDescriptor, ImageKey};
use crate::units::*;


pub type TileSize = u16;
/// Documents are rendered in the ascending order of their associated layer values.
pub type DocumentLayer = i8;

#[derive(Clone, Deserialize, Serialize)]
pub enum ResourceUpdate {
    AddImage(AddImage),
    UpdateImage(UpdateImage),
    AddBlobImage(AddBlobImage),
    UpdateBlobImage(UpdateBlobImage),
    DeleteImage(ImageKey),
    SetBlobImageVisibleArea(BlobImageKey, DeviceIntRect),
    AddFont(AddFont),
    DeleteFont(font::FontKey),
    AddFontInstance(AddFontInstance),
    DeleteFontInstance(font::FontInstanceKey),
}

/// A Transaction is a group of commands to apply atomically to a document.
///
/// This mechanism ensures that:
///  - no other message can be interleaved between two commands that need to be applied together.
///  - no redundant work is performed if two commands in the same transaction cause the scene or
///    the frame to be rebuilt.
pub struct Transaction {
    // Operations affecting the scene (applied before scene building).
    scene_ops: Vec<SceneMsg>,
    // Operations affecting the generation of frames (applied after scene building).
    frame_ops: Vec<FrameMsg>,

    // Additional display list data.
    payloads: Vec<Payload>,

    notifications: Vec<NotificationRequest>,

    // Resource updates are applied after scene building.
    pub resource_updates: Vec<ResourceUpdate>,

    // If true the transaction is piped through the scene building thread, if false
    // it will be applied directly on the render backend.
    use_scene_builder_thread: bool,

    generate_frame: bool,

    pub invalidate_rendered_frame: bool,

    low_priority: bool,
}

impl Transaction {
    pub fn new() -> Self {
        Transaction {
            scene_ops: Vec::new(),
            frame_ops: Vec::new(),
            resource_updates: Vec::new(),
            payloads: Vec::new(),
            notifications: Vec::new(),
            use_scene_builder_thread: true,
            generate_frame: false,
            invalidate_rendered_frame: false,
            low_priority: false,
        }
    }

    // TODO: better name?
    pub fn skip_scene_builder(&mut self) {
        self.use_scene_builder_thread = false;
    }

    // TODO: this is temporary, using the scene builder thread is the default for
    // most transactions, and opt-in for specific cases like scrolling and async video.
    pub fn use_scene_builder_thread(&mut self) {
        self.use_scene_builder_thread = true;
    }

    pub fn is_empty(&self) -> bool {
        !self.generate_frame &&
            !self.invalidate_rendered_frame &&
            self.scene_ops.is_empty() &&
            self.frame_ops.is_empty() &&
            self.resource_updates.is_empty() &&
            self.notifications.is_empty()
    }

    pub fn update_epoch(&mut self, pipeline_id: PipelineId, epoch: Epoch) {
        // We track epochs before and after scene building.
        // This one will be applied to the pending scene right away:
        self.scene_ops.push(SceneMsg::UpdateEpoch(pipeline_id, epoch));
        // And this one will be applied to the currently built scene at the end
        // of the transaction (potentially long after the scene_ops one).
        self.frame_ops.push(FrameMsg::UpdateEpoch(pipeline_id, epoch));
        // We could avoid the duplication here by storing the epoch updates in a
        // separate array and let the render backend schedule the updates at the
        // proper times, but it wouldn't make things simpler.
    }

    /// Sets the root pipeline.
    ///
    /// # Examples
    ///
    /// ```
    /// # use webrender_api::{PipelineId, RenderApiSender, Transaction};
    /// # use webrender_api::units::{DeviceIntSize};
    /// # fn example() {
    /// let pipeline_id = PipelineId(0, 0);
    /// let mut txn = Transaction::new();
    /// txn.set_root_pipeline(pipeline_id);
    /// # }
    /// ```
    pub fn set_root_pipeline(&mut self, pipeline_id: PipelineId) {
        self.scene_ops.push(SceneMsg::SetRootPipeline(pipeline_id));
    }

    /// Removes data associated with a pipeline from the internal data structures.
    /// If the specified `pipeline_id` is for the root pipeline, the root pipeline
    /// is reset back to `None`.
    pub fn remove_pipeline(&mut self, pipeline_id: PipelineId) {
        self.scene_ops.push(SceneMsg::RemovePipeline(pipeline_id));
    }

    /// Supplies a new frame to WebRender.
    ///
    /// Non-blocking, it notifies a worker process which processes the display list.
    ///
    /// Note: Scrolling doesn't require an own Frame.
    ///
    /// Arguments:
    ///
    /// * `epoch`: The unique Frame ID, monotonically increasing.
    /// * `background`: The background color of this pipeline.
    /// * `viewport_size`: The size of the viewport for this frame.
    /// * `pipeline_id`: The ID of the pipeline that is supplying this display list.
    /// * `content_size`: The total screen space size of this display list's display items.
    /// * `display_list`: The root Display list used in this frame.
    /// * `preserve_frame_state`: If a previous frame exists which matches this pipeline
    ///                           id, this setting determines if frame state (such as scrolling
    ///                           position) should be preserved for this new display list.
    pub fn set_display_list(
        &mut self,
        epoch: Epoch,
        background: Option<ColorF>,
        viewport_size: LayoutSize,
        (pipeline_id, content_size, display_list): (PipelineId, LayoutSize, BuiltDisplayList),
        preserve_frame_state: bool,
    ) {
        let (display_list_data, list_descriptor) = display_list.into_data();
        self.scene_ops.push(
            SceneMsg::SetDisplayList {
                epoch,
                pipeline_id,
                background,
                viewport_size,
                content_size,
                list_descriptor,
                preserve_frame_state,
            }
        );
        self.payloads.push(Payload { epoch, pipeline_id, display_list_data });
    }

    pub fn update_resources(&mut self, resources: Vec<ResourceUpdate>) {
        self.merge(resources);
    }

    // Note: Gecko uses this to get notified when a transaction that contains
    // potentially long blob rasterization or scene build is ready to be rendered.
    // so that the tab-switching integration can react adequately when tab
    // switching takes too long. For this use case when matters is that the
    // notification doesn't fire before scene building and blob rasterization.

    /// Trigger a notification at a certain stage of the rendering pipeline.
    ///
    /// Not that notification requests are skipped during serialization, so is is
    /// best to use them for synchronization purposes and not for things that could
    /// affect the WebRender's state.
    pub fn notify(&mut self, event: NotificationRequest) {
        self.notifications.push(event);
    }

    /// Setup the output region in the framebuffer for a given document.
    pub fn set_document_view(
        &mut self,
        device_rect: DeviceIntRect,
        device_pixel_ratio: f32,
    ) {
        self.scene_ops.push(
            SceneMsg::SetDocumentView {
                device_rect,
                device_pixel_ratio,
            },
        );
    }

    /// Enable copying of the output of this pipeline id to
    /// an external texture for callers to consume.
    pub fn enable_frame_output(&mut self, pipeline_id: PipelineId, enable: bool) {
        self.scene_ops.push(SceneMsg::EnableFrameOutput(pipeline_id, enable));
    }

    /// Scrolls the scrolling layer under the `cursor`
    ///
    /// WebRender looks for the layer closest to the user
    /// which has `ScrollPolicy::Scrollable` set.
    pub fn scroll(&mut self, scroll_location: ScrollLocation, cursor: WorldPoint) {
        self.frame_ops.push(FrameMsg::Scroll(scroll_location, cursor));
    }

    pub fn scroll_node_with_id(
        &mut self,
        origin: LayoutPoint,
        id: di::ExternalScrollId,
        clamp: ScrollClamping,
    ) {
        self.frame_ops.push(FrameMsg::ScrollNodeWithId(origin, id, clamp));
    }

    pub fn set_page_zoom(&mut self, page_zoom: ZoomFactor) {
        self.scene_ops.push(SceneMsg::SetPageZoom(page_zoom));
    }

    pub fn set_pinch_zoom(&mut self, pinch_zoom: ZoomFactor) {
        self.frame_ops.push(FrameMsg::SetPinchZoom(pinch_zoom));
    }

    pub fn set_pan(&mut self, pan: DeviceIntPoint) {
        self.frame_ops.push(FrameMsg::SetPan(pan));
    }

    /// Generate a new frame. When it's done and a RenderNotifier has been set
    /// in `webrender::Renderer`, [new_frame_ready()][notifier] gets called.
    /// Note that the notifier is called even if the frame generation was a
    /// no-op; the arguments passed to `new_frame_ready` will provide information
    /// as to when happened.
    ///
    /// [notifier]: trait.RenderNotifier.html#tymethod.new_frame_ready
    pub fn generate_frame(&mut self) {
        self.generate_frame = true;
    }

    /// Invalidate rendered frame. It ensure that frame will be rendered during
    /// next frame generation. WebRender could skip frame rendering if there
    /// is no update.
    /// But there are cases that needs to force rendering.
    ///  - Content of image is updated by reusing same ExternalImageId.
    ///  - Platform requests it if pixels become stale (like wakeup from standby).
    pub fn invalidate_rendered_frame(&mut self) {
        self.invalidate_rendered_frame = true;
    }

    /// Supply a list of animated property bindings that should be used to resolve
    /// bindings in the current display list.
    pub fn update_dynamic_properties(&mut self, properties: DynamicProperties) {
        self.frame_ops.push(FrameMsg::UpdateDynamicProperties(properties));
    }

    /// Add to the list of animated property bindings that should be used to
    /// resolve bindings in the current display list. This is a convenience method
    /// so the caller doesn't have to figure out all the dynamic properties before
    /// setting them on the transaction but can do them incrementally.
    pub fn append_dynamic_properties(&mut self, properties: DynamicProperties) {
        self.frame_ops.push(FrameMsg::AppendDynamicProperties(properties));
    }

    /// Consumes this object and just returns the frame ops.
    pub fn get_frame_ops(self) -> Vec<FrameMsg> {
        self.frame_ops
    }

    fn finalize(self) -> (TransactionMsg, Vec<Payload>) {
        (
            TransactionMsg {
                scene_ops: self.scene_ops,
                frame_ops: self.frame_ops,
                resource_updates: self.resource_updates,
                notifications: self.notifications,
                use_scene_builder_thread: self.use_scene_builder_thread,
                generate_frame: self.generate_frame,
                invalidate_rendered_frame: self.invalidate_rendered_frame,
                low_priority: self.low_priority,
            },
            self.payloads,
        )
    }

    pub fn add_image(
        &mut self,
        key: ImageKey,
        descriptor: ImageDescriptor,
        data: ImageData,
        tiling: Option<TileSize>,
    ) {
        self.resource_updates.push(ResourceUpdate::AddImage(AddImage {
            key,
            descriptor,
            data,
            tiling,
        }));
    }

    pub fn update_image(
        &mut self,
        key: ImageKey,
        descriptor: ImageDescriptor,
        data: ImageData,
        dirty_rect: &ImageDirtyRect,
    ) {
        self.resource_updates.push(ResourceUpdate::UpdateImage(UpdateImage {
            key,
            descriptor,
            data,
            dirty_rect: *dirty_rect,
        }));
    }

    pub fn delete_image(&mut self, key: ImageKey) {
        self.resource_updates.push(ResourceUpdate::DeleteImage(key));
    }

    pub fn add_blob_image(
        &mut self,
        key: BlobImageKey,
        descriptor: ImageDescriptor,
        data: Arc<BlobImageData>,
        tiling: Option<TileSize>,
    ) {
        self.resource_updates.push(
            ResourceUpdate::AddBlobImage(AddBlobImage {
                key,
                descriptor,
                data,
                tiling,
            })
        );
    }

    pub fn update_blob_image(
        &mut self,
        key: BlobImageKey,
        descriptor: ImageDescriptor,
        data: Arc<BlobImageData>,
        dirty_rect: &BlobDirtyRect,
    ) {
        self.resource_updates.push(
            ResourceUpdate::UpdateBlobImage(UpdateBlobImage {
                key,
                descriptor,
                data,
                dirty_rect: *dirty_rect,
            })
        );
    }

    pub fn delete_blob_image(&mut self, key: BlobImageKey) {
        self.resource_updates.push(ResourceUpdate::DeleteImage(key.as_image()));
    }

    pub fn set_blob_image_visible_area(&mut self, key: BlobImageKey, area: DeviceIntRect) {
        self.resource_updates.push(ResourceUpdate::SetBlobImageVisibleArea(key, area))
    }

    pub fn add_raw_font(&mut self, key: font::FontKey, bytes: Vec<u8>, index: u32) {
        self.resource_updates
            .push(ResourceUpdate::AddFont(AddFont::Raw(key, bytes, index)));
    }

    pub fn add_native_font(&mut self, key: font::FontKey, native_handle: font::NativeFontHandle) {
        self.resource_updates
            .push(ResourceUpdate::AddFont(AddFont::Native(key, native_handle)));
    }

    pub fn delete_font(&mut self, key: font::FontKey) {
        self.resource_updates.push(ResourceUpdate::DeleteFont(key));
    }

    pub fn add_font_instance(
        &mut self,
        key: font::FontInstanceKey,
        font_key: font::FontKey,
        glyph_size: Au,
        options: Option<font::FontInstanceOptions>,
        platform_options: Option<font::FontInstancePlatformOptions>,
        variations: Vec<font::FontVariation>,
    ) {
        self.resource_updates
            .push(ResourceUpdate::AddFontInstance(AddFontInstance {
                key,
                font_key,
                glyph_size,
                options,
                platform_options,
                variations,
            }));
    }

    pub fn delete_font_instance(&mut self, key: font::FontInstanceKey) {
        self.resource_updates.push(ResourceUpdate::DeleteFontInstance(key));
    }

    // A hint that this transaction can be processed at a lower priority. High-
    // priority transactions can jump ahead of regular-priority transactions,
    // but both high- and regular-priority transactions are processed in order
    // relative to other transactions of the same priority.
    pub fn set_low_priority(&mut self, low_priority: bool) {
        self.low_priority = low_priority;
    }

    pub fn is_low_priority(&self) -> bool {
        self.low_priority
    }

    pub fn merge(&mut self, mut other: Vec<ResourceUpdate>) {
        self.resource_updates.append(&mut other);
    }

    pub fn clear(&mut self) {
        self.resource_updates.clear()
    }
}

pub struct DocumentTransaction {
    pub document_id: DocumentId,
    pub transaction: Transaction,
}

/// Represents a transaction in the format sent through the channel.
#[derive(Clone, Deserialize, Serialize)]
pub struct TransactionMsg {
    pub scene_ops: Vec<SceneMsg>,
    pub frame_ops: Vec<FrameMsg>,
    pub resource_updates: Vec<ResourceUpdate>,
    pub generate_frame: bool,
    pub invalidate_rendered_frame: bool,
    pub use_scene_builder_thread: bool,
    pub low_priority: bool,

    #[serde(skip)]
    pub notifications: Vec<NotificationRequest>,
}

impl TransactionMsg {
    pub fn is_empty(&self) -> bool {
        !self.generate_frame &&
            !self.invalidate_rendered_frame &&
            self.scene_ops.is_empty() &&
            self.frame_ops.is_empty() &&
            self.resource_updates.is_empty() &&
            self.notifications.is_empty()
    }

    // TODO: We only need this for a few RenderApi methods which we should remove.
    pub fn frame_message(msg: FrameMsg) -> Self {
        TransactionMsg {
            scene_ops: Vec::new(),
            frame_ops: vec![msg],
            resource_updates: Vec::new(),
            notifications: Vec::new(),
            generate_frame: false,
            invalidate_rendered_frame: false,
            use_scene_builder_thread: false,
            low_priority: false,
        }
    }

    pub fn scene_message(msg: SceneMsg) -> Self {
        TransactionMsg {
            scene_ops: vec![msg],
            frame_ops: Vec::new(),
            resource_updates: Vec::new(),
            notifications: Vec::new(),
            generate_frame: false,
            invalidate_rendered_frame: false,
            use_scene_builder_thread: false,
            low_priority: false,
        }
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub struct AddImage {
    pub key: ImageKey,
    pub descriptor: ImageDescriptor,
    pub data: ImageData,
    pub tiling: Option<TileSize>,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct UpdateImage {
    pub key: ImageKey,
    pub descriptor: ImageDescriptor,
    pub data: ImageData,
    pub dirty_rect: ImageDirtyRect,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct AddBlobImage {
    pub key: BlobImageKey,
    pub descriptor: ImageDescriptor,
    //#[serde(with = "serde_image_data_raw")]
    pub data: Arc<BlobImageData>,
    pub tiling: Option<TileSize>,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct UpdateBlobImage {
    pub key: BlobImageKey,
    pub descriptor: ImageDescriptor,
    //#[serde(with = "serde_image_data_raw")]
    pub data: Arc<BlobImageData>,
    pub dirty_rect: BlobDirtyRect,
}

#[derive(Clone, Deserialize, Serialize)]
pub enum AddFont {
    Raw(
        font::FontKey,
        #[serde(with = "serde_bytes")] Vec<u8>,
        u32
    ),
    Native(font::FontKey, font::NativeFontHandle),
}

#[derive(Clone, Debug, Deserialize, PartialEq, Serialize)]
pub struct HitTestItem {
    /// The pipeline that the display item that was hit belongs to.
    pub pipeline: PipelineId,

    /// The tag of the hit display item.
    pub tag: di::ItemTag,

    /// The hit point in the coordinate space of the "viewport" of the display item. The
    /// viewport is the scroll node formed by the root reference frame of the display item's
    /// pipeline.
    pub point_in_viewport: LayoutPoint,

    /// The coordinates of the original hit test point relative to the origin of this item.
    /// This is useful for calculating things like text offsets in the client.
    pub point_relative_to_item: LayoutPoint,
}

#[derive(Clone, Debug, Default, Deserialize, Serialize)]
pub struct HitTestResult {
    pub items: Vec<HitTestItem>,
}

bitflags! {
    #[derive(Deserialize, MallocSizeOf, Serialize)]
    pub struct HitTestFlags: u8 {
        const FIND_ALL = 0b00000001;
        const POINT_RELATIVE_TO_PIPELINE_VIEWPORT = 0b00000010;
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub struct AddFontInstance {
    pub key: font::FontInstanceKey,
    pub font_key: font::FontKey,
    pub glyph_size: Au,
    pub options: Option<font::FontInstanceOptions>,
    pub platform_options: Option<font::FontInstancePlatformOptions>,
    pub variations: Vec<font::FontVariation>,
}

// Frame messages affect building the scene.
#[derive(Clone, Deserialize, Serialize)]
pub enum SceneMsg {
    UpdateEpoch(PipelineId, Epoch),
    SetPageZoom(ZoomFactor),
    SetRootPipeline(PipelineId),
    RemovePipeline(PipelineId),
    EnableFrameOutput(PipelineId, bool),
    SetDisplayList {
        list_descriptor: BuiltDisplayListDescriptor,
        epoch: Epoch,
        pipeline_id: PipelineId,
        background: Option<ColorF>,
        viewport_size: LayoutSize,
        content_size: LayoutSize,
        preserve_frame_state: bool,
    },
    SetDocumentView {
        device_rect: DeviceIntRect,
        device_pixel_ratio: f32,
    },
}

// Frame messages affect frame generation (applied after building the scene).
#[derive(Clone, Deserialize, Serialize)]
pub enum FrameMsg {
    UpdateEpoch(PipelineId, Epoch),
    HitTest(Option<PipelineId>, WorldPoint, HitTestFlags, MsgSender<HitTestResult>),
    SetPan(DeviceIntPoint),
    Scroll(ScrollLocation, WorldPoint),
    ScrollNodeWithId(LayoutPoint, di::ExternalScrollId, ScrollClamping),
    GetScrollNodeState(MsgSender<Vec<ScrollNodeState>>),
    UpdateDynamicProperties(DynamicProperties),
    AppendDynamicProperties(DynamicProperties),
    SetPinchZoom(ZoomFactor),
}

impl fmt::Debug for SceneMsg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match *self {
            SceneMsg::UpdateEpoch(..) => "SceneMsg::UpdateEpoch",
            SceneMsg::SetDisplayList { .. } => "SceneMsg::SetDisplayList",
            SceneMsg::SetPageZoom(..) => "SceneMsg::SetPageZoom",
            SceneMsg::RemovePipeline(..) => "SceneMsg::RemovePipeline",
            SceneMsg::EnableFrameOutput(..) => "SceneMsg::EnableFrameOutput",
            SceneMsg::SetDocumentView { .. } => "SceneMsg::SetDocumentView",
            SceneMsg::SetRootPipeline(..) => "SceneMsg::SetRootPipeline",
        })
    }
}

impl fmt::Debug for FrameMsg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match *self {
            FrameMsg::UpdateEpoch(..) => "FrameMsg::UpdateEpoch",
            FrameMsg::HitTest(..) => "FrameMsg::HitTest",
            FrameMsg::SetPan(..) => "FrameMsg::SetPan",
            FrameMsg::Scroll(..) => "FrameMsg::Scroll",
            FrameMsg::ScrollNodeWithId(..) => "FrameMsg::ScrollNodeWithId",
            FrameMsg::GetScrollNodeState(..) => "FrameMsg::GetScrollNodeState",
            FrameMsg::UpdateDynamicProperties(..) => "FrameMsg::UpdateDynamicProperties",
            FrameMsg::AppendDynamicProperties(..) => "FrameMsg::AppendDynamicProperties",
            FrameMsg::SetPinchZoom(..) => "FrameMsg::SetPinchZoom",
        })
    }
}

bitflags!{
    /// Bit flags for WR stages to store in a capture.
    // Note: capturing `FRAME` without `SCENE` is not currently supported.
    #[derive(Deserialize, Serialize)]
    pub struct CaptureBits: u8 {
        const SCENE = 0x1;
        const FRAME = 0x2;
    }
}

bitflags!{
    /// Mask for clearing caches in debug commands.
    #[derive(Deserialize, Serialize)]
    pub struct ClearCache: u8 {
        const IMAGES = 0b1;
        const GLYPHS = 0b01;
        const GLYPH_DIMENSIONS = 0b001;
        const RENDER_TASKS = 0b0001;
        const TEXTURE_CACHE = 0b00001;
        const RASTERIZED_BLOBS = 0b000001;
    }
}

/// Information about a loaded capture of each document
/// that is returned by `RenderBackend`.
#[derive(Clone, Debug, Deserialize, Serialize)]
pub struct CapturedDocument {
    pub document_id: DocumentId,
    pub root_pipeline_id: Option<PipelineId>,
}

#[derive(Clone, Deserialize, Serialize)]
pub enum DebugCommand {
    /// Sets the provided debug flags.
    SetFlags(DebugFlags),
    /// Configure if dual-source blending is used, if available.
    EnableDualSourceBlending(bool),
    /// Fetch current documents and display lists.
    FetchDocuments,
    /// Fetch current passes and batches.
    FetchPasses,
    /// Fetch clip-scroll tree.
    FetchClipScrollTree,
    /// Fetch render tasks.
    FetchRenderTasks,
    /// Fetch screenshot.
    FetchScreenshot,
    /// Save a capture of all the documents state.
    SaveCapture(PathBuf, CaptureBits),
    /// Load a capture of all the documents state.
    LoadCapture(PathBuf, MsgSender<CapturedDocument>),
    /// Clear cached resources, forcing them to be re-uploaded from templates.
    ClearCaches(ClearCache),
    /// Invalidate GPU cache, forcing the update from the CPU mirror.
    InvalidateGpuCache,
    /// Causes the scene builder to pause for a given amount of miliseconds each time it
    /// processes a transaction.
    SimulateLongSceneBuild(u32),
    /// Causes the low priority scene builder to pause for a given amount of miliseconds
    /// each time it processes a transaction.
    SimulateLongLowPrioritySceneBuild(u32),
}

#[derive(Clone, Deserialize, Serialize)]
pub enum ApiMsg {
    /// Add/remove/update images and fonts.
    UpdateResources(Vec<ResourceUpdate>),
    /// Gets the glyph dimensions
    GetGlyphDimensions(
        font::FontInstanceKey,
        Vec<font::GlyphIndex>,
        MsgSender<Vec<Option<font::GlyphDimensions>>>,
    ),
    /// Gets the glyph indices from a string
    GetGlyphIndices(font::FontKey, String, MsgSender<Vec<Option<u32>>>),
    /// Adds a new document namespace.
    CloneApi(MsgSender<IdNamespace>),
    /// Adds a new document namespace.
    CloneApiByClient(IdNamespace),
    /// Adds a new document with given initial size.
    AddDocument(DocumentId, DeviceIntSize, DocumentLayer),
    /// A message targeted at a particular document.
    UpdateDocuments(Vec<DocumentId>, Vec<TransactionMsg>),
    /// Deletes an existing document.
    DeleteDocument(DocumentId),
    /// An opaque handle that must be passed to the render notifier. It is used by Gecko
    /// to forward gecko-specific messages to the render thread preserving the ordering
    /// within the other messages.
    ExternalEvent(ExternalEvent),
    /// Removes all resources associated with a namespace.
    ClearNamespace(IdNamespace),
    /// Flush from the caches anything that isn't necessary, to free some memory.
    MemoryPressure,
    /// Collects a memory report.
    ReportMemory(MsgSender<MemoryReport>),
    /// Change debugging options.
    DebugCommand(DebugCommand),
    /// Wakes the render backend's event loop up. Needed when an event is communicated
    /// through another channel.
    WakeUp,
    WakeSceneBuilder,
    FlushSceneBuilder(MsgSender<()>),
    ShutDown,
}

impl fmt::Debug for ApiMsg {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(match *self {
            ApiMsg::UpdateResources(..) => "ApiMsg::UpdateResources",
            ApiMsg::GetGlyphDimensions(..) => "ApiMsg::GetGlyphDimensions",
            ApiMsg::GetGlyphIndices(..) => "ApiMsg::GetGlyphIndices",
            ApiMsg::CloneApi(..) => "ApiMsg::CloneApi",
            ApiMsg::CloneApiByClient(..) => "ApiMsg::CloneApiByClient",
            ApiMsg::AddDocument(..) => "ApiMsg::AddDocument",
            ApiMsg::UpdateDocuments(..) => "ApiMsg::UpdateDocuments",
            ApiMsg::DeleteDocument(..) => "ApiMsg::DeleteDocument",
            ApiMsg::ExternalEvent(..) => "ApiMsg::ExternalEvent",
            ApiMsg::ClearNamespace(..) => "ApiMsg::ClearNamespace",
            ApiMsg::MemoryPressure => "ApiMsg::MemoryPressure",
            ApiMsg::ReportMemory(..) => "ApiMsg::ReportMemory",
            ApiMsg::DebugCommand(..) => "ApiMsg::DebugCommand",
            ApiMsg::ShutDown => "ApiMsg::ShutDown",
            ApiMsg::WakeUp => "ApiMsg::WakeUp",
            ApiMsg::WakeSceneBuilder => "ApiMsg::WakeSceneBuilder",
            ApiMsg::FlushSceneBuilder(..) => "ApiMsg::FlushSceneBuilder",
        })
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, Ord, PartialEq, PartialOrd, Serialize)]
pub struct Epoch(pub u32);

impl Epoch {
    pub fn invalid() -> Epoch {
        Epoch(u32::MAX)
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, Hash, Ord, PartialOrd, Deserialize, Serialize)]
pub struct IdNamespace(pub u32);

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, MallocSizeOf, PartialEq, Serialize)]
pub struct DocumentId {
    pub namespace_id: IdNamespace,
    pub id: u32,
}

impl DocumentId {
    pub fn new(namespace_id: IdNamespace, id: u32) -> Self {
        DocumentId {
            namespace_id,
            id,
        }
    }

    pub const INVALID: DocumentId = DocumentId { namespace_id: IdNamespace(0), id: 0 };
}

/// This type carries no valuable semantics for WR. However, it reflects the fact that
/// clients (Servo) may generate pipelines by different semi-independent sources.
/// These pipelines still belong to the same `IdNamespace` and the same `DocumentId`.
/// Having this extra Id field enables them to generate `PipelineId` without collision.
pub type PipelineSourceId = u32;

/// From the point of view of WR, `PipelineId` is completely opaque and generic as long as
/// it's clonable, serializable, comparable, and hashable.
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, Eq, Hash, MallocSizeOf, PartialEq, Serialize)]
pub struct PipelineId(pub PipelineSourceId, pub u32);

impl PipelineId {
    pub fn dummy() -> Self {
        PipelineId(0, 0)
    }
}

#[derive(Copy, Clone, Debug, MallocSizeOf, Serialize, Deserialize)]
pub enum ClipIntern {}
#[derive(Copy, Clone, Debug, MallocSizeOf, Serialize, Deserialize)]
pub enum FilterDataIntern {}

/// Information specific to a primitive type that
/// uniquely identifies a primitive template by key.
#[derive(Debug, Clone, Eq, MallocSizeOf, PartialEq, Hash, Serialize, Deserialize)]
pub enum PrimitiveKeyKind {
    /// Clear an existing rect, used for special effects on some platforms.
    Clear,
    Rectangle {
        color: ColorU,
    },
}

/// Meta-macro to enumerate the various interner identifiers and types.
///
/// IMPORTANT: Keep this synchronized with the list in mozilla-central located at
/// gfx/webrender_bindings/webrender_ffi.h
///
/// Note that this could be a lot less verbose if concat_idents! were stable. :-(
#[macro_export]
macro_rules! enumerate_interners {
    ($macro_name: ident) => {
        $macro_name! {
            clip: ClipIntern,
            prim: PrimitiveKeyKind,
            normal_border: NormalBorderPrim,
            image_border: ImageBorder,
            image: Image,
            yuv_image: YuvImage,
            line_decoration: LineDecoration,
            linear_grad: LinearGradient,
            radial_grad: RadialGradient,
            picture: Picture,
            text_run: TextRun,
            filter_data: FilterDataIntern,
        }
    }
}

macro_rules! declare_interning_memory_report {
    ( $( $name:ident: $ty:ident, )+ ) => {
        #[repr(C)]
        #[derive(AddAssign, Clone, Debug, Default, Deserialize, Serialize)]
        pub struct InternerSubReport {
            $(
                pub $name: usize,
            )+
        }
    }
}

enumerate_interners!(declare_interning_memory_report);

/// Memory report for interning-related data structures.
/// cbindgen:derive-eq=false
#[repr(C)]
#[derive(Clone, Debug, Default, Deserialize, Serialize)]
pub struct InterningMemoryReport {
    pub interners: InternerSubReport,
    pub data_stores: InternerSubReport,
}

impl ::std::ops::AddAssign for InterningMemoryReport {
    fn add_assign(&mut self, other: InterningMemoryReport) {
        self.interners += other.interners;
        self.data_stores += other.data_stores;
    }
}

/// Collection of heap sizes, in bytes.
/// cbindgen:derive-eq=false
#[repr(C)]
#[derive(AddAssign, Clone, Debug, Default, Deserialize, Serialize)]
pub struct MemoryReport {
    //
    // CPU Memory.
    //
    pub clip_stores: usize,
    pub gpu_cache_metadata: usize,
    pub gpu_cache_cpu_mirror: usize,
    pub render_tasks: usize,
    pub hit_testers: usize,
    pub fonts: usize,
    pub images: usize,
    pub rasterized_blobs: usize,
    pub shader_cache: usize,
    pub interning: InterningMemoryReport,

    //
    // GPU memory.
    //
    pub gpu_cache_textures: usize,
    pub vertex_data_textures: usize,
    pub render_target_textures: usize,
    pub texture_cache_textures: usize,
    pub depth_target_textures: usize,
    pub swap_chain: usize,
}

/// A C function that takes a pointer to a heap allocation and returns its size.
///
/// This is borrowed from the malloc_size_of crate, upon which we want to avoid
/// a dependency from WebRender.
pub type VoidPtrToSizeFn = unsafe extern "C" fn(ptr: *const c_void) -> usize;

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
    pub fn from_raw(raw: usize) -> Self {
        ExternalEvent { raw }
    }
    /// Consumes self to make it obvious that the event should be forwarded only once.
    pub fn unwrap(self) -> usize {
        self.raw
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub enum ScrollClamping {
    ToContentBounds,
    NoClamping,
}

#[derive(Clone, Deserialize, Serialize)]
pub struct RenderApiSender {
    api_sender: MsgSender<ApiMsg>,
    payload_sender: PayloadSender,
}

impl RenderApiSender {
    pub fn new(api_sender: MsgSender<ApiMsg>, payload_sender: PayloadSender) -> Self {
        RenderApiSender {
            api_sender,
            payload_sender,
        }
    }

    /// Creates a new resource API object with a dedicated namespace.
    pub fn create_api(&self) -> RenderApi {
        let (sync_tx, sync_rx) =
            channel::msg_channel().expect("Failed to create channel");
        let msg = ApiMsg::CloneApi(sync_tx);
        self.api_sender.send(msg).expect("Failed to send CloneApi message");
        let namespace_id = match sync_rx.recv() {
            Ok(id) => id,
            Err(e) => {
                // This is used to discover the underlying cause of https://github.com/servo/servo/issues/13480.
                let webrender_is_alive = self.api_sender.send(ApiMsg::WakeUp);
                if webrender_is_alive.is_err() {
                    panic!("Webrender was shut down before processing CloneApi: {}", e);
                } else {
                    panic!("CloneApi message response was dropped while Webrender was still alive: {}", e);
                }
            }
        };
        RenderApi {
            api_sender: self.api_sender.clone(),
            payload_sender: self.payload_sender.clone(),
            namespace_id,
            next_id: Cell::new(ResourceId(0)),
        }
    }

    /// Creates a new resource API object with a dedicated namespace.
    /// Namespace id is allocated by client.
    ///
    /// The function could be used only when RendererOptions::namespace_alloc_by_client is true.
    /// When the option is true, create_api() could not be used to prevent namespace id conflict.
    pub fn create_api_by_client(&self, namespace_id: IdNamespace) -> RenderApi {
        let msg = ApiMsg::CloneApiByClient(namespace_id);
        self.api_sender.send(msg).expect("Failed to send CloneApiByClient message");
        RenderApi {
            api_sender: self.api_sender.clone(),
            payload_sender: self.payload_sender.clone(),
            namespace_id,
            next_id: Cell::new(ResourceId(0)),
        }
    }
}

bitflags! {
    #[repr(C)]
    #[derive(Default, Deserialize, MallocSizeOf, Serialize)]
    pub struct DebugFlags: u32 {
        /// Display the frame profiler on screen.
        const PROFILER_DBG          = 1 << 0;
        /// Display intermediate render targets on screen.
        const RENDER_TARGET_DBG     = 1 << 1;
        /// Display all texture cache pages on screen.
        const TEXTURE_CACHE_DBG     = 1 << 2;
        /// Display GPU timing results.
        const GPU_TIME_QUERIES      = 1 << 3;
        const GPU_SAMPLE_QUERIES    = 1 << 4;
        const DISABLE_BATCHING      = 1 << 5;
        const EPOCHS                = 1 << 6;
        const COMPACT_PROFILER      = 1 << 7;
        const ECHO_DRIVER_MESSAGES  = 1 << 8;
        /// Show an indicator that moves every time a frame is rendered.
        const NEW_FRAME_INDICATOR   = 1 << 9;
        /// Show an indicator that moves every time a scene is built.
        const NEW_SCENE_INDICATOR   = 1 << 10;
        /// Show an overlay displaying overdraw amount.
        const SHOW_OVERDRAW         = 1 << 11;
        /// Display the contents of GPU cache.
        const GPU_CACHE_DBG         = 1 << 12;
        const SLOW_FRAME_INDICATOR  = 1 << 13;
        const TEXTURE_CACHE_DBG_CLEAR_EVICTED = 1 << 14;
        /// Show picture caching debug overlay
        const PICTURE_CACHING_DBG   = 1 << 15;
        /// Highlight all primitives with colors based on kind.
        const PRIMITIVE_DBG = 1 << 16;
        /// Draw a zoom widget showing part of the framebuffer zoomed in.
        const ZOOM_DBG = 1 << 17;
        /// Scale the debug renderer down for a smaller screen. This will disrupt
        /// any mapping between debug display items and page content, so shouldn't
        /// be used with overlays like the picture caching or primitive display.
        const SMALL_SCREEN = 1 << 18;
        /// Disable various bits of the WebRender pipeline, to help narrow
        /// down where slowness might be coming from.
        const DISABLE_OPAQUE_PASS = 1 << 19;
        const DISABLE_ALPHA_PASS = 1 << 20;
        const DISABLE_CLIP_MASKS = 1 << 21;
        const DISABLE_TEXT_PRIMS = 1 << 22;
        const DISABLE_GRADIENT_PRIMS = 1 << 23;
    }
}

pub struct RenderApi {
    api_sender: MsgSender<ApiMsg>,
    payload_sender: PayloadSender,
    namespace_id: IdNamespace,
    next_id: Cell<ResourceId>,
}

impl RenderApi {
    pub fn get_namespace_id(&self) -> IdNamespace {
        self.namespace_id
    }

    pub fn clone_sender(&self) -> RenderApiSender {
        RenderApiSender::new(self.api_sender.clone(), self.payload_sender.clone())
    }

    pub fn add_document(&self, initial_size: DeviceIntSize, layer: DocumentLayer) -> DocumentId {
        let new_id = self.next_unique_id();
        self.add_document_with_id(initial_size, layer, new_id)
    }

    pub fn add_document_with_id(&self,
                                initial_size: DeviceIntSize,
                                layer: DocumentLayer,
                                id: u32) -> DocumentId {
        let document_id = DocumentId::new(self.namespace_id, id);

        let msg = ApiMsg::AddDocument(document_id, initial_size, layer);
        self.api_sender.send(msg).unwrap();

        document_id
    }

    pub fn delete_document(&self, document_id: DocumentId) {
        let msg = ApiMsg::DeleteDocument(document_id);
        self.api_sender.send(msg).unwrap();
    }

    pub fn generate_font_key(&self) -> font::FontKey {
        let new_id = self.next_unique_id();
        font::FontKey::new(self.namespace_id, new_id)
    }

    pub fn generate_font_instance_key(&self) -> font::FontInstanceKey {
        let new_id = self.next_unique_id();
        font::FontInstanceKey::new(self.namespace_id, new_id)
    }

    /// Gets the dimensions for the supplied glyph keys
    ///
    /// Note: Internally, the internal texture cache doesn't store
    /// 'empty' textures (height or width = 0)
    /// This means that glyph dimensions e.g. for spaces (' ') will mostly be None.
    pub fn get_glyph_dimensions(
        &self,
        font: font::FontInstanceKey,
        glyph_indices: Vec<font::GlyphIndex>,
    ) -> Vec<Option<font::GlyphDimensions>> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::GetGlyphDimensions(font, glyph_indices, tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    /// Gets the glyph indices for the supplied string. These
    /// can be used to construct GlyphKeys.
    pub fn get_glyph_indices(&self, font_key: font::FontKey, text: &str) -> Vec<Option<u32>> {
        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::GetGlyphIndices(font_key, text.to_string(), tx);
        self.api_sender.send(msg).unwrap();
        rx.recv().unwrap()
    }

    /// Creates an `ImageKey`.
    pub fn generate_image_key(&self) -> ImageKey {
        let new_id = self.next_unique_id();
        ImageKey::new(self.namespace_id, new_id)
    }

    /// Creates a `BlobImageKey`.
    pub fn generate_blob_image_key(&self) -> BlobImageKey {
        BlobImageKey(self.generate_image_key())
    }

    /// Add/remove/update resources such as images and fonts.
    pub fn update_resources(&self, resources: Vec<ResourceUpdate>) {
        if resources.is_empty() {
            return;
        }
        self.api_sender
            .send(ApiMsg::UpdateResources(resources))
            .unwrap();
    }

    pub fn send_external_event(&self, evt: ExternalEvent) {
        let msg = ApiMsg::ExternalEvent(evt);
        self.api_sender.send(msg).unwrap();
    }

    pub fn notify_memory_pressure(&self) {
        self.api_sender.send(ApiMsg::MemoryPressure).unwrap();
    }

    pub fn report_memory(&self) -> MemoryReport {
        let (tx, rx) = channel::msg_channel().unwrap();
        self.api_sender.send(ApiMsg::ReportMemory(tx)).unwrap();
        rx.recv().unwrap()
    }

    pub fn set_debug_flags(&self, flags: DebugFlags) {
        let cmd = DebugCommand::SetFlags(flags);
        self.api_sender.send(ApiMsg::DebugCommand(cmd)).unwrap();
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
                namespace: self.namespace_id,
                uid: new_id,
            },
            _phantom: PhantomData,
        }
    }

    #[inline]
    fn next_unique_id(&self) -> u32 {
        let ResourceId(id) = self.next_id.get();
        self.next_id.set(ResourceId(id + 1));
        id
    }

    // For use in Wrench only
    #[doc(hidden)]
    pub fn send_message(&self, msg: ApiMsg) {
        self.api_sender.send(msg).unwrap();
    }

    // For use in Wrench only
    #[doc(hidden)]
    pub fn send_payload(&self, data: &[u8]) {
        self.payload_sender
            .send_payload(Payload::from_data(data))
            .unwrap();
    }

    /// A helper method to send document messages.
    fn send_scene_msg(&self, document_id: DocumentId, msg: SceneMsg) {
        // This assertion fails on Servo use-cases, because it creates different
        // `RenderApi` instances for layout and compositor.
        //assert_eq!(document_id.0, self.namespace_id);
        self.api_sender
            .send(ApiMsg::UpdateDocuments(vec![document_id], vec![TransactionMsg::scene_message(msg)]))
            .unwrap()
    }

    /// A helper method to send document messages.
    fn send_frame_msg(&self, document_id: DocumentId, msg: FrameMsg) {
        // This assertion fails on Servo use-cases, because it creates different
        // `RenderApi` instances for layout and compositor.
        //assert_eq!(document_id.0, self.namespace_id);
        self.api_sender
            .send(ApiMsg::UpdateDocuments(vec![document_id], vec![TransactionMsg::frame_message(msg)]))
            .unwrap()
    }

    pub fn send_transaction(&self, document_id: DocumentId, transaction: Transaction) {
        let (msg, payloads) = transaction.finalize();
        for payload in payloads {
            self.payload_sender.send_payload(payload).unwrap();
        }
        self.api_sender.send(ApiMsg::UpdateDocuments(vec![document_id], vec![msg])).unwrap();
    }

    pub fn send_transactions(&self, document_ids: Vec<DocumentId>, mut transactions: Vec<Transaction>) {
        debug_assert!(document_ids.len() == transactions.len());
        let length = document_ids.len();
        let (msgs, mut document_payloads) = transactions.drain(..)
            .fold((Vec::with_capacity(length), Vec::with_capacity(length)),
                |(mut msgs, mut document_payloads), transaction| {
                    let (msg, payloads) = transaction.finalize();
                    msgs.push(msg);
                    document_payloads.push(payloads);
                    (msgs, document_payloads)
                });
        for payload in document_payloads.drain(..).flatten() {
            self.payload_sender.send_payload(payload).unwrap();
        }
        self.api_sender.send(ApiMsg::UpdateDocuments(document_ids.clone(), msgs)).unwrap();
    }

    /// Does a hit test on display items in the specified document, at the given
    /// point. If a pipeline_id is specified, it is used to further restrict the
    /// hit results so that only items inside that pipeline are matched. If the
    /// HitTestFlags argument contains the FIND_ALL flag, then the vector of hit
    /// results will contain all display items that match, ordered from front
    /// to back.
    pub fn hit_test(&self,
                    document_id: DocumentId,
                    pipeline_id: Option<PipelineId>,
                    point: WorldPoint,
                    flags: HitTestFlags)
                    -> HitTestResult {
        let (tx, rx) = channel::msg_channel().unwrap();

        self.send_frame_msg(
            document_id,
            FrameMsg::HitTest(pipeline_id, point, flags, tx)
        );
        rx.recv().unwrap()
    }

    /// Setup the output region in the framebuffer for a given document.
    pub fn set_document_view(
        &self,
        document_id: DocumentId,
        device_rect: DeviceIntRect,
        device_pixel_ratio: f32,
    ) {
        self.send_scene_msg(
            document_id,
            SceneMsg::SetDocumentView { device_rect, device_pixel_ratio },
        );
    }

    /// Setup the output region in the framebuffer for a given document.
    /// Enable copying of the output of this pipeline id to
    /// an external texture for callers to consume.
    pub fn enable_frame_output(
        &self,
        document_id: DocumentId,
        pipeline_id: PipelineId,
        enable: bool,
    ) {
        self.send_scene_msg(
            document_id,
            SceneMsg::EnableFrameOutput(pipeline_id, enable),
        );
    }


    pub fn get_scroll_node_state(&self, document_id: DocumentId) -> Vec<ScrollNodeState> {
        let (tx, rx) = channel::msg_channel().unwrap();
        self.send_frame_msg(document_id, FrameMsg::GetScrollNodeState(tx));
        rx.recv().unwrap()
    }

    pub fn wake_scene_builder(&self) {
        self.send_message(ApiMsg::WakeSceneBuilder);
    }

    /// Block until a round-trip to the scene builder thread has completed. This
    /// ensures that any transactions (including ones deferred to the scene
    /// builder thread) have been processed.
    pub fn flush_scene_builder(&self) {
        let (tx, rx) = channel::msg_channel().unwrap();
        self.send_message(ApiMsg::FlushSceneBuilder(tx));
        rx.recv().unwrap(); // block until done
    }

    /// Save a capture of the current frame state for debugging.
    pub fn save_capture(&self, path: PathBuf, bits: CaptureBits) {
        let msg = ApiMsg::DebugCommand(DebugCommand::SaveCapture(path, bits));
        self.send_message(msg);
    }

    /// Load a capture of the current frame state for debugging.
    pub fn load_capture(&self, path: PathBuf) -> Vec<CapturedDocument> {
        // First flush the scene builder otherwise async scenes might clobber
        // the capture we are about to load.
        self.flush_scene_builder();

        let (tx, rx) = channel::msg_channel().unwrap();
        let msg = ApiMsg::DebugCommand(DebugCommand::LoadCapture(path, tx));
        self.send_message(msg);

        let mut documents = Vec::new();
        while let Ok(captured_doc) = rx.recv() {
            documents.push(captured_doc);
        }
        documents
    }

    pub fn send_debug_cmd(&self, cmd: DebugCommand) {
        let msg = ApiMsg::DebugCommand(cmd);
        self.send_message(msg);
    }
}

impl Drop for RenderApi {
    fn drop(&mut self) {
        let msg = ApiMsg::ClearNamespace(self.namespace_id);
        let _ = self.api_sender.send(msg);
    }
}

#[derive(Clone, Deserialize, Serialize)]
pub struct ScrollNodeState {
    pub id: di::ExternalScrollId,
    pub scroll_offset: LayoutVector2D,
}

#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub enum ScrollLocation {
    /// Scroll by a certain amount.
    Delta(LayoutVector2D),
    /// Scroll to very top of element.
    Start,
    /// Scroll to very bottom of element.
    End,
}

/// Represents a zoom factor.
#[derive(Clone, Copy, Serialize, Deserialize, Debug)]
pub struct ZoomFactor(f32);

impl ZoomFactor {
    /// Construct a new zoom factor.
    pub fn new(scale: f32) -> Self {
        ZoomFactor(scale)
    }

    /// Get the zoom factor as an untyped float.
    pub fn get(&self) -> f32 {
        self.0
    }
}

#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, MallocSizeOf, PartialEq, Serialize, Eq, Hash)]
pub struct PropertyBindingId {
    namespace: IdNamespace,
    uid: u32,
}

impl PropertyBindingId {
    pub fn new(value: u64) -> Self {
        PropertyBindingId {
            namespace: IdNamespace((value >> 32) as u32),
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
        PropertyValue { key: *self, value }
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
/// Note that Binding has also a non-animated value, the value is
/// used for the case where the animation is still in-delay phase
/// (i.e. the animation doesn't produce any animation values).
#[repr(C)]
#[derive(Clone, Copy, Debug, Deserialize, PartialEq, Serialize)]
pub enum PropertyBinding<T> {
    Value(T),
    Binding(PropertyBindingKey<T>, T),
}

impl<T> From<T> for PropertyBinding<T> {
    fn from(value: T) -> PropertyBinding<T> {
        PropertyBinding::Value(value)
    }
}

/// The current value of an animated property. This is
/// supplied by the calling code.
#[derive(Clone, Copy, Debug, Deserialize, Serialize, PartialEq)]
pub struct PropertyValue<T> {
    pub key: PropertyBindingKey<T>,
    pub value: T,
}

/// When using `generate_frame()`, a list of `PropertyValue` structures
/// can optionally be supplied to provide the current value of any
/// animated properties.
#[derive(Clone, Deserialize, Serialize, Debug, PartialEq, Default)]
pub struct DynamicProperties {
    pub transforms: Vec<PropertyValue<LayoutTransform>>,
    pub floats: Vec<PropertyValue<f32>>,
}

pub trait RenderNotifier: Send {
    fn clone(&self) -> Box<RenderNotifier>;
    fn wake_up(&self);
    fn new_frame_ready(&self, _: DocumentId, scrolled: bool, composite_needed: bool, render_time_ns: Option<u64>);
    fn external_event(&self, _evt: ExternalEvent) {
        unimplemented!()
    }
    fn shut_down(&self) {}
}

#[repr(u32)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub enum Checkpoint {
    SceneBuilt,
    FrameBuilt,
    FrameTexturesUpdated,
    FrameRendered,
    /// NotificationRequests get notified with this if they get dropped without having been
    /// notified. This provides the guarantee that if a request is created it will get notified.
    TransactionDropped,
}

pub trait NotificationHandler : Send + Sync {
    fn notify(&self, when: Checkpoint);
}

pub struct NotificationRequest {
    handler: Option<Box<NotificationHandler>>,
    when: Checkpoint,
}

impl NotificationRequest {
    pub fn new(when: Checkpoint, handler: Box<NotificationHandler>) -> Self {
        NotificationRequest {
            handler: Some(handler),
            when,
        }
    }

    pub fn when(&self) -> Checkpoint { self.when }

    pub fn notify(mut self) {
        if let Some(handler) = self.handler.take() {
            handler.notify(self.when);
        }
    }
}

impl Drop for NotificationRequest {
    fn drop(&mut self) {
        if let Some(ref mut handler) = self.handler {
            handler.notify(Checkpoint::TransactionDropped);
        }
    }
}

// This Clone impl yields an "empty" request because we don't want the requests
// to be notified twice so the request is owned by only one of the API messages
// (the original one) after the clone.
// This works in practice because the notifications requests are used for
// synchronization so we don't need to include them in the recording mechanism
// in wrench that clones the messages.
impl Clone for NotificationRequest {
    fn clone(&self) -> Self {
        NotificationRequest {
            when: self.when,
            handler: None,
        }
    }
}

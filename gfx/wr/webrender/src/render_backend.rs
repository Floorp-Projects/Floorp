/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! The high-level module responsible for managing the pipeline and preparing
//! commands to be issued by the `Renderer`.
//!
//! See the comment at the top of the `renderer` module for a description of
//! how these two pieces interact.

use api::{ApiMsg, BuiltDisplayList, ClearCache, DebugCommand, DebugFlags};
#[cfg(feature = "debugger")]
use api::{BuiltDisplayListIter, DisplayItem};
use api::{DocumentId, DocumentLayer, ExternalScrollId, FrameMsg, HitTestFlags, HitTestResult};
use api::{IdNamespace, MemoryReport, PipelineId, RenderNotifier, SceneMsg, ScrollClamping};
use api::{ScrollLocation, ScrollNodeState, TransactionMsg, ResourceUpdate, BlobImageKey};
use api::{NotificationRequest, Checkpoint};
use api::{ClipIntern, FilterDataIntern, PrimitiveKeyKind};
use api::units::*;
use api::channel::{MsgReceiver, MsgSender, Payload};
#[cfg(feature = "capture")]
use api::CaptureBits;
#[cfg(feature = "replay")]
use api::CapturedDocument;
use crate::clip_scroll_tree::{SpatialNodeIndex, ClipScrollTree};
#[cfg(feature = "debugger")]
use crate::debug_server;
use crate::frame_builder::{FrameBuilder, FrameBuilderConfig};
use crate::glyph_rasterizer::{FontInstance};
use crate::gpu_cache::GpuCache;
use crate::hit_test::{HitTest, HitTester};
use crate::intern::DataStore;
use crate::internal_types::{DebugOutput, FastHashMap, FastHashSet, RenderedDocument, ResultMsg};
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps};
use crate::picture::RetainedTiles;
use crate::prim_store::{PrimitiveScratchBuffer, PrimitiveInstance};
use crate::prim_store::{PrimitiveInstanceKind, PrimTemplateCommonData};
use crate::prim_store::interned::*;
use crate::profiler::{BackendProfileCounters, IpcProfileCounters, ResourceProfileCounters};
use crate::record::ApiRecordingReceiver;
use crate::render_task::RenderTaskGraphCounters;
use crate::renderer::{AsyncPropertySampler, PipelineInfo};
use crate::resource_cache::ResourceCache;
#[cfg(feature = "replay")]
use crate::resource_cache::PlainCacheOwn;
#[cfg(any(feature = "capture", feature = "replay"))]
use crate::resource_cache::PlainResources;
use crate::scene::{Scene, SceneProperties};
use crate::scene_builder::*;
#[cfg(feature = "serialize")]
use serde::{Serialize, Deserialize};
#[cfg(feature = "debugger")]
use serde_json;
#[cfg(any(feature = "capture", feature = "replay"))]
use std::path::PathBuf;
use std::sync::Arc;
use std::sync::atomic::{AtomicUsize, Ordering};
use std::sync::mpsc::{channel, Sender, Receiver};
use std::time::{UNIX_EPOCH, SystemTime};
use std::u32;
#[cfg(feature = "replay")]
use crate::tiling::Frame;
use time::precise_time_ns;
use crate::util::{Recycler, VecHelper, drain_filter};


#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
#[derive(Clone)]
pub struct DocumentView {
    pub device_rect: DeviceIntRect,
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

#[derive(Copy, Clone, Hash, MallocSizeOf, PartialEq, PartialOrd, Debug, Eq, Ord)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameId(usize);

impl FrameId {
    /// Returns a FrameId corresponding to the first frame.
    ///
    /// Note that we use 0 as the internal id here because the current code
    /// increments the frame id at the beginning of the frame, rather than
    /// at the end, and we want the first frame to be 1. It would probably
    /// be sensible to move the advance() call to after frame-building, and
    /// then make this method return FrameId(1).
    pub fn first() -> Self {
        FrameId(0)
    }

    /// Returns the backing usize for this FrameId.
    pub fn as_usize(&self) -> usize {
        self.0
    }

    /// Advances this FrameId to the next frame.
    pub fn advance(&mut self) {
        self.0 += 1;
    }

    /// An invalid sentinel FrameId, which will always compare less than
    /// any valid FrameId.
    pub const INVALID: FrameId = FrameId(0);
}

impl ::std::ops::Add<usize> for FrameId {
    type Output = Self;
    fn add(self, other: usize) -> FrameId {
        FrameId(self.0 + other)
    }
}

impl ::std::ops::Sub<usize> for FrameId {
    type Output = Self;
    fn sub(self, other: usize) -> FrameId {
        assert!(self.0 >= other, "Underflow subtracting FrameIds");
        FrameId(self.0 - other)
    }
}

/// Identifier to track a sequence of frames.
///
/// This is effectively a `FrameId` with a ridealong timestamp corresponding
/// to when advance() was called, which allows for more nuanced cache eviction
/// decisions. As such, we use the `FrameId` for equality and comparison, since
/// we should never have two `FrameStamps` with the same id but different
/// timestamps.
#[derive(Copy, Clone, Debug, MallocSizeOf)]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
pub struct FrameStamp {
    id: FrameId,
    time: SystemTime,
    document_id: DocumentId,
}

impl Eq for FrameStamp {}

impl PartialEq for FrameStamp {
    fn eq(&self, other: &Self) -> bool {
        // We should not be checking equality unless the documents are the same
        debug_assert!(self.document_id == other.document_id);
        self.id == other.id
    }
}

impl PartialOrd for FrameStamp {
    fn partial_cmp(&self, other: &Self) -> Option<::std::cmp::Ordering> {
        self.id.partial_cmp(&other.id)
    }
}

impl FrameStamp {
    /// Gets the FrameId in this stamp.
    pub fn frame_id(&self) -> FrameId {
        self.id
    }

    /// Gets the time associated with this FrameStamp.
    pub fn time(&self) -> SystemTime {
        self.time
    }

    /// Gets the DocumentId in this stamp.
    pub fn document_id(&self) -> DocumentId {
        self.document_id
    }

    pub fn is_valid(&self) -> bool {
        // If any fields are their default values, the whole struct should equal INVALID
        debug_assert!((self.time != UNIX_EPOCH && self.id != FrameId(0) && self.document_id != DocumentId::INVALID) ||
                      *self == Self::INVALID);
        self.document_id != DocumentId::INVALID
    }

    /// Returns a FrameStamp corresponding to the first frame.
    pub fn first(document_id: DocumentId) -> Self {
        FrameStamp {
            id: FrameId::first(),
            time: SystemTime::now(),
            document_id: document_id,
        }
    }

    /// Advances to a new frame.
    pub fn advance(&mut self) {
        self.id.advance();
        self.time = SystemTime::now();
    }

    /// An invalid sentinel FrameStamp.
    pub const INVALID: FrameStamp = FrameStamp {
        id: FrameId(0),
        time: UNIX_EPOCH,
        document_id: DocumentId::INVALID,
    };
}

macro_rules! declare_data_stores {
    ( $( $name:ident : $ty:ty, )+ ) => {
        /// A collection of resources that are shared by clips, primitives
        /// between display lists.
        #[cfg_attr(feature = "capture", derive(Serialize))]
        #[cfg_attr(feature = "replay", derive(Deserialize))]
        #[derive(Default)]
        pub struct DataStores {
            $(
                pub $name: DataStore<$ty>,
            )+
        }

        impl DataStores {
            /// Reports CPU heap usage.
            fn report_memory(&self, ops: &mut MallocSizeOfOps, r: &mut MemoryReport) {
                $(
                    r.interning.data_stores.$name += self.$name.size_of(ops);
                )+
            }

            fn apply_updates(
                &mut self,
                updates: InternerUpdates,
                profile_counters: &mut BackendProfileCounters,
            ) {
                $(
                    self.$name.apply_updates(
                        updates.$name,
                        &mut profile_counters.intern.$name,
                    );
                )+
            }
        }
    }
}

enumerate_interners!(declare_data_stores);

impl DataStores {
    pub fn as_common_data(
        &self,
        prim_inst: &PrimitiveInstance
    ) -> &PrimTemplateCommonData {
        match prim_inst.kind {
            PrimitiveInstanceKind::Rectangle { data_handle, .. } |
            PrimitiveInstanceKind::Clear { data_handle, .. } => {
                let prim_data = &self.prim[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::Image { data_handle, .. } => {
                let prim_data = &self.image[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::ImageBorder { data_handle, .. } => {
                let prim_data = &self.image_border[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::LineDecoration { data_handle, .. } => {
                let prim_data = &self.line_decoration[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::LinearGradient { data_handle, .. } => {
                let prim_data = &self.linear_grad[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::NormalBorder { data_handle, .. } => {
                let prim_data = &self.normal_border[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::Picture { data_handle, .. } => {
                let prim_data = &self.picture[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::RadialGradient { data_handle, .. } => {
                let prim_data = &self.radial_grad[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::TextRun { data_handle, .. }  => {
                let prim_data = &self.text_run[data_handle];
                &prim_data.common
            }
            PrimitiveInstanceKind::YuvImage { data_handle, .. } => {
                let prim_data = &self.yuv_image[data_handle];
                &prim_data.common
            }
        }
    }
}

struct Document {
    // The id of this document
    id: DocumentId,
    // The latest built scene, usable to build frames.
    // received from the scene builder thread.
    scene: Scene,

    // Temporary list of removed pipelines received from the scene builder
    // thread and forwarded to the renderer.
    removed_pipelines: Vec<(PipelineId, DocumentId)>,

    view: DocumentView,

    /// The ClipScrollTree for this document which tracks SpatialNodes, ClipNodes, and ClipChains.
    /// This is stored here so that we are able to preserve scrolling positions between rendered
    /// frames.
    clip_scroll_tree: ClipScrollTree,

    /// The id and time of the current frame.
    stamp: FrameStamp,

    // the `Option` here is only to deal with borrow checker
    frame_builder: Option<FrameBuilder>,
    // A set of pipelines that the caller has requested be
    // made available as output textures.
    output_pipelines: FastHashSet<PipelineId>,

    /// A data structure to allow hit testing against rendered frames. This is updated
    /// every time we produce a fully rendered frame.
    hit_tester: Option<HitTester>,

    /// Properties that are resolved during frame building and can be changed at any time
    /// without requiring the scene to be re-built.
    dynamic_properties: SceneProperties,

    /// Track whether the last built frame is up to date or if it will need to be re-built
    /// before rendering again.
    frame_is_valid: bool,
    hit_tester_is_valid: bool,
    rendered_frame_is_valid: bool,
    // We track this information to be able to display debugging information from the
    // renderer.
    has_built_scene: bool,

    data_stores: DataStores,

    /// Contains various vecs of data that is used only during frame building,
    /// where we want to recycle the memory each new display list, to avoid constantly
    /// re-allocating and moving memory around.
    scratch: PrimitiveScratchBuffer,
    /// Keep track of the size of render task graph to pre-allocate memory up-front
    /// the next frame.
    render_task_counters: RenderTaskGraphCounters,
}

impl Document {
    pub fn new(
        id: DocumentId,
        size: DeviceIntSize,
        layer: DocumentLayer,
        default_device_pixel_ratio: f32,
    ) -> Self {
        Document {
            id,
            scene: Scene::new(),
            removed_pipelines: Vec::new(),
            view: DocumentView {
                device_rect: size.into(),
                layer,
                pan: DeviceIntPoint::zero(),
                page_zoom_factor: 1.0,
                pinch_zoom_factor: 1.0,
                device_pixel_ratio: default_device_pixel_ratio,
            },
            clip_scroll_tree: ClipScrollTree::new(),
            stamp: FrameStamp::first(id),
            frame_builder: None,
            output_pipelines: FastHashSet::default(),
            hit_tester: None,
            dynamic_properties: SceneProperties::new(),
            frame_is_valid: false,
            hit_tester_is_valid: false,
            rendered_frame_is_valid: false,
            has_built_scene: false,
            data_stores: DataStores::default(),
            scratch: PrimitiveScratchBuffer::new(),
            render_task_counters: RenderTaskGraphCounters::new(),
        }
    }

    fn can_render(&self) -> bool {
        self.frame_builder.is_some() && self.scene.has_root_pipeline()
    }

    fn has_pixels(&self) -> bool {
        !self.view.device_rect.size.is_empty_or_negative()
    }

    fn process_frame_msg(
        &mut self,
        message: FrameMsg,
    ) -> DocumentOps {
        match message {
            FrameMsg::UpdateEpoch(pipeline_id, epoch) => {
                self.scene.update_epoch(pipeline_id, epoch);
            }
            FrameMsg::Scroll(delta, cursor) => {
                profile_scope!("Scroll");

                let node_index = match self.hit_tester {
                    Some(ref hit_tester) => {
                        // Ideally we would call self.scroll_nearest_scrolling_ancestor here, but
                        // we need have to avoid a double-borrow.
                        let test = HitTest::new(None, cursor, HitTestFlags::empty());
                        hit_tester.find_node_under_point(test)
                    }
                    None => {
                        None
                    }
                };

                if self.hit_tester.is_some() {
                    if self.scroll_nearest_scrolling_ancestor(delta, node_index) {
                        self.hit_tester_is_valid = false;
                        self.frame_is_valid = false;
                    }
                }

                return DocumentOps {
                    // TODO: Does it make sense to track this as a scrolling even if we
                    // ended up not scrolling anything?
                    scroll: true,
                    ..DocumentOps::nop()
                };
            }
            FrameMsg::HitTest(pipeline_id, point, flags, tx) => {
                if !self.hit_tester_is_valid {
                    self.rebuild_hit_tester();
                }

                let result = match self.hit_tester {
                    Some(ref hit_tester) => {
                        hit_tester.hit_test(HitTest::new(pipeline_id, point, flags))
                    }
                    None => HitTestResult { items: Vec::new() },
                };

                tx.send(result).unwrap();
            }
            FrameMsg::SetPan(pan) => {
                if self.view.pan != pan {
                    self.view.pan = pan;
                    self.hit_tester_is_valid = false;
                    self.frame_is_valid = false;
                }
            }
            FrameMsg::ScrollNodeWithId(origin, id, clamp) => {
                profile_scope!("ScrollNodeWithScrollId");

                if self.scroll_node(origin, id, clamp) {
                    self.hit_tester_is_valid = false;
                    self.frame_is_valid = false;
                }

                return DocumentOps {
                    scroll: true,
                    ..DocumentOps::nop()
                };
            }
            FrameMsg::GetScrollNodeState(tx) => {
                profile_scope!("GetScrollNodeState");
                tx.send(self.get_scroll_node_state()).unwrap();
            }
            FrameMsg::UpdateDynamicProperties(property_bindings) => {
                self.dynamic_properties.set_properties(property_bindings);
            }
            FrameMsg::AppendDynamicProperties(property_bindings) => {
                self.dynamic_properties.add_properties(property_bindings);
            }
            FrameMsg::SetPinchZoom(factor) => {
                if self.view.pinch_zoom_factor != factor.get() {
                    self.view.pinch_zoom_factor = factor.get();
                    self.frame_is_valid = false;
                }
            }
        }

        DocumentOps::nop()
    }

    fn build_frame(
        &mut self,
        resource_cache: &mut ResourceCache,
        gpu_cache: &mut GpuCache,
        resource_profile: &mut ResourceProfileCounters,
        debug_flags: DebugFlags,
    ) -> RenderedDocument {
        let accumulated_scale_factor = self.view.accumulated_scale_factor();
        let pan = self.view.pan.to_f32() / accumulated_scale_factor;

        // Advance to the next frame.
        self.stamp.advance();

        assert!(self.stamp.frame_id() != FrameId::INVALID,
                "First frame increment must happen before build_frame()");

        let frame = {
            let frame_builder = self.frame_builder.as_mut().unwrap();
            let frame = frame_builder.build(
                resource_cache,
                gpu_cache,
                self.stamp,
                &mut self.clip_scroll_tree,
                &self.scene.pipelines,
                accumulated_scale_factor,
                self.view.layer,
                self.view.device_rect.origin,
                pan,
                &mut resource_profile.texture_cache,
                &mut resource_profile.gpu_cache,
                &self.dynamic_properties,
                &mut self.data_stores,
                &mut self.scratch,
                &mut self.render_task_counters,
                debug_flags,
            );
            self.hit_tester = Some(frame_builder.create_hit_tester(
                &self.clip_scroll_tree,
                &self.data_stores.clip,
            ));
            frame
        };

        self.frame_is_valid = true;
        self.hit_tester_is_valid = true;

        let is_new_scene = self.has_built_scene;
        self.has_built_scene = false;

        RenderedDocument {
            frame,
            is_new_scene,
        }
    }

    fn rebuild_hit_tester(&mut self) {
        if let Some(ref mut frame_builder) = self.frame_builder {
            let accumulated_scale_factor = self.view.accumulated_scale_factor();
            let pan = self.view.pan.to_f32() / accumulated_scale_factor;

            self.clip_scroll_tree.update_tree(
                pan,
                &self.dynamic_properties,
            );

            self.hit_tester = Some(frame_builder.create_hit_tester(
                &self.clip_scroll_tree,
                &self.data_stores.clip,
            ));
            self.hit_tester_is_valid = true;
        }
    }

    pub fn updated_pipeline_info(&mut self) -> PipelineInfo {
        let removed_pipelines = self.removed_pipelines.take_and_preallocate();
        PipelineInfo {
            epochs: self.scene.pipeline_epochs.iter()
                .map(|(&pipeline_id, &epoch)| ((pipeline_id, self.id), epoch)).collect(),
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
        scroll_node_index: Option<SpatialNodeIndex>,
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

    pub fn new_async_scene_ready(
        &mut self,
        mut built_scene: BuiltScene,
        recycler: &mut Recycler,
    ) {
        self.scene = built_scene.scene;
        self.frame_is_valid = false;
        self.hit_tester_is_valid = false;

        // Give the old frame builder a chance to destroy any resources.
        // Right now, all this does is build a hash map of any cached
        // surface tiles, that can be provided to the next frame builder.
        let mut retained_tiles = RetainedTiles::new();
        if let Some(frame_builder) = self.frame_builder.take() {
            let globals = frame_builder.destroy(
                &mut retained_tiles,
                &self.clip_scroll_tree,
            );

            // Provide any cached tiles from the previous frame builder to
            // the newly built one.
            built_scene.frame_builder.set_retained_resources(
                retained_tiles,
                globals,
            );
        }

        self.frame_builder = Some(built_scene.frame_builder);

        self.scratch.recycle(recycler);

        let old_scrolling_states = self.clip_scroll_tree.drain();
        self.clip_scroll_tree = built_scene.clip_scroll_tree;
        self.clip_scroll_tree.finalize_and_apply_pending_scroll_offsets(old_scrolling_states);
    }
}

struct DocumentOps {
    scroll: bool,
}

impl DocumentOps {
    fn nop() -> Self {
        DocumentOps {
            scroll: false,
        }
    }
}

/// The unique id for WR resource identification.
/// The namespace_id should start from 1.
static NEXT_NAMESPACE_ID: AtomicUsize = AtomicUsize::new(1);

#[cfg(any(feature = "capture", feature = "replay"))]
#[cfg_attr(feature = "capture", derive(Serialize))]
#[cfg_attr(feature = "replay", derive(Deserialize))]
struct PlainRenderBackend {
    default_device_pixel_ratio: f32,
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
    payload_rx: Receiver<Payload>,
    result_tx: Sender<ResultMsg>,
    scene_tx: Sender<SceneBuilderRequest>,
    low_priority_scene_tx: Sender<SceneBuilderRequest>,
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
    size_of_ops: Option<MallocSizeOfOps>,
    debug_flags: DebugFlags,
    namespace_alloc_by_client: bool,

    recycler: Recycler,
}

impl RenderBackend {
    pub fn new(
        api_rx: MsgReceiver<ApiMsg>,
        payload_rx: Receiver<Payload>,
        result_tx: Sender<ResultMsg>,
        scene_tx: Sender<SceneBuilderRequest>,
        low_priority_scene_tx: Sender<SceneBuilderRequest>,
        scene_rx: Receiver<SceneBuilderResult>,
        default_device_pixel_ratio: f32,
        resource_cache: ResourceCache,
        notifier: Box<RenderNotifier>,
        frame_config: FrameBuilderConfig,
        recorder: Option<Box<ApiRecordingReceiver>>,
        sampler: Option<Box<AsyncPropertySampler + Send>>,
        size_of_ops: Option<MallocSizeOfOps>,
        debug_flags: DebugFlags,
        namespace_alloc_by_client: bool,
    ) -> RenderBackend {
        RenderBackend {
            api_rx,
            payload_rx,
            result_tx,
            scene_tx,
            low_priority_scene_tx,
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
            size_of_ops,
            debug_flags,
            namespace_alloc_by_client,
            recycler: Recycler::new(),
        }
    }

    fn process_scene_msg(
        &mut self,
        document_id: DocumentId,
        message: SceneMsg,
        frame_counter: u32,
        txn: &mut Transaction,
        ipc_profile_counters: &mut IpcProfileCounters,
    ) {
        let doc = self.documents.get_mut(&document_id).expect("No document?");

        match message {
            SceneMsg::UpdateEpoch(pipeline_id, epoch) => {
                txn.epoch_updates.push((pipeline_id, epoch));
            }
            SceneMsg::SetPageZoom(factor) => {
                doc.view.page_zoom_factor = factor.get();
            }
            SceneMsg::SetDocumentView {
                device_rect,
                device_pixel_ratio,
            } => {
                doc.view.device_rect = device_rect;
                doc.view.device_pixel_ratio = device_pixel_ratio;
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

                txn.display_list_updates.push(DisplayListUpdate {
                    built_display_list,
                    pipeline_id,
                    epoch,
                    background,
                    viewport_size,
                    content_size,
                });

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
            }
            SceneMsg::SetRootPipeline(pipeline_id) => {
                profile_scope!("SetRootPipeline");
                txn.set_root_pipeline = Some(pipeline_id);
            }
            SceneMsg::RemovePipeline(pipeline_id) => {
                profile_scope!("RemovePipeline");
                txn.removed_pipelines.push((pipeline_id, document_id));
            }
            SceneMsg::EnableFrameOutput(pipeline_id, enable) => {
                if enable {
                    doc.output_pipelines.insert(pipeline_id);
                } else {
                    doc.output_pipelines.remove(&pipeline_id);
                }
            }
        }
    }

    fn next_namespace_id(&self) -> IdNamespace {
        IdNamespace(NEXT_NAMESPACE_ID.fetch_add(1, Ordering::Relaxed) as u32)
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
                    SceneBuilderResult::Transactions(mut txns, result_tx) => {
                        self.prepare_for_frames();
                        self.maybe_force_nop_documents(
                            &mut frame_counter,
                            &mut profile_counters,
                            |document_id| txns.iter().any(|txn| txn.document_id == document_id));

                        for mut txn in txns.drain(..) {
                            let has_built_scene = txn.built_scene.is_some();
                            if let Some(doc) = self.documents.get_mut(&txn.document_id) {

                                doc.removed_pipelines.append(&mut txn.removed_pipelines);

                                if let Some(built_scene) = txn.built_scene.take() {
                                    doc.new_async_scene_ready(
                                        built_scene,
                                        &mut self.recycler,
                                    );
                                }

                                if let Some(ref tx) = result_tx {
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
                                if let Some(ref tx) = result_tx {
                                    tx.send(SceneSwapResult::Aborted).unwrap();
                                }
                                continue;
                            }

                            self.resource_cache.add_rasterized_blob_images(
                                txn.rasterized_blobs.take()
                            );
                            if let Some((rasterizer, info)) = txn.blob_rasterizer.take() {
                                self.resource_cache.set_blob_rasterizer(rasterizer, info);
                            }

                            self.update_document(
                                txn.document_id,
                                txn.resource_updates.take(),
                                txn.interner_updates.take(),
                                txn.frame_ops.take(),
                                txn.notifications.take(),
                                txn.render_frame,
                                txn.invalidate_rendered_frame,
                                &mut frame_counter,
                                &mut profile_counters,
                                has_built_scene,
                            );
                        }
                        self.bookkeep_after_frames();
                    },
                    SceneBuilderResult::FlushComplete(tx) => {
                        tx.send(()).ok();
                    }
                    SceneBuilderResult::ExternalEvent(evt) => {
                        self.notifier.external_event(evt);
                    }
                    SceneBuilderResult::ClearNamespace(id) => {
                        self.resource_cache.clear_namespace(id);
                        self.documents.retain(|doc_id, _doc| doc_id.namespace_id != id);
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

        let _ = self.low_priority_scene_tx.send(SceneBuilderRequest::Stop);
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
                self.low_priority_scene_tx.send(SceneBuilderRequest::Flush(tx)).unwrap();
            }
            ApiMsg::UpdateResources(mut updates) => {
                self.resource_cache.pre_scene_building_update(
                    &mut updates,
                    &mut profile_counters.resources
                );
                self.resource_cache.post_scene_building_update(
                    updates,
                    &mut profile_counters.resources
                );
            }
            ApiMsg::GetGlyphDimensions(instance_key, glyph_indices, tx) => {
                let mut glyph_dimensions = Vec::with_capacity(glyph_indices.len());
                if let Some(base) = self.resource_cache.get_font_instance(instance_key) {
                    let font = FontInstance::from_base(Arc::clone(&base));
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
                assert!(!self.namespace_alloc_by_client);
                sender.send(self.next_namespace_id()).unwrap();
            }
            ApiMsg::CloneApiByClient(namespace_id) => {
                assert!(self.namespace_alloc_by_client);
                debug_assert!(!self.documents.iter().any(|(did, _doc)| did.namespace_id == namespace_id));
            }
            ApiMsg::AddDocument(document_id, initial_size, layer) => {
                let document = Document::new(
                    document_id,
                    initial_size,
                    layer,
                    self.default_device_pixel_ratio,
                );
                let old = self.documents.insert(document_id, document);
                debug_assert!(old.is_none());
            }
            ApiMsg::DeleteDocument(document_id) => {
                self.documents.remove(&document_id);
                self.low_priority_scene_tx.send(
                    SceneBuilderRequest::DeleteDocument(document_id)
                ).unwrap();
            }
            ApiMsg::ExternalEvent(evt) => {
                self.low_priority_scene_tx.send(SceneBuilderRequest::ExternalEvent(evt)).unwrap();
            }
            ApiMsg::ClearNamespace(id) => {
                self.low_priority_scene_tx.send(SceneBuilderRequest::ClearNamespace(id)).unwrap();
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

                self.gpu_cache.clear();

                let pending_update = self.resource_cache.pending_updates();
                let msg = ResultMsg::UpdateResources {
                    updates: pending_update,
                    memory_pressure: true,
                };
                self.result_tx.send(msg).unwrap();
                self.notifier.wake_up();
            }
            ApiMsg::ReportMemory(tx) => {
                self.report_memory(tx);
            }
            ApiMsg::DebugCommand(option) => {
                let msg = match option {
                    DebugCommand::EnableDualSourceBlending(enable) => {
                        // Set in the config used for any future documents
                        // that are created.
                        self.frame_config
                            .dual_source_blending_is_enabled = enable;

                        self.low_priority_scene_tx.send(SceneBuilderRequest::SetFrameBuilderConfig(
                            self.frame_config.clone()
                        )).unwrap();

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
                                root_pipeline_id: doc.scene.root_pipeline_id,
                            };
                            tx.send(captured).unwrap();

                            // notify the active recorder
                            if let Some(ref mut r) = self.recorder {
                                let pipeline_id = doc.scene.root_pipeline_id.unwrap();
                                let epoch =  doc.scene.pipeline_epochs[&pipeline_id];
                                let pipeline = &doc.scene.pipelines[&pipeline_id];
                                let scene_msg = SceneMsg::SetDisplayList {
                                    list_descriptor: pipeline.display_list.descriptor().clone(),
                                    epoch,
                                    pipeline_id,
                                    background: pipeline.background_color,
                                    viewport_size: pipeline.viewport_size,
                                    content_size: pipeline.content_size,
                                    preserve_frame_state: false,
                                };
                                let txn = TransactionMsg::scene_message(scene_msg);
                                r.write_msg(*frame_counter, &ApiMsg::UpdateDocuments(vec![*id], vec![txn]));
                                r.write_payload(*frame_counter, &Payload::construct_data(
                                    epoch,
                                    pipeline_id,
                                    pipeline.display_list.data(),
                                ));
                            }
                        }

                        // Note: we can't pass `LoadCapture` here since it needs to arrive
                        // before the `PublishDocument` messages sent by `load_capture`.
                        return true;
                    }
                    DebugCommand::ClearCaches(mask) => {
                        self.resource_cache.clear(mask);
                        return true;
                    }
                    DebugCommand::SimulateLongSceneBuild(time_ms) => {
                        self.scene_tx.send(SceneBuilderRequest::SimulateLongSceneBuild(time_ms)).unwrap();
                        return true;
                    }
                    DebugCommand::SimulateLongLowPrioritySceneBuild(time_ms) => {
                        self.low_priority_scene_tx.send(
                            SceneBuilderRequest::SimulateLongLowPrioritySceneBuild(time_ms)
                        ).unwrap();
                        return true;
                    }
                    DebugCommand::SetFlags(flags) => {
                        self.resource_cache.set_debug_flags(flags);
                        self.gpu_cache.set_debug_flags(flags);

                        // If we're toggling on the GPU cache debug display, we
                        // need to blow away the cache. This is because we only
                        // send allocation/free notifications to the renderer
                        // thread when the debug display is enabled, and thus
                        // enabling it when the cache is partially populated will
                        // give the renderer an incomplete view of the world.
                        // And since we might as well drop all the debugging state
                        // from the renderer when we disable the debug display,
                        // we just clear the cache on toggle.
                        let changed = self.debug_flags ^ flags;
                        if changed.contains(DebugFlags::GPU_CACHE_DBG) {
                            self.gpu_cache.clear();
                        }
                        self.debug_flags = flags;

                        ResultMsg::DebugCommand(option)
                    }
                    _ => ResultMsg::DebugCommand(option),
                };
                self.result_tx.send(msg).unwrap();
                self.notifier.wake_up();
            }
            ApiMsg::ShutDown => {
                info!("Recycling stats: {:?}", self.recycler);
                return false;
            }
            ApiMsg::UpdateDocuments(document_ids, transaction_msgs) => {
                self.prepare_transactions(
                    document_ids,
                    transaction_msgs,
                    frame_counter,
                    profile_counters,
                );
            }
        }

        true
    }

    fn prepare_for_frames(&mut self) {
        self.resource_cache.prepare_for_frames(SystemTime::now());
        self.gpu_cache.prepare_for_frames();
    }

    fn bookkeep_after_frames(&mut self) {
        self.resource_cache.bookkeep_after_frames();
        self.gpu_cache.bookkeep_after_frames();
    }

    fn requires_frame_build(&mut self) -> bool {
        self.resource_cache.requires_frame_build() || self.gpu_cache.requires_frame_build()
    }

    fn prepare_transactions(
        &mut self,
        document_ids: Vec<DocumentId>,
        mut transaction_msgs: Vec<TransactionMsg>,
        frame_counter: &mut u32,
        profile_counters: &mut BackendProfileCounters,
    ) {
        let mut use_scene_builder = transaction_msgs.iter()
            .any(|transaction_msg| transaction_msg.use_scene_builder_thread);
        let use_high_priority = transaction_msgs.iter()
            .any(|transaction_msg| !transaction_msg.low_priority);

        let mut txns : Vec<Box<Transaction>> = document_ids.iter().zip(transaction_msgs.drain(..))
            .map(|(&document_id, mut transaction_msg)| {
                let mut txn = Box::new(Transaction {
                    document_id,
                    display_list_updates: Vec::new(),
                    removed_pipelines: Vec::new(),
                    epoch_updates: Vec::new(),
                    request_scene_build: None,
                    blob_rasterizer: None,
                    blob_requests: Vec::new(),
                    resource_updates: transaction_msg.resource_updates,
                    frame_ops: transaction_msg.frame_ops,
                    rasterized_blobs: Vec::new(),
                    notifications: transaction_msg.notifications,
                    set_root_pipeline: None,
                    render_frame: transaction_msg.generate_frame,
                    invalidate_rendered_frame: transaction_msg.invalidate_rendered_frame,
                });

                self.resource_cache.pre_scene_building_update(
                    &mut txn.resource_updates,
                    &mut profile_counters.resources,
                );

                for scene_msg in transaction_msg.scene_ops.drain(..) {
                    let _timer = profile_counters.total_time.timer();
                    self.process_scene_msg(
                        document_id,
                        scene_msg,
                        *frame_counter,
                        &mut txn,
                        &mut profile_counters.ipc,
                    )
                }

                let blobs_to_rasterize = get_blob_image_updates(&txn.resource_updates);
                if !blobs_to_rasterize.is_empty() {
                    let (blob_rasterizer, blob_requests) = self.resource_cache
                        .create_blob_scene_builder_requests(&blobs_to_rasterize);

                    txn.blob_requests = blob_requests;
                    txn.blob_rasterizer = blob_rasterizer;
                }
                txn
            }).collect();

        use_scene_builder = use_scene_builder || txns.iter().any(|txn| {
            !txn.can_skip_scene_builder() || txn.blob_rasterizer.is_some()
        });

        if use_scene_builder {
            for txn in txns.iter_mut() {
                let doc = self.documents.get_mut(&txn.document_id).unwrap();

                if txn.should_build_scene() {
                    txn.request_scene_build = Some(SceneRequest {
                        view: doc.view.clone(),
                        font_instances: self.resource_cache.get_font_instances(),
                        output_pipelines: doc.output_pipelines.clone(),
                    });
                }
            }
        } else {
            self.prepare_for_frames();
            self.maybe_force_nop_documents(
                frame_counter,
                profile_counters,
                |document_id| txns.iter().any(|txn| txn.document_id == document_id));

            for mut txn in txns {
                self.update_document(
                    txn.document_id,
                    txn.resource_updates.take(),
                    None,
                    txn.frame_ops.take(),
                    txn.notifications.take(),
                    txn.render_frame,
                    txn.invalidate_rendered_frame,
                    frame_counter,
                    profile_counters,
                    false
                );
            }

            self.bookkeep_after_frames();
            return;
        }

        let tx = if use_high_priority {
            &self.scene_tx
        } else {
            &self.low_priority_scene_tx
        };

        tx.send(SceneBuilderRequest::Transactions(txns)).unwrap();
    }

    /// In certain cases, resources shared by multiple documents have to run
    /// maintenance operations, like cleaning up unused cache items. In those
    /// cases, we are forced to build frames for all documents, however we
    /// may not have a transaction ready for every document - this method
    /// calls update_document with the details of a fake, nop transaction just
    /// to force a frame build.
    fn maybe_force_nop_documents<F>(&mut self,
                                    frame_counter: &mut u32,
                                    profile_counters: &mut BackendProfileCounters,
                                    document_already_present: F) where
        F: Fn(DocumentId) -> bool {
        if self.requires_frame_build() {
            let nop_documents : Vec<DocumentId> = self.documents.keys()
                .cloned()
                .filter(|key| !document_already_present(*key))
                .collect();
            for &document_id in &nop_documents {
                self.update_document(
                    document_id,
                    Vec::default(),
                    None,
                    Vec::default(),
                    Vec::default(),
                    false,
                    false,
                    frame_counter,
                    profile_counters,
                    false);
            }
        }
    }

    fn update_document(
        &mut self,
        document_id: DocumentId,
        resource_updates: Vec<ResourceUpdate>,
        interner_updates: Option<InternerUpdates>,
        mut frame_ops: Vec<FrameMsg>,
        mut notifications: Vec<NotificationRequest>,
        mut render_frame: bool,
        invalidate_rendered_frame: bool,
        frame_counter: &mut u32,
        profile_counters: &mut BackendProfileCounters,
        has_built_scene: bool,
    ) {
        let requested_frame = render_frame;

        // If we have a sampler, get more frame ops from it and add them
        // to the transaction. This is a hook to allow the WR user code to
        // fiddle with things after a potentially long scene build, but just
        // before rendering. This is useful for rendering with the latest
        // async transforms.
        if requested_frame || has_built_scene {
            if let Some(ref sampler) = self.sampler {
                frame_ops.append(&mut sampler.sample(document_id));
            }
        }

        let requires_frame_build = self.requires_frame_build();
        let doc = self.documents.get_mut(&document_id).unwrap();
        doc.has_built_scene |= has_built_scene;

        // If there are any additions or removals of clip modes
        // during the scene build, apply them to the data store now.
        if let Some(updates) = interner_updates {
            doc.data_stores.apply_updates(updates, profile_counters);
        }

        // TODO: this scroll variable doesn't necessarily mean we scrolled. It is only used
        // for something wrench specific and we should remove it.
        let mut scroll = false;
        for frame_msg in frame_ops {
            let _timer = profile_counters.total_time.timer();
            let op = doc.process_frame_msg(frame_msg);
            scroll |= op.scroll;
        }

        for update in &resource_updates {
            if let ResourceUpdate::UpdateImage(..) = update {
                doc.frame_is_valid = false;
            }
        }

        self.resource_cache.post_scene_building_update(
            resource_updates,
            &mut profile_counters.resources,
        );

        if doc.dynamic_properties.flush_pending_updates() {
            doc.frame_is_valid = false;
            doc.hit_tester_is_valid = false;
        }

        if !doc.can_render() {
            // TODO: this happens if we are building the first scene asynchronously and
            // scroll at the same time. we should keep track of the fact that we skipped
            // composition here and do it as soon as we receive the scene.
            render_frame = false;
        }

        // Avoid re-building the frame if the current built frame is still valid.
        // However, if the resource_cache requires a frame build, _always_ do that, unless
        // doc.can_render() is false, as in that case a frame build can't happen anyway.
        // We want to ensure we do this because even if the doc doesn't have pixels it
        // can still try to access stale texture cache items.
        let build_frame = (render_frame && !doc.frame_is_valid && doc.has_pixels()) ||
            (requires_frame_build && doc.can_render());

        // Request composite is true when we want to composite frame even when
        // there is no frame update. This happens when video frame is updated under
        // external image with NativeTexture or when platform requested to composite frame.
        if invalidate_rendered_frame {
            doc.rendered_frame_is_valid = false;
        }

        let mut frame_build_time = None;
        if build_frame {
            profile_scope!("generate frame");

            *frame_counter += 1;
            doc.rendered_frame_is_valid = false;

            // borrow ck hack for profile_counters
            let (pending_update, rendered_document) = {
                let _timer = profile_counters.total_time.timer();
                let frame_build_start_time = precise_time_ns();

                let rendered_document = doc.build_frame(
                    &mut self.resource_cache,
                    &mut self.gpu_cache,
                    &mut profile_counters.resources,
                    self.debug_flags,
                );

                debug!("generated frame for document {:?} with {} passes",
                    document_id, rendered_document.frame.passes.len());

                let msg = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
                self.result_tx.send(msg).unwrap();

                frame_build_time = Some(precise_time_ns() - frame_build_start_time);

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
        } else if requested_frame {
            // WR-internal optimization to avoid doing a bunch of render work if
            // there's no pixels. We still want to pretend to render and request
            // a render to make sure that the callbacks (particularly the
            // new_frame_ready callback below) has the right flags.
            let msg = ResultMsg::PublishPipelineInfo(doc.updated_pipeline_info());
            self.result_tx.send(msg).unwrap();
        }

        drain_filter(
            &mut notifications,
            |n| { n.when() == Checkpoint::FrameBuilt },
            |n| { n.notify(); },
        );

        if !notifications.is_empty() {
            self.result_tx.send(ResultMsg::AppendNotificationRequests(notifications)).unwrap();
        }

        // Always forward the transaction to the renderer if a frame was requested,
        // otherwise gecko can get into a state where it waits (forever) for the
        // transaction to complete before sending new work.
        if requested_frame {
            // If rendered frame is already valid, there is no need to render frame.
            if doc.rendered_frame_is_valid {
                render_frame = false;
            } else if render_frame {
                doc.rendered_frame_is_valid = true;
            }
            self.notifier.new_frame_ready(document_id, scroll, render_frame, frame_build_time);
        }

        if !doc.hit_tester_is_valid {
            doc.rebuild_hit_tester();
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
                    display_item @ DisplayItem::PushStackingContext(..) => {
                        let mut subtraversal = item.sub_iter();
                        let mut child_node =
                            debug_server::TreeNode::new(&display_item.debug_name().to_string());
                        self.traverse_items(&mut subtraversal, &mut child_node);
                        node.add_child(child_node);
                        Some(subtraversal)
                    }
                    DisplayItem::PopStackingContext => {
                        return;
                    }
                    display_item => {
                        node.add_item(&display_item.debug_name().to_string());
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
        use crate::print_tree::PrintableTree;

        let mut debug_root = debug_server::ClipScrollTreeList::new();

        for (_, doc) in &self.documents {
            let debug_node = debug_server::TreeNode::new("document clip-scroll tree");
            let mut builder = debug_server::TreeNodeBuilder::new(debug_node);

            doc.clip_scroll_tree.print_with(&mut builder);

            debug_root.add(builder.build());
        }

        serde_json::to_string(&debug_root).unwrap()
    }

    fn report_memory(&mut self, tx: MsgSender<MemoryReport>) {
        let mut report = MemoryReport::default();
        let ops = self.size_of_ops.as_mut().unwrap();
        let op = ops.size_of_op;
        report.gpu_cache_metadata = self.gpu_cache.size_of(ops);
        for (_id, doc) in &self.documents {
            if let Some(ref fb) = doc.frame_builder {
                report.clip_stores += fb.clip_store.size_of(ops);
            }
            report.hit_testers += doc.hit_tester.size_of(ops);

            doc.data_stores.report_memory(ops, &mut report)
        }

        report += self.resource_cache.report_memory(op);

        // Send a message to report memory on the scene-builder thread, which
        // will add its report to this one and send the result back to the original
        // thread waiting on the request.
        self.scene_tx.send(SceneBuilderRequest::ReportMemory(report, tx)).unwrap();
    }
}

fn get_blob_image_updates(updates: &[ResourceUpdate]) -> Vec<BlobImageKey> {
    let mut requests = Vec::new();
    for update in updates {
        match *update {
            ResourceUpdate::AddBlobImage(ref img) => {
                requests.push(img.key);
            }
            ResourceUpdate::UpdateBlobImage(ref img) => {
                requests.push(img.key);
            }
            _ => {}
        }
    }

    requests
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
        use crate::capture::CaptureConfig;
        use crate::render_task::dump_render_tasks_as_svg;

        debug!("capture: saving {:?}", root);
        if !root.is_dir() {
            if let Err(e) = fs::create_dir_all(&root) {
                panic!("Unable to create capture dir: {:?}", e);
            }
        }
        let config = CaptureConfig::new(root, bits);

        if config.bits.contains(CaptureBits::FRAME) {
            self.prepare_for_frames();
        }

        for (&id, doc) in &mut self.documents {
            debug!("\tdocument {:?}", id);
            if config.bits.contains(CaptureBits::SCENE) {
                let file_name = format!("scene-{}-{}", id.namespace_id.0, id.id);
                config.serialize(&doc.scene, file_name);
            }
            if config.bits.contains(CaptureBits::FRAME) {
                let rendered_document = doc.build_frame(
                    &mut self.resource_cache,
                    &mut self.gpu_cache,
                    &mut profile_counters.resources,
                    self.debug_flags,
                );
                // After we rendered the frames, there are pending updates to both
                // GPU cache and resources. Instead of serializing them, we are going to make sure
                // they are applied on the `Renderer` side.
                let msg_update_gpu_cache = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
                self.result_tx.send(msg_update_gpu_cache).unwrap();
                //TODO: write down doc's pipeline info?
                // it has `pipeline_epoch_map`,
                // which may capture necessary details for some cases.
                let file_name = format!("frame-{}-{}", id.namespace_id.0, id.id);
                config.serialize(&rendered_document.frame, file_name);
                let file_name = format!("clip-scroll-{}-{}", id.namespace_id.0, id.id);
                config.serialize_tree(&doc.clip_scroll_tree, file_name);
                let file_name = format!("builder-{}-{}", id.namespace_id.0, id.id);
                config.serialize(doc.frame_builder.as_ref().unwrap(), file_name);
                let file_name = format!("render-tasks-{}-{}.svg", id.namespace_id.0, id.id);
                let mut svg_file = fs::File::create(&config.file_path(file_name, "svg"))
                    .expect("Failed to open the SVG file.");
                dump_render_tasks_as_svg(
                    &rendered_document.frame.render_tasks,
                    &rendered_document.frame.passes,
                    &mut svg_file
                ).unwrap();
            }

            let data_stores_name = format!("data-stores-{}-{}", id.namespace_id.0, id.id);
            config.serialize(&doc.data_stores, data_stores_name);
        }

        if config.bits.contains(CaptureBits::FRAME) {
            // TODO: there is no guarantee that we won't hit this case, but we want to
            // report it here if we do. If we don't, it will simply crash in
            // Renderer::render_impl and give us less information about the source.
            assert!(!self.requires_frame_build(), "Caches were cleared during a capture.");
            self.bookkeep_after_frames();
        }

        debug!("\tscene builder");
        self.scene_tx.send(SceneBuilderRequest::SaveScene(config.clone())).unwrap();

        debug!("\tresource cache");
        let (resources, deferred) = self.resource_cache.save_capture(&config.root);

        info!("\tbackend");
        let backend = PlainRenderBackend {
            default_device_pixel_ratio: self.default_device_pixel_ratio,
            frame_config: self.frame_config.clone(),
            documents: self.documents
                .iter()
                .map(|(id, doc)| (*id, doc.view.clone()))
                .collect(),
            resources,
        };

        config.serialize(&backend, "backend");

        if config.bits.contains(CaptureBits::FRAME) {
            let msg_update_resources = ResultMsg::UpdateResources {
                updates: self.resource_cache.pending_updates(),
                memory_pressure: false,
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
        use crate::capture::CaptureConfig;

        debug!("capture: loading {:?}", root);
        let backend = CaptureConfig::deserialize::<PlainRenderBackend, _>(root, "backend")
            .expect("Unable to open backend.ron");
        let caches_maybe = CaptureConfig::deserialize::<PlainCacheOwn, _>(root, "resource_cache");

        // Note: it would be great to have `RenderBackend` to be split
        // rather explicitly on what's used before and after scene building
        // so that, for example, we never miss anything in the code below:

        let plain_externals = self.resource_cache.load_capture(
            backend.resources,
            caches_maybe,
            root,
        );
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

        let mut scenes_to_build = Vec::new();

        for (id, view) in backend.documents {
            debug!("\tdocument {:?}", id);
            let scene_name = format!("scene-{}-{}", id.namespace_id.0, id.id);
            let scene = CaptureConfig::deserialize::<Scene, _>(root, &scene_name)
                .expect(&format!("Unable to open {}.ron", scene_name));

            let interners_name = format!("interners-{}-{}", id.namespace_id.0, id.id);
            let interners = CaptureConfig::deserialize::<Interners, _>(root, &interners_name)
                .expect(&format!("Unable to open {}.ron", interners_name));

            let data_stores_name = format!("data-stores-{}-{}", id.namespace_id.0, id.id);
            let data_stores = CaptureConfig::deserialize::<DataStores, _>(root, &data_stores_name)
                .expect(&format!("Unable to open {}.ron", data_stores_name));

            let doc = Document {
                id: id,
                scene: scene.clone(),
                removed_pipelines: Vec::new(),
                view: view.clone(),
                clip_scroll_tree: ClipScrollTree::new(),
                stamp: FrameStamp::first(id),
                frame_builder: Some(FrameBuilder::empty()),
                output_pipelines: FastHashSet::default(),
                dynamic_properties: SceneProperties::new(),
                hit_tester: None,
                frame_is_valid: false,
                hit_tester_is_valid: false,
                rendered_frame_is_valid: false,
                has_built_scene: false,
                data_stores,
                scratch: PrimitiveScratchBuffer::new(),
                render_task_counters: RenderTaskGraphCounters::new(),
            };

            let frame_name = format!("frame-{}-{}", id.namespace_id.0, id.id);
            let frame = CaptureConfig::deserialize::<Frame, _>(root, frame_name);
            let build_frame = match frame {
                Some(frame) => {
                    info!("\tloaded a built frame with {} passes", frame.passes.len());

                    let msg_update = ResultMsg::UpdateGpuCache(self.gpu_cache.extract_updates());
                    self.result_tx.send(msg_update).unwrap();

                    let msg_publish = ResultMsg::PublishDocument(
                        id,
                        RenderedDocument { frame, is_new_scene: true },
                        self.resource_cache.pending_updates(),
                        profile_counters.clone(),
                    );
                    self.result_tx.send(msg_publish).unwrap();
                    profile_counters.reset();

                    self.notifier.new_frame_ready(id, false, true, None);

                    // We deserialized the state of the frame so we don't want to build
                    // it (but we do want to update the scene builder's state)
                    false
                }
                None => true,
            };

            scenes_to_build.push(LoadScene {
                document_id: id,
                scene: doc.scene.clone(),
                view: view.clone(),
                config: self.frame_config.clone(),
                output_pipelines: doc.output_pipelines.clone(),
                font_instances: self.resource_cache.get_font_instances(),
                build_frame,
                interners,
            });

            self.documents.insert(id, doc);
        }

        if !scenes_to_build.is_empty() {
            self.low_priority_scene_tx.send(
                SceneBuilderRequest::LoadScenes(scenes_to_build)
            ).unwrap();
        }
    }
}

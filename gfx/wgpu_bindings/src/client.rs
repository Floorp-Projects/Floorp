/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    cow_label, error::HasErrorBufferType, wgpu_string, AdapterInformation, ByteBuf,
    CommandEncoderAction, DeviceAction, DropAction, ImageDataLayout, ImplicitLayout,
    QueueWriteAction, RawString, TextureAction,
};

use crate::SwapChainId;

use wgc::{id, identity::IdentityManager};
use wgt::{Backend, TextureFormat};

pub use wgc::command::{compute_ffi::*, render_ffi::*};

use parking_lot::Mutex;

use nsstring::{nsACString, nsString};

use std::{borrow::Cow, ptr};

// we can't call `from_raw_parts` unconditionally because the caller
// may not even have a valid pointer (e.g. NULL) if the `length` is zero.
fn make_slice<'a, T>(pointer: *const T, length: usize) -> &'a [T] {
    if length == 0 {
        &[]
    } else {
        unsafe { std::slice::from_raw_parts(pointer, length) }
    }
}

fn make_byte_buf<T: serde::Serialize>(data: &T) -> ByteBuf {
    let vec = bincode::serialize(data).unwrap();
    ByteBuf::from_vec(vec)
}

#[repr(C)]
pub struct ProgrammableStageDescriptor {
    module: id::ShaderModuleId,
    entry_point: RawString,
}

impl ProgrammableStageDescriptor {
    fn to_wgpu(&self) -> wgc::pipeline::ProgrammableStageDescriptor {
        wgc::pipeline::ProgrammableStageDescriptor {
            module: self.module,
            entry_point: cow_label(&self.entry_point).unwrap(),
        }
    }
}

#[repr(C)]
pub struct ComputePipelineDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: Option<id::PipelineLayoutId>,
    stage: ProgrammableStageDescriptor,
}

#[repr(C)]
pub struct VertexBufferLayout {
    array_stride: wgt::BufferAddress,
    step_mode: wgt::VertexStepMode,
    attributes: *const wgt::VertexAttribute,
    attributes_length: usize,
}

#[repr(C)]
pub struct VertexState {
    stage: ProgrammableStageDescriptor,
    buffers: *const VertexBufferLayout,
    buffers_length: usize,
}

impl VertexState {
    fn to_wgpu(&self) -> wgc::pipeline::VertexState {
        let buffer_layouts = make_slice(self.buffers, self.buffers_length)
            .iter()
            .map(|vb| wgc::pipeline::VertexBufferLayout {
                array_stride: vb.array_stride,
                step_mode: vb.step_mode,
                attributes: Cow::Borrowed(make_slice(vb.attributes, vb.attributes_length)),
            })
            .collect();
        wgc::pipeline::VertexState {
            stage: self.stage.to_wgpu(),
            buffers: Cow::Owned(buffer_layouts),
        }
    }
}

#[repr(C)]
pub struct ColorTargetState<'a> {
    format: wgt::TextureFormat,
    blend: Option<&'a wgt::BlendState>,
    write_mask: wgt::ColorWrites,
}

#[repr(C)]
pub struct FragmentState<'a> {
    stage: ProgrammableStageDescriptor,
    targets: *const ColorTargetState<'a>,
    targets_length: usize,
}

impl FragmentState<'_> {
    fn to_wgpu(&self) -> wgc::pipeline::FragmentState {
        let color_targets = make_slice(self.targets, self.targets_length)
            .iter()
            .map(|ct| {
                Some(wgt::ColorTargetState {
                    format: ct.format,
                    blend: ct.blend.cloned(),
                    write_mask: ct.write_mask,
                })
            })
            .collect();
        wgc::pipeline::FragmentState {
            stage: self.stage.to_wgpu(),
            targets: Cow::Owned(color_targets),
        }
    }
}

#[repr(C)]
pub struct PrimitiveState<'a> {
    topology: wgt::PrimitiveTopology,
    strip_index_format: Option<&'a wgt::IndexFormat>,
    front_face: wgt::FrontFace,
    cull_mode: Option<&'a wgt::Face>,
    polygon_mode: wgt::PolygonMode,
    unclipped_depth: bool,
}

impl PrimitiveState<'_> {
    fn to_wgpu(&self) -> wgt::PrimitiveState {
        wgt::PrimitiveState {
            topology: self.topology,
            strip_index_format: self.strip_index_format.cloned(),
            front_face: self.front_face.clone(),
            cull_mode: self.cull_mode.cloned(),
            polygon_mode: self.polygon_mode,
            unclipped_depth: self.unclipped_depth,
            conservative: false,
        }
    }
}

#[repr(C)]
pub struct RenderPipelineDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: Option<id::PipelineLayoutId>,
    vertex: &'a VertexState,
    primitive: PrimitiveState<'a>,
    fragment: Option<&'a FragmentState<'a>>,
    depth_stencil: Option<&'a wgt::DepthStencilState>,
    multisample: wgt::MultisampleState,
}

#[repr(C)]
pub enum RawTextureSampleType {
    Float,
    UnfilterableFloat,
    Uint,
    Sint,
    Depth,
}

#[repr(C)]
pub enum RawBindingType {
    UniformBuffer,
    StorageBuffer,
    ReadonlyStorageBuffer,
    Sampler,
    SampledTexture,
    ReadonlyStorageTexture,
    WriteonlyStorageTexture,
}

#[repr(C)]
pub struct BindGroupLayoutEntry<'a> {
    binding: u32,
    visibility: wgt::ShaderStages,
    ty: RawBindingType,
    has_dynamic_offset: bool,
    min_binding_size: Option<wgt::BufferSize>,
    view_dimension: Option<&'a wgt::TextureViewDimension>,
    texture_sample_type: Option<&'a RawTextureSampleType>,
    multisampled: bool,
    storage_texture_format: Option<&'a wgt::TextureFormat>,
    sampler_filter: bool,
    sampler_compare: bool,
}

#[repr(C)]
pub struct BindGroupLayoutDescriptor<'a> {
    label: Option<&'a nsACString>,
    entries: *const BindGroupLayoutEntry<'a>,
    entries_length: usize,
}

#[repr(C)]
#[derive(Debug)]
pub struct BindGroupEntry {
    binding: u32,
    buffer: Option<id::BufferId>,
    offset: wgt::BufferAddress,
    size: Option<wgt::BufferSize>,
    sampler: Option<id::SamplerId>,
    texture_view: Option<id::TextureViewId>,
}

#[repr(C)]
pub struct BindGroupDescriptor<'a> {
    label: Option<&'a nsACString>,
    layout: id::BindGroupLayoutId,
    entries: *const BindGroupEntry,
    entries_length: usize,
}

#[repr(C)]
pub struct PipelineLayoutDescriptor<'a> {
    label: Option<&'a nsACString>,
    bind_group_layouts: *const id::BindGroupLayoutId,
    bind_group_layouts_length: usize,
}

#[repr(C)]
pub struct SamplerDescriptor<'a> {
    label: Option<&'a nsACString>,
    address_modes: [wgt::AddressMode; 3],
    mag_filter: wgt::FilterMode,
    min_filter: wgt::FilterMode,
    mipmap_filter: wgt::FilterMode,
    lod_min_clamp: f32,
    lod_max_clamp: f32,
    compare: Option<&'a wgt::CompareFunction>,
    anisotropy_clamp: Option<&'a u16>,
}

#[repr(C)]
pub struct TextureViewDescriptor<'a> {
    label: Option<&'a nsACString>,
    format: Option<&'a wgt::TextureFormat>,
    dimension: Option<&'a wgt::TextureViewDimension>,
    aspect: wgt::TextureAspect,
    base_mip_level: u32,
    mip_level_count: Option<&'a u32>,
    base_array_layer: u32,
    array_layer_count: Option<&'a u32>,
}

#[repr(C)]
pub struct RenderBundleEncoderDescriptor<'a> {
    label: Option<&'a nsACString>,
    color_formats: *const wgt::TextureFormat,
    color_formats_length: usize,
    depth_stencil_format: Option<&'a wgt::TextureFormat>,
    depth_read_only: bool,
    stencil_read_only: bool,
    sample_count: u32,
}

#[derive(Debug, Default)]
struct IdentityHub {
    adapters: IdentityManager,
    devices: IdentityManager,
    buffers: IdentityManager,
    command_buffers: IdentityManager,
    render_bundles: IdentityManager,
    bind_group_layouts: IdentityManager,
    pipeline_layouts: IdentityManager,
    bind_groups: IdentityManager,
    shader_modules: IdentityManager,
    compute_pipelines: IdentityManager,
    render_pipelines: IdentityManager,
    textures: IdentityManager,
    texture_views: IdentityManager,
    samplers: IdentityManager,
}

impl ImplicitLayout<'_> {
    fn new(identities: &mut IdentityHub, backend: Backend) -> Self {
        ImplicitLayout {
            pipeline: identities.pipeline_layouts.alloc(backend),
            bind_groups: Cow::Owned(
                (0..8) // hal::MAX_BIND_GROUPS
                    .map(|_| identities.bind_group_layouts.alloc(backend))
                    .collect(),
            ),
        }
    }
}

#[derive(Debug, Default)]
struct Identities {
    vulkan: IdentityHub,
    #[cfg(any(target_os = "ios", target_os = "macos"))]
    metal: IdentityHub,
    #[cfg(windows)]
    dx12: IdentityHub,
}

impl Identities {
    fn select(&mut self, backend: Backend) -> &mut IdentityHub {
        match backend {
            Backend::Vulkan => &mut self.vulkan,
            #[cfg(any(target_os = "ios", target_os = "macos"))]
            Backend::Metal => &mut self.metal,
            #[cfg(windows)]
            Backend::Dx12 => &mut self.dx12,
            _ => panic!("Unexpected backend: {:?}", backend),
        }
    }
}

#[derive(Debug)]
pub struct Client {
    identities: Mutex<Identities>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_drop_action(client: &mut Client, byte_buf: &ByteBuf) {
    let mut cursor = std::io::Cursor::new(byte_buf.as_slice());
    let mut identities = client.identities.lock();
    while let Ok(action) = bincode::deserialize_from(&mut cursor) {
        match action {
            DropAction::Adapter(id) => identities.select(id.backend()).adapters.free(id),
            DropAction::Device(id) => identities.select(id.backend()).devices.free(id),
            DropAction::ShaderModule(id) => identities.select(id.backend()).shader_modules.free(id),
            DropAction::PipelineLayout(id) => {
                identities.select(id.backend()).pipeline_layouts.free(id)
            }
            DropAction::BindGroupLayout(id) => {
                identities.select(id.backend()).bind_group_layouts.free(id)
            }
            DropAction::BindGroup(id) => identities.select(id.backend()).bind_groups.free(id),
            DropAction::CommandBuffer(id) => {
                identities.select(id.backend()).command_buffers.free(id)
            }
            DropAction::RenderBundle(id) => identities.select(id.backend()).render_bundles.free(id),
            DropAction::RenderPipeline(id) => {
                identities.select(id.backend()).render_pipelines.free(id)
            }
            DropAction::ComputePipeline(id) => {
                identities.select(id.backend()).compute_pipelines.free(id)
            }
            DropAction::Buffer(id) => identities.select(id.backend()).buffers.free(id),
            DropAction::Texture(id) => identities.select(id.backend()).textures.free(id),
            DropAction::TextureView(id) => identities.select(id.backend()).texture_views.free(id),
            DropAction::Sampler(id) => identities.select(id.backend()).samplers.free(id),
        }
    }
}

#[no_mangle]
pub extern "C" fn wgpu_client_kill_device_id(client: &Client, id: id::DeviceId) {
    client
        .identities
        .lock()
        .select(id.backend())
        .devices
        .free(id)
}

#[repr(C)]
#[derive(Debug)]
pub struct Infrastructure {
    pub client: *mut Client,
    pub error: *const u8,
}

#[no_mangle]
pub extern "C" fn wgpu_client_new() -> Infrastructure {
    log::info!("Initializing WGPU client");
    let client = Box::new(Client {
        identities: Mutex::new(Identities::default()),
    });
    Infrastructure {
        client: Box::into_raw(client),
        error: ptr::null(),
    }
}

/// # Safety
///
/// This function is unsafe because improper use may lead to memory
/// problems. For example, a double-free may occur if the function is called
/// twice on the same raw pointer.
#[no_mangle]
pub unsafe extern "C" fn wgpu_client_delete(client: *mut Client) {
    log::info!("Terminating WGPU client");
    let _client = Box::from_raw(client);
}

/// # Safety
///
/// This function is unsafe as there is no guarantee that the given pointer is
/// valid for `id_length` elements.
#[no_mangle]
pub unsafe extern "C" fn wgpu_client_make_adapter_ids(
    client: &Client,
    ids: *mut id::AdapterId,
    id_length: usize,
) -> usize {
    let mut identities = client.identities.lock();
    assert_ne!(id_length, 0);
    let mut ids = std::slice::from_raw_parts_mut(ids, id_length).iter_mut();

    *ids.next().unwrap() = identities.vulkan.adapters.alloc(Backend::Vulkan);

    #[cfg(any(target_os = "ios", target_os = "macos"))]
    {
        *ids.next().unwrap() = identities.metal.adapters.alloc(Backend::Metal);
    }
    #[cfg(windows)]
    {
        *ids.next().unwrap() = identities.dx12.adapters.alloc(Backend::Dx12);
    }

    id_length - ids.len()
}

#[no_mangle]
pub extern "C" fn wgpu_client_fill_default_limits(limits: &mut wgt::Limits) {
    *limits = wgt::Limits::default();
}

#[no_mangle]
pub extern "C" fn wgpu_client_adapter_extract_info(
    byte_buf: &ByteBuf,
    info: &mut AdapterInformation<nsString>,
) {
    let AdapterInformation {
        backend,
        device_type,
        device,
        driver_info,
        driver,
        features,
        id,
        limits,
        name,
        vendor,
    } = bincode::deserialize::<AdapterInformation<String>>(unsafe { byte_buf.as_slice() }).unwrap();

    let nss = |s: &str| {
        let mut ns_string = nsString::new();
        ns_string.assign_str(s);
        ns_string
    };
    *info = AdapterInformation {
        backend,
        device_type,
        device,
        driver_info: nss(&driver_info),
        driver: nss(&driver),
        features,
        id,
        limits,
        name: nss(&name),
        vendor,
    };
}

#[no_mangle]
pub extern "C" fn wgpu_client_serialize_device_descriptor(
    desc: &wgt::DeviceDescriptor<Option<&nsACString>>,
    bb: &mut ByteBuf,
) {
    let label = wgpu_string(desc.label);
    *bb = make_byte_buf(&desc.map_label(|_| label));
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_device_id(
    client: &Client,
    adapter_id: id::AdapterId,
) -> id::DeviceId {
    let backend = adapter_id.backend();
    client
        .identities
        .lock()
        .select(backend)
        .devices
        .alloc(backend)
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_buffer_id(
    client: &Client,
    device_id: id::DeviceId,
) -> id::BufferId {
    let backend = device_id.backend();
    client
        .identities
        .lock()
        .select(backend)
        .buffers
        .alloc(backend)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture(
    client: &Client,
    device_id: id::DeviceId,
    desc: &wgt::TextureDescriptor<Option<&nsACString>, crate::FfiSlice<TextureFormat>>,
    swap_chain_id: Option<&SwapChainId>,
    bb: &mut ByteBuf,
) -> id::TextureId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .textures
        .alloc(backend);

    let view_formats = unsafe { desc.view_formats.as_slice() }.to_vec();

    let action = DeviceAction::CreateTexture(
        id,
        desc.map_label_and_view_formats(|_| label, |_| view_formats),
        swap_chain_id.copied(),
    );
    *bb = make_byte_buf(&action);

    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_texture_view(
    client: &Client,
    device_id: id::DeviceId,
    desc: &TextureViewDescriptor,
    bb: &mut ByteBuf,
) -> id::TextureViewId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .texture_views
        .alloc(backend);

    let wgpu_desc = wgc::resource::TextureViewDescriptor {
        label,
        format: desc.format.cloned(),
        dimension: desc.dimension.cloned(),
        range: wgt::ImageSubresourceRange {
            aspect: desc.aspect,
            base_mip_level: desc.base_mip_level,
            mip_level_count: desc.mip_level_count.map(|ptr| *ptr),
            base_array_layer: desc.base_array_layer,
            array_layer_count: desc.array_layer_count.map(|ptr| *ptr),
        },
    };

    let action = TextureAction::CreateView(id, wgpu_desc);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_sampler(
    client: &Client,
    device_id: id::DeviceId,
    desc: &SamplerDescriptor,
    bb: &mut ByteBuf,
) -> id::SamplerId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .samplers
        .alloc(backend);

    let wgpu_desc = wgc::resource::SamplerDescriptor {
        label,
        address_modes: desc.address_modes,
        mag_filter: desc.mag_filter,
        min_filter: desc.min_filter,
        mipmap_filter: desc.mipmap_filter,
        lod_min_clamp: desc.lod_min_clamp,
        lod_max_clamp: desc.lod_max_clamp,
        compare: desc.compare.cloned(),
        anisotropy_clamp: *desc.anisotropy_clamp.unwrap_or(&1),
        border_color: None,
    };
    let action = DeviceAction::CreateSampler(id, wgpu_desc);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_encoder_id(
    client: &Client,
    device_id: id::DeviceId,
) -> id::CommandEncoderId {
    let backend = device_id.backend();
    client
        .identities
        .lock()
        .select(backend)
        .command_buffers
        .alloc(backend)
}

#[no_mangle]
pub extern "C" fn wgpu_client_create_command_encoder(
    client: &Client,
    device_id: id::DeviceId,
    desc: &wgt::CommandEncoderDescriptor<Option<&nsACString>>,
    bb: &mut ByteBuf,
) -> id::CommandEncoderId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .command_buffers
        .alloc(backend);

    let action = DeviceAction::CreateCommandEncoder(id, desc.map_label(|_| label));
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_device_create_render_bundle_encoder(
    device_id: id::DeviceId,
    desc: &RenderBundleEncoderDescriptor,
    bb: &mut ByteBuf,
) -> *mut wgc::command::RenderBundleEncoder {
    let label = wgpu_string(desc.label);

    let color_formats: Vec<_> = make_slice(desc.color_formats, desc.color_formats_length)
        .iter()
        .map(|format| Some(format.clone()))
        .collect();
    let descriptor = wgc::command::RenderBundleEncoderDescriptor {
        label,
        color_formats: Cow::Owned(color_formats),
        depth_stencil: desc
            .depth_stencil_format
            .map(|&format| wgt::RenderBundleDepthStencil {
                format,
                depth_read_only: desc.depth_read_only,
                stencil_read_only: desc.stencil_read_only,
            }),
        sample_count: desc.sample_count,
        multiview: None,
    };
    match wgc::command::RenderBundleEncoder::new(&descriptor, device_id, None) {
        Ok(encoder) => Box::into_raw(Box::new(encoder)),
        Err(e) => {
            let message = format!("Error in `Device::create_render_bundle_encoder`: {}", e);
            let action = DeviceAction::Error {
                message,
                r#type: e.error_type(),
            };
            *bb = make_byte_buf(&action);
            ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_bundle_encoder_destroy(
    pass: *mut wgc::command::RenderBundleEncoder,
) {
    // The RB encoder is just a boxed Rust struct, it doesn't have any API primitives
    // associated with it right now, but in the future it will.
    let _ = Box::from_raw(pass);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_bundle(
    client: &Client,
    encoder: *mut wgc::command::RenderBundleEncoder,
    device_id: id::DeviceId,
    desc: &wgt::RenderBundleDescriptor<Option<&nsACString>>,
    bb: &mut ByteBuf,
) -> id::RenderBundleId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .render_bundles
        .alloc(backend);

    let action =
        DeviceAction::CreateRenderBundle(id, *Box::from_raw(encoder), desc.map_label(|_| label));
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_bundle_error(
    client: &Client,
    device_id: id::DeviceId,
    label: Option<&nsACString>,
    bb: &mut ByteBuf,
) -> id::RenderBundleId {
    let label = wgpu_string(label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .render_bundles
        .alloc(backend);

    let action = DeviceAction::CreateRenderBundleError(id, label);
    *bb = make_byte_buf(&action);
    id
}

#[repr(C)]
pub struct ComputePassDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub timestamp_writes: Option<&'a ComputePassTimestampWrites<'a>>,
}

#[repr(C)]
pub struct ComputePassTimestampWrites<'a> {
    pub query_set: id::QuerySetId,
    pub beginning_of_pass_write_index: Option<&'a u32>,
    pub end_of_pass_write_index: Option<&'a u32>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_begin_compute_pass(
    encoder_id: id::CommandEncoderId,
    desc: &ComputePassDescriptor,
) -> *mut wgc::command::ComputePass {
    let &ComputePassDescriptor {
        label,
        timestamp_writes,
    } = desc;

    let label = wgpu_string(label);

    let timestamp_writes = timestamp_writes.map(|tsw| {
        let &ComputePassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        } = tsw;
        let beginning_of_pass_write_index = beginning_of_pass_write_index.cloned();
        let end_of_pass_write_index = end_of_pass_write_index.cloned();
        wgc::command::ComputePassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        }
    });
    let timestamp_writes = timestamp_writes.as_ref();

    let pass = wgc::command::ComputePass::new(
        encoder_id,
        &wgc::command::ComputePassDescriptor {
            label,
            timestamp_writes,
        },
    );
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_finish(
    pass: *mut wgc::command::ComputePass,
    output: &mut ByteBuf,
) {
    let command = Box::from_raw(pass).into_command();
    *output = make_byte_buf(&command);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_compute_pass_destroy(pass: *mut wgc::command::ComputePass) {
    let _ = Box::from_raw(pass);
}

#[repr(C)]
pub struct RenderPassDescriptor<'a> {
    pub label: Option<&'a nsACString>,
    pub color_attachments: *const wgc::command::RenderPassColorAttachment,
    pub color_attachments_length: usize,
    pub depth_stencil_attachment: *const wgc::command::RenderPassDepthStencilAttachment,
    pub timestamp_writes: Option<&'a RenderPassTimestampWrites<'a>>,
    pub occlusion_query_set: Option<wgc::id::QuerySetId>,
}

#[repr(C)]
pub struct RenderPassTimestampWrites<'a> {
    pub query_set: wgc::id::QuerySetId,
    pub beginning_of_pass_write_index: Option<&'a u32>,
    pub end_of_pass_write_index: Option<&'a u32>,
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_begin_render_pass(
    encoder_id: id::CommandEncoderId,
    desc: &RenderPassDescriptor,
) -> *mut wgc::command::RenderPass {
    let &RenderPassDescriptor {
        label,
        color_attachments,
        color_attachments_length,
        depth_stencil_attachment,
        timestamp_writes,
        occlusion_query_set,
    } = desc;

    let label = wgpu_string(label);

    let timestamp_writes = timestamp_writes.map(|tsw| {
        let &RenderPassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        } = tsw;
        let beginning_of_pass_write_index = beginning_of_pass_write_index.cloned();
        let end_of_pass_write_index = end_of_pass_write_index.cloned();
        wgc::command::RenderPassTimestampWrites {
            query_set,
            beginning_of_pass_write_index,
            end_of_pass_write_index,
        }
    });

    let timestamp_writes = timestamp_writes.as_ref();

    let color_attachments: Vec<_> = make_slice(color_attachments, color_attachments_length)
        .iter()
        .map(|format| Some(format.clone()))
        .collect();
    let pass = wgc::command::RenderPass::new(
        encoder_id,
        &wgc::command::RenderPassDescriptor {
            label,
            color_attachments: Cow::Owned(color_attachments),
            depth_stencil_attachment: depth_stencil_attachment.as_ref(),
            timestamp_writes,
            occlusion_query_set,
        },
    );
    Box::into_raw(Box::new(pass))
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_finish(
    pass: *mut wgc::command::RenderPass,
    output: &mut ByteBuf,
) {
    let command = Box::from_raw(pass).into_command();
    *output = make_byte_buf(&command);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_render_pass_destroy(pass: *mut wgc::command::RenderPass) {
    let _ = Box::from_raw(pass);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group_layout(
    client: &Client,
    device_id: id::DeviceId,
    desc: &BindGroupLayoutDescriptor,
    bb: &mut ByteBuf,
) -> id::BindGroupLayoutId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .bind_group_layouts
        .alloc(backend);

    let mut entries = Vec::with_capacity(desc.entries_length);
    for entry in make_slice(desc.entries, desc.entries_length) {
        entries.push(wgt::BindGroupLayoutEntry {
            binding: entry.binding,
            visibility: entry.visibility,
            count: None,
            ty: match entry.ty {
                RawBindingType::UniformBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Uniform,
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::StorageBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Storage { read_only: false },
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::ReadonlyStorageBuffer => wgt::BindingType::Buffer {
                    ty: wgt::BufferBindingType::Storage { read_only: true },
                    has_dynamic_offset: entry.has_dynamic_offset,
                    min_binding_size: entry.min_binding_size,
                },
                RawBindingType::Sampler => wgt::BindingType::Sampler(if entry.sampler_compare {
                    wgt::SamplerBindingType::Comparison
                } else if entry.sampler_filter {
                    wgt::SamplerBindingType::Filtering
                } else {
                    wgt::SamplerBindingType::NonFiltering
                }),
                RawBindingType::SampledTexture => wgt::BindingType::Texture {
                    //TODO: the spec has a bug here
                    view_dimension: *entry
                        .view_dimension
                        .unwrap_or(&wgt::TextureViewDimension::D2),
                    sample_type: match entry.texture_sample_type {
                        None | Some(RawTextureSampleType::Float) => {
                            wgt::TextureSampleType::Float { filterable: true }
                        }
                        Some(RawTextureSampleType::UnfilterableFloat) => {
                            wgt::TextureSampleType::Float { filterable: false }
                        }
                        Some(RawTextureSampleType::Uint) => wgt::TextureSampleType::Uint,
                        Some(RawTextureSampleType::Sint) => wgt::TextureSampleType::Sint,
                        Some(RawTextureSampleType::Depth) => wgt::TextureSampleType::Depth,
                    },
                    multisampled: entry.multisampled,
                },
                RawBindingType::ReadonlyStorageTexture => wgt::BindingType::StorageTexture {
                    access: wgt::StorageTextureAccess::ReadOnly,
                    view_dimension: *entry.view_dimension.unwrap(),
                    format: *entry.storage_texture_format.unwrap(),
                },
                RawBindingType::WriteonlyStorageTexture => wgt::BindingType::StorageTexture {
                    access: wgt::StorageTextureAccess::WriteOnly,
                    view_dimension: *entry.view_dimension.unwrap(),
                    format: *entry.storage_texture_format.unwrap(),
                },
            },
        });
    }
    let wgpu_desc = wgc::binding_model::BindGroupLayoutDescriptor {
        label,
        entries: Cow::Owned(entries),
    };

    let action = DeviceAction::CreateBindGroupLayout(id, wgpu_desc);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_render_pipeline_get_bind_group_layout(
    client: &Client,
    pipeline_id: id::RenderPipelineId,
    index: u32,
    bb: &mut ByteBuf,
) -> id::BindGroupLayoutId {
    let backend = pipeline_id.backend();
    let bgl_id = client
        .identities
        .lock()
        .select(backend)
        .bind_group_layouts
        .alloc(backend);

    let action = DeviceAction::RenderPipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    *bb = make_byte_buf(&action);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_compute_pipeline_get_bind_group_layout(
    client: &Client,
    pipeline_id: id::ComputePipelineId,
    index: u32,
    bb: &mut ByteBuf,
) -> id::BindGroupLayoutId {
    let backend = pipeline_id.backend();
    let bgl_id = client
        .identities
        .lock()
        .select(backend)
        .bind_group_layouts
        .alloc(backend);

    let action = DeviceAction::ComputePipelineGetBindGroupLayout(pipeline_id, index, bgl_id);
    *bb = make_byte_buf(&action);

    bgl_id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_pipeline_layout(
    client: &Client,
    device_id: id::DeviceId,
    desc: &PipelineLayoutDescriptor,
    bb: &mut ByteBuf,
) -> id::PipelineLayoutId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .pipeline_layouts
        .alloc(backend);

    let wgpu_desc = wgc::binding_model::PipelineLayoutDescriptor {
        label,
        bind_group_layouts: Cow::Borrowed(make_slice(
            desc.bind_group_layouts,
            desc.bind_group_layouts_length,
        )),
        push_constant_ranges: Cow::Borrowed(&[]),
    };

    let action = DeviceAction::CreatePipelineLayout(id, wgpu_desc);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_bind_group(
    client: &Client,
    device_id: id::DeviceId,
    desc: &BindGroupDescriptor,
    bb: &mut ByteBuf,
) -> id::BindGroupId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let id = client
        .identities
        .lock()
        .select(backend)
        .bind_groups
        .alloc(backend);

    let mut entries = Vec::with_capacity(desc.entries_length);
    for entry in make_slice(desc.entries, desc.entries_length) {
        entries.push(wgc::binding_model::BindGroupEntry {
            binding: entry.binding,
            resource: if let Some(id) = entry.buffer {
                wgc::binding_model::BindingResource::Buffer(wgc::binding_model::BufferBinding {
                    buffer_id: id,
                    offset: entry.offset,
                    size: entry.size,
                })
            } else if let Some(id) = entry.sampler {
                wgc::binding_model::BindingResource::Sampler(id)
            } else if let Some(id) = entry.texture_view {
                wgc::binding_model::BindingResource::TextureView(id)
            } else {
                panic!("Unexpected binding entry {:?}", entry);
            },
        });
    }
    let wgpu_desc = wgc::binding_model::BindGroupDescriptor {
        label,
        layout: desc.layout,
        entries: Cow::Owned(entries),
    };

    let action = DeviceAction::CreateBindGroup(id, wgpu_desc);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub extern "C" fn wgpu_client_make_shader_module_id(
    client: &Client,
    device_id: id::DeviceId,
) -> id::ShaderModuleId {
    let backend = device_id.backend();
    client
        .identities
        .lock()
        .select(backend)
        .shader_modules
        .alloc(backend)
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_compute_pipeline(
    client: &Client,
    device_id: id::DeviceId,
    desc: &ComputePipelineDescriptor,
    bb: &mut ByteBuf,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
) -> id::ComputePipelineId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let mut identities = client.identities.lock();
    let id = identities.select(backend).compute_pipelines.alloc(backend);

    let wgpu_desc = wgc::pipeline::ComputePipelineDescriptor {
        label,
        layout: desc.layout,
        stage: desc.stage.to_wgpu(),
    };

    let implicit = match desc.layout {
        Some(_) => None,
        None => {
            let implicit = ImplicitLayout::new(identities.select(backend), backend);
            ptr::write(implicit_pipeline_layout_id, Some(implicit.pipeline));
            for (i, bgl_id) in implicit.bind_groups.iter().enumerate() {
                *implicit_bind_group_layout_ids.add(i) = Some(*bgl_id);
            }
            Some(implicit)
        }
    };

    let action = DeviceAction::CreateComputePipeline(id, wgpu_desc, implicit);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_client_create_render_pipeline(
    client: &Client,
    device_id: id::DeviceId,
    desc: &RenderPipelineDescriptor,
    bb: &mut ByteBuf,
    implicit_pipeline_layout_id: *mut Option<id::PipelineLayoutId>,
    implicit_bind_group_layout_ids: *mut Option<id::BindGroupLayoutId>,
) -> id::RenderPipelineId {
    let label = wgpu_string(desc.label);

    let backend = device_id.backend();
    let mut identities = client.identities.lock();
    let id = identities.select(backend).render_pipelines.alloc(backend);

    let wgpu_desc = wgc::pipeline::RenderPipelineDescriptor {
        label,
        layout: desc.layout,
        vertex: desc.vertex.to_wgpu(),
        fragment: desc.fragment.map(FragmentState::to_wgpu),
        primitive: desc.primitive.to_wgpu(),
        depth_stencil: desc.depth_stencil.cloned(),
        multisample: desc.multisample.clone(),
        multiview: None,
    };

    let implicit = match desc.layout {
        Some(_) => None,
        None => {
            let implicit = ImplicitLayout::new(identities.select(backend), backend);
            ptr::write(implicit_pipeline_layout_id, Some(implicit.pipeline));
            for (i, bgl_id) in implicit.bind_groups.iter().enumerate() {
                *implicit_bind_group_layout_ids.add(i) = Some(*bgl_id);
            }
            Some(implicit)
        }
    };

    let action = DeviceAction::CreateRenderPipeline(id, wgpu_desc, implicit);
    *bb = make_byte_buf(&action);
    id
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_buffer(
    src: id::BufferId,
    src_offset: wgt::BufferAddress,
    dst: id::BufferId,
    dst_offset: wgt::BufferAddress,
    size: wgt::BufferAddress,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyBufferToBuffer {
        src,
        src_offset,
        dst,
        dst_offset,
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_buffer(
    src: wgc::command::ImageCopyTexture,
    dst_buffer: wgc::id::BufferId,
    dst_layout: &ImageDataLayout,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyTextureToBuffer {
        src,
        dst: wgc::command::ImageCopyBuffer {
            buffer: dst_buffer,
            layout: dst_layout.into_wgt(),
        },
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_buffer_to_texture(
    src_buffer: wgc::id::BufferId,
    src_layout: &ImageDataLayout,
    dst: wgc::command::ImageCopyTexture,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyBufferToTexture {
        src: wgc::command::ImageCopyBuffer {
            buffer: src_buffer,
            layout: src_layout.into_wgt(),
        },
        dst,
        size,
    };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_copy_texture_to_texture(
    src: wgc::command::ImageCopyTexture,
    dst: wgc::command::ImageCopyTexture,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let action = CommandEncoderAction::CopyTextureToTexture { src, dst, size };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub extern "C" fn wgpu_command_encoder_push_debug_group(marker: &nsACString, bb: &mut ByteBuf) {
    let string = marker.to_string();
    let action = CommandEncoderAction::PushDebugGroup(string);
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_pop_debug_group(bb: &mut ByteBuf) {
    let action = CommandEncoderAction::PopDebugGroup;
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_command_encoder_insert_debug_marker(
    marker: &nsACString,
    bb: &mut ByteBuf,
) {
    let string = marker.to_string();
    let action = CommandEncoderAction::InsertDebugMarker(string);
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_buffer(
    dst: id::BufferId,
    offset: wgt::BufferAddress,
    bb: &mut ByteBuf,
) {
    let action = QueueWriteAction::Buffer { dst, offset };
    *bb = make_byte_buf(&action);
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_queue_write_texture(
    dst: wgt::ImageCopyTexture<id::TextureId>,
    layout: ImageDataLayout,
    size: wgt::Extent3d,
    bb: &mut ByteBuf,
) {
    let layout = layout.into_wgt();
    let action = QueueWriteAction::Texture { dst, layout, size };
    *bb = make_byte_buf(&action);
}

/// Returns the block size or zero if the format has multiple aspects (for example depth+stencil).
#[no_mangle]
pub extern "C" fn wgpu_texture_format_block_size_single_aspect(format: wgt::TextureFormat) -> u32 {
    format.block_copy_size(None).unwrap_or(0)
}

#[no_mangle]
pub extern "C" fn wgpu_client_use_external_texture_in_swapChain(
    device_id: id::DeviceId,
    format: wgt::TextureFormat,
) -> bool {
    if device_id.backend() != wgt::Backend::Dx12 {
        return false;
    }

    if !static_prefs::pref!("dom.webgpu.swap-chain.external-texture-dx12") {
        return false;
    }

    let supported = match format {
        wgt::TextureFormat::Bgra8Unorm => true,
        _ => false,
    };

    supported
}

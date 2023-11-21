/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{
    error::{ErrMsg, ErrorBuffer, ErrorBufferType},
    identity::IdentityRecyclerFactory,
    wgpu_string, AdapterInformation, ByteBuf, CommandEncoderAction, DeviceAction, DropAction,
    QueueWriteAction, SwapChainId, TextureAction,
};

use nsstring::{nsACString, nsCString, nsString};

use wgc::{gfx_select, id};
use wgc::{pipeline::CreateShaderModuleError, resource::BufferAccessError};
#[allow(unused_imports)]
use wgh::Instance;

use std::borrow::Cow;
#[allow(unused_imports)]
use std::mem;
use std::os::raw::c_void;
use std::slice;
use std::sync::atomic::{AtomicU32, Ordering};

use std::ffi::{c_long, c_ulong};
#[cfg(target_os = "windows")]
use winapi::shared::dxgi;
#[cfg(target_os = "windows")]
use winapi::um::d3d12 as d3d12_ty;
#[cfg(target_os = "windows")]
use winapi::Interface;

// The seemingly redundant u64 suffixes help cbindgen with generating the right C++ code.
// See https://github.com/mozilla/cbindgen/issues/849.

/// We limit the size of buffer allocations for stability reason.
/// We can reconsider this limit in the future. Note that some drivers (mesa for example),
/// have issues when the size of a buffer, mapping or copy command does not fit into a
/// signed 32 bits integer, so beyond a certain size, large allocations will need some form
/// of driver allow/blocklist.
pub const MAX_BUFFER_SIZE: wgt::BufferAddress = 1u64 << 30u64;
const MAX_BUFFER_SIZE_U32: u32 = MAX_BUFFER_SIZE as u32;

// Mesa has issues with height/depth that don't fit in a 16 bits signed integers.
const MAX_TEXTURE_EXTENT: u32 = std::i16::MAX as u32;

fn restrict_limits(limits: wgt::Limits) -> wgt::Limits {
    wgt::Limits {
        max_buffer_size: limits.max_buffer_size.min(MAX_BUFFER_SIZE),
        max_texture_dimension_1d: limits.max_texture_dimension_1d.min(MAX_TEXTURE_EXTENT),
        max_texture_dimension_2d: limits.max_texture_dimension_2d.min(MAX_TEXTURE_EXTENT),
        max_texture_dimension_3d: limits.max_texture_dimension_3d.min(MAX_TEXTURE_EXTENT),
        max_sampled_textures_per_shader_stage: limits
            .max_sampled_textures_per_shader_stage
            .min(256),
        max_samplers_per_shader_stage: limits.max_samplers_per_shader_stage.min(256),
        max_storage_textures_per_shader_stage: limits
            .max_storage_textures_per_shader_stage
            .min(256),
        max_uniform_buffers_per_shader_stage: limits.max_uniform_buffers_per_shader_stage.min(256),
        max_uniform_buffer_binding_size: limits
            .max_uniform_buffer_binding_size
            .min(MAX_BUFFER_SIZE_U32),
        max_storage_buffer_binding_size: limits
            .max_storage_buffer_binding_size
            .min(MAX_BUFFER_SIZE_U32),
        max_non_sampler_bindings: 10_000,
        ..limits
    }
}

// hide wgc's global in private
pub struct Global {
    global: wgc::global::Global<IdentityRecyclerFactory>,
    #[allow(dead_code)]
    owner: *mut c_void,
}

impl std::ops::Deref for Global {
    type Target = wgc::global::Global<IdentityRecyclerFactory>;
    fn deref(&self) -> &Self::Target {
        &self.global
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_new(
    factory: IdentityRecyclerFactory,
    owner: *mut c_void,
) -> *mut Global {
    log::info!("Initializing WGPU server");
    let backends_pref = static_prefs::pref!("dom.webgpu.wgpu-backend").to_string();
    let backends = if backends_pref.is_empty() {
        #[cfg(windows)]
        {
            wgt::Backends::DX12
        }
        #[cfg(not(windows))]
        {
            wgt::Backends::PRIMARY
        }
    } else {
        log::info!(
            "Selecting backends based on dom.webgpu.wgpu-backend pref: {:?}",
            backends_pref
        );
        wgc::instance::parse_backends_from_comma_list(&backends_pref)
    };

    let mut instance_flags = wgt::InstanceFlags::from_build_config().with_env();
    if !static_prefs::pref!("dom.webgpu.hal-labels") {
        instance_flags.insert(wgt::InstanceFlags::DISCARD_HAL_LABELS);
    }

    let global = wgc::global::Global::new(
        "wgpu",
        factory,
        wgt::InstanceDescriptor {
            backends,
            flags: instance_flags,
            dx12_shader_compiler: wgt::Dx12Compiler::Fxc,
            gles_minor_version: wgt::Gles3MinorVersion::Automatic,
        },
    );
    let global = Global { global, owner };
    Box::into_raw(Box::new(global))
}

/// # Safety
///
/// This function is unsafe because improper use may lead to memory
/// problems. For example, a double-free may occur if the function is called
/// twice on the same raw pointer.
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_delete(global: *mut Global) {
    log::info!("Terminating WGPU server");
    let _ = Box::from_raw(global);
}

#[no_mangle]
pub extern "C" fn wgpu_server_poll_all_devices(global: &Global, force_wait: bool) {
    global.poll_all_devices(force_wait).unwrap();
}

#[repr(C)]
#[derive(Copy, Clone, Debug)]
pub struct FfiLUID {
    low_part: c_ulong,
    high_part: c_long,
}

/// Request an adapter according to the specified options.
/// Provide the list of IDs to pick from.
///
/// Returns the index in this list, or -1 if unable to pick.
///
/// # Safety
///
/// This function is unsafe as there is no guarantee that the given pointer is
/// valid for `id_length` elements.
#[allow(unused_variables)]
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_instance_request_adapter(
    global: &Global,
    desc: &wgc::instance::RequestAdapterOptions,
    ids: *const id::AdapterId,
    id_length: usize,
    adapter_luid: Option<&FfiLUID>,
    mut error_buf: ErrorBuffer,
) -> i8 {
    let ids = slice::from_raw_parts(ids, id_length);

    // Prefer to use the dx12 backend, if one exists, and use the same DXGI adapter as WebRender.
    // If wgpu uses a different adapter than WebRender, textures created by
    // webgpu::ExternalTexture do not work with wgpu.
    #[cfg(target_os = "windows")]
    if global.global.instance.dx12.is_some() && adapter_luid.is_some() {
        let hal = global.global.instance_as_hal::<wgc::api::Dx12>().unwrap();
        for adapter in hal.enumerate_adapters() {
            let raw_adapter = adapter.adapter.raw_adapter();
            let mut desc: dxgi::DXGI_ADAPTER_DESC = unsafe { mem::zeroed() };
            unsafe {
                raw_adapter.GetDesc(&mut desc);
            }
            let id = ids
                .iter()
                .find_map(|id| (id.backend() == wgt::Backend::Dx12).then_some(id));
            if id.is_some()
                && desc.AdapterLuid.LowPart == adapter_luid.unwrap().low_part
                && desc.AdapterLuid.HighPart == adapter_luid.unwrap().high_part
            {
                let adapter_id =
                    global.create_adapter_from_hal::<wgh::api::Dx12>(adapter, id.unwrap().clone());
                return ids.iter().position(|&i| i == adapter_id).unwrap() as i8;
            }
        }
        error_buf.init(ErrMsg {
            message: "Failed to create adapter for dx12",
            r#type: ErrorBufferType::Internal,
        });
        return -1;
    }

    match global.request_adapter(
        desc,
        wgc::instance::AdapterInputs::IdSet(ids, |i| i.backend()),
    ) {
        Ok(id) => ids.iter().position(|&i| i == id).unwrap() as i8,
        Err(e) => {
            error_buf.init(e);
            -1
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_adapter_pack_info(
    global: &Global,
    self_id: Option<id::AdapterId>,
    byte_buf: &mut ByteBuf,
) {
    let mut data = Vec::new();
    match self_id {
        Some(id) => {
            let wgt::AdapterInfo {
                name,
                vendor,
                device,
                device_type,
                driver,
                driver_info,
                backend,
            } = gfx_select!(id => global.adapter_get_info(id)).unwrap();

            if static_prefs::pref!("dom.webgpu.testing.assert-hardware-adapter") {
                let is_hardware = match device_type {
                    wgt::DeviceType::IntegratedGpu | wgt::DeviceType::DiscreteGpu => true,
                    _ => false,
                };
                assert!(
                    is_hardware,
                    "Expected a hardware gpu adapter, got {:?}",
                    device_type
                );
            }

            let info = AdapterInformation {
                id,
                limits: restrict_limits(gfx_select!(id => global.adapter_limits(id)).unwrap()),
                features: gfx_select!(id => global.adapter_features(id)).unwrap(),
                name,
                vendor,
                device,
                device_type,
                driver,
                driver_info,
                backend,
            };
            bincode::serialize_into(&mut data, &info).unwrap();
        }
        None => {
            bincode::serialize_into(&mut data, &0u64).unwrap();
        }
    }
    *byte_buf = ByteBuf::from_vec(data);
}

static TRACE_IDX: AtomicU32 = AtomicU32::new(0);

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_adapter_request_device(
    global: &Global,
    self_id: id::AdapterId,
    byte_buf: &ByteBuf,
    new_id: id::DeviceId,
    mut error_buf: ErrorBuffer,
) {
    let desc: wgc::device::DeviceDescriptor = bincode::deserialize(byte_buf.as_slice()).unwrap();
    let trace_string = std::env::var("WGPU_TRACE").ok().map(|s| {
        let idx = TRACE_IDX.fetch_add(1, Ordering::Relaxed);
        let path = format!("{}/{}/", s, idx);

        if std::fs::create_dir_all(&path).is_err() {
            log::warn!("Failed to create directory {:?} for wgpu recording.", path);
        }

        path
    });
    let trace_path = trace_string
        .as_ref()
        .map(|string| std::path::Path::new(string.as_str()));
    // TODO: in https://github.com/gfx-rs/wgpu/pull/3626/files#diff-033343814319f5a6bd781494692ea626f06f6c3acc0753a12c867b53a646c34eR97
    // which introduced the queue id parameter, the queue id is also the device id. I don't know how applicable this is to
    // other situations (this one in particular).
    let (_, _, error) = gfx_select!(self_id => global.adapter_request_device(self_id, &desc, trace_path, new_id, new_id));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_adapter_drop(global: &Global, adapter_id: id::AdapterId) {
    gfx_select!(adapter_id => global.adapter_drop(adapter_id))
}

#[no_mangle]
pub extern "C" fn wgpu_server_device_destroy(global: &Global, self_id: id::DeviceId) {
    gfx_select!(self_id => global.device_destroy(self_id))
}

#[no_mangle]
pub extern "C" fn wgpu_server_device_drop(global: &Global, self_id: id::DeviceId) {
    gfx_select!(self_id => global.device_drop(self_id))
}

impl ShaderModuleCompilationMessage {
    fn set_error(&mut self, error: &CreateShaderModuleError, source: &str) {
        // The WebGPU spec says that if the message doesn't point to a particular position in
        // the source, the line number, position, offset and lengths should be zero.
        self.line_number = 0;
        self.line_pos = 0;
        self.utf16_offset = 0;
        self.utf16_length = 0;

        if let Some(location) = error.location(source) {
            self.line_number = location.line_number as u64;
            self.line_pos = location.line_position as u64;

            let start = location.offset as usize;
            let end = start + location.length as usize;
            self.utf16_offset = source[0..start].chars().map(|c| c.len_utf16() as u64).sum();
            self.utf16_length = source[start..end]
                .chars()
                .map(|c| c.len_utf16() as u64)
                .sum();
        }

        let error_string = error.to_string();

        if !error_string.is_empty() {
            self.message = nsString::from(&error_string[..]);
        }
    }
}

/// A compilation message representation for the ffi boundary.
/// the message is immediately copied into an equivalent C++
/// structure that owns its strings.
#[repr(C)]
#[derive(Clone)]
pub struct ShaderModuleCompilationMessage {
    pub line_number: u64,
    pub line_pos: u64,
    pub utf16_offset: u64,
    pub utf16_length: u64,
    pub message: nsString,
}

/// Creates a shader module and returns an object describing the errors if any.
///
/// If there was no error, the returned pointer is nil.
#[no_mangle]
pub extern "C" fn wgpu_server_device_create_shader_module(
    global: &Global,
    self_id: id::DeviceId,
    module_id: id::ShaderModuleId,
    label: Option<&nsACString>,
    code: &nsCString,
    out_message: &mut ShaderModuleCompilationMessage,
) -> bool {
    let utf8_label = label.map(|utf16| utf16.to_string());
    let label = utf8_label.as_ref().map(|s| Cow::from(&s[..]));

    let source_str = code.to_utf8();

    let source = wgc::pipeline::ShaderModuleSource::Wgsl(Cow::from(&source_str[..]));

    let desc = wgc::pipeline::ShaderModuleDescriptor {
        label,
        shader_bound_checks: wgt::ShaderBoundChecks::new(),
    };

    let (_, error) = gfx_select!(
        self_id => global.device_create_shader_module(
            self_id, &desc, source, module_id
        )
    );

    if let Some(err) = error {
        out_message.set_error(&err, &source_str[..]);
        return false;
    }

    // Avoid allocating the structure that holds errors in the common case (no errors).
    return true;
}

#[no_mangle]
pub extern "C" fn wgpu_server_device_create_buffer(
    global: &Global,
    self_id: id::DeviceId,
    buffer_id: id::BufferId,
    label: Option<&nsACString>,
    size: wgt::BufferAddress,
    usage: u32,
    mapped_at_creation: bool,
    shm_allocation_failed: bool,
    mut error_buf: ErrorBuffer,
) {
    let utf8_label = label.map(|utf16| utf16.to_string());
    let label = utf8_label.as_ref().map(|s| Cow::from(&s[..]));
    let usage = wgt::BufferUsages::from_bits_retain(usage);

    // Don't trust the graphics driver with buffer sizes larger than our conservative max texture size.
    if shm_allocation_failed || size > MAX_BUFFER_SIZE {
        error_buf.init(ErrMsg {
            message: "Out of memory",
            r#type: ErrorBufferType::OutOfMemory,
        });
        gfx_select!(self_id => global.create_buffer_error(buffer_id, label));
        return;
    }

    let desc = wgc::resource::BufferDescriptor {
        label,
        size,
        usage,
        mapped_at_creation,
    };
    let (_, error) = gfx_select!(self_id => global.device_create_buffer(self_id, &desc, buffer_id));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

/// # Safety
///
/// Callers are responsible for ensuring `callback` is well-formed.
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_buffer_map(
    global: &Global,
    buffer_id: id::BufferId,
    start: wgt::BufferAddress,
    size: wgt::BufferAddress,
    map_mode: wgc::device::HostMap,
    callback: wgc::resource::BufferMapCallbackC,
) {
    let callback = wgc::resource::BufferMapCallback::from_c(callback);
    let operation = wgc::resource::BufferMapOperation {
        host: map_mode,
        callback: Some(callback),
    };
    // All errors are also exposed to the mapping callback, so we handle them there and ignore
    // the returned value of buffer_map_async.
    let _ = gfx_select!(buffer_id => global.buffer_map_async(
        buffer_id,
        start .. start + size,
        operation
    ));
}

#[repr(C)]
pub struct MappedBufferSlice {
    pub ptr: *mut u8,
    pub length: u64,
}

/// # Safety
///
/// This function is unsafe as there is no guarantee that the given pointer is
/// valid for `size` elements.
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_buffer_get_mapped_range(
    global: &Global,
    buffer_id: id::BufferId,
    start: wgt::BufferAddress,
    size: wgt::BufferAddress,
) -> MappedBufferSlice {
    let result = gfx_select!(buffer_id => global.buffer_get_mapped_range(
        buffer_id,
        start,
        Some(size)
    ));

    // TODO: error reporting.

    result
        .map(|(ptr, length)| MappedBufferSlice { ptr, length })
        .unwrap_or(MappedBufferSlice {
            ptr: std::ptr::null_mut(),
            length: 0,
        })
}

#[no_mangle]
pub extern "C" fn wgpu_server_buffer_unmap(
    global: &Global,
    buffer_id: id::BufferId,
    mut error_buf: ErrorBuffer,
) {
    if let Err(e) = gfx_select!(buffer_id => global.buffer_unmap(buffer_id)) {
        match e {
            // NOTE: This is presumed by CTS test cases, and was even formally specified in the
            // WebGPU spec. previously, but this doesn't seem formally specified now. :confused:
            //
            // TODO: upstream this; see <https://bugzilla.mozilla.org/show_bug.cgi?id=1842297>.
            BufferAccessError::Invalid => (),
            other => error_buf.init(other),
        }
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_buffer_destroy(global: &Global, self_id: id::BufferId) {
    // Per spec, there is no need for the buffer or even device to be in a valid state,
    // even calling calling destroy multiple times is fine, so no error to push into
    // an error scope.
    let _ = gfx_select!(self_id => global.buffer_destroy(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_buffer_drop(global: &Global, self_id: id::BufferId) {
    gfx_select!(self_id => global.buffer_drop(self_id, false));
}

extern "C" {
    #[allow(dead_code)]
    fn wgpu_server_use_external_texture_for_swap_chain(
        param: *mut c_void,
        swap_chain_id: SwapChainId,
    ) -> bool;
    #[allow(dead_code)]
    fn wgpu_server_ensure_external_texture_for_swap_chain(
        param: *mut c_void,
        swap_chain_id: SwapChainId,
        device_id: id::DeviceId,
        texture_id: id::TextureId,
        width: u32,
        height: u32,
        format: wgt::TextureFormat,
        usage: wgt::TextureUsages,
    ) -> bool;
    #[allow(dead_code)]
    fn wgpu_server_get_external_texture_handle(
        param: *mut c_void,
        id: id::TextureId,
    ) -> *mut c_void;
}

impl Global {
    fn device_action<A: wgc::hal_api::HalApi>(
        &self,
        self_id: id::DeviceId,
        action: DeviceAction,
        mut error_buf: ErrorBuffer,
    ) {
        match action {
            #[allow(unused_variables)]
            DeviceAction::CreateTexture(id, desc, swap_chain_id) => {
                let max = MAX_TEXTURE_EXTENT;
                if desc.size.width > max
                    || desc.size.height > max
                    || desc.size.depth_or_array_layers > max
                {
                    gfx_select!(self_id => self.create_texture_error(id, desc.label));
                    error_buf.init(ErrMsg {
                        message: "Out of memory",
                        r#type: ErrorBufferType::OutOfMemory,
                    });
                    return;
                }

                #[cfg(target_os = "windows")]
                {
                    let use_external_texture = if swap_chain_id.is_some() {
                        unsafe {
                            wgpu_server_use_external_texture_for_swap_chain(
                                self.owner,
                                swap_chain_id.unwrap(),
                            )
                        }
                    } else {
                        false
                    };

                    if use_external_texture && self_id.backend() == wgt::Backend::Dx12 {
                        let ret = unsafe {
                            wgpu_server_ensure_external_texture_for_swap_chain(
                                self.owner,
                                swap_chain_id.unwrap(),
                                self_id,
                                id,
                                desc.size.width,
                                desc.size.height,
                                desc.format,
                                desc.usage,
                            )
                        };
                        if ret != true {
                            error_buf.init(ErrMsg {
                                message: "Failed to create external texture",
                                r#type: ErrorBufferType::Internal,
                            });
                            return;
                        }

                        let dx12_device = unsafe {
                            self.device_as_hal::<wgc::api::Dx12, _, d3d12::Device>(
                                self_id,
                                |hal_device| hal_device.unwrap().raw_device().clone(),
                            )
                        };

                        let handle =
                            unsafe { wgpu_server_get_external_texture_handle(self.owner, id) };
                        if handle.is_null() {
                            error_buf.init(ErrMsg {
                                message: "Failed to get external texture handle",
                                r#type: ErrorBufferType::Internal,
                            });
                        }
                        let mut resource = d3d12::Resource::null();
                        let hr = unsafe {
                            dx12_device.OpenSharedHandle(
                                handle,
                                &d3d12_ty::ID3D12Resource::uuidof(),
                                resource.mut_void(),
                            )
                        };
                        if hr != 0 {
                            error_buf.init(ErrMsg {
                                message: "Failed to open shared handle",
                                r#type: ErrorBufferType::Internal,
                            });
                        }

                        let hal_texture = unsafe {
                            <wgh::api::Dx12 as wgh::Api>::Device::texture_from_raw(
                                resource,
                                wgt::TextureFormat::Bgra8Unorm,
                                wgt::TextureDimension::D2,
                                desc.size,
                                1,
                                1,
                            )
                        };
                        let (_, error) = unsafe {
                            self.create_texture_from_hal::<wgh::api::Dx12>(
                                hal_texture,
                                self_id,
                                &desc,
                                id,
                            )
                        };
                        if let Some(err) = error {
                            error_buf.init(err);
                        }
                        return;
                    }
                }

                let (_, error) = self.device_create_texture::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateSampler(id, desc) => {
                let (_, error) = self.device_create_sampler::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateBindGroupLayout(id, desc) => {
                let (_, error) = self.device_create_bind_group_layout::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::RenderPipelineGetBindGroupLayout(pipeline_id, index, bgl_id) => {
                let (_, error) =
                    self.render_pipeline_get_bind_group_layout::<A>(pipeline_id, index, bgl_id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::ComputePipelineGetBindGroupLayout(pipeline_id, index, bgl_id) => {
                let (_, error) =
                    self.compute_pipeline_get_bind_group_layout::<A>(pipeline_id, index, bgl_id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreatePipelineLayout(id, desc) => {
                let (_, error) = self.device_create_pipeline_layout::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateBindGroup(id, desc) => {
                let (_, error) = self.device_create_bind_group::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateShaderModule(id, desc, code) => {
                let source = wgc::pipeline::ShaderModuleSource::Wgsl(code);
                let (_, error) = self.device_create_shader_module::<A>(self_id, &desc, source, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateComputePipeline(id, desc, implicit) => {
                let implicit_ids = implicit
                    .as_ref()
                    .map(|imp| wgc::device::ImplicitPipelineIds {
                        root_id: imp.pipeline,
                        group_ids: &imp.bind_groups,
                    });
                let (_, error) =
                    self.device_create_compute_pipeline::<A>(self_id, &desc, id, implicit_ids);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateRenderPipeline(id, desc, implicit) => {
                let implicit_ids = implicit
                    .as_ref()
                    .map(|imp| wgc::device::ImplicitPipelineIds {
                        root_id: imp.pipeline,
                        group_ids: &imp.bind_groups,
                    });
                let (_, error) =
                    self.device_create_render_pipeline::<A>(self_id, &desc, id, implicit_ids);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateRenderBundle(id, encoder, desc) => {
                let (_, error) = self.render_bundle_encoder_finish::<A>(encoder, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::CreateRenderBundleError(buffer_id, label) => {
                self.create_render_bundle_error::<A>(buffer_id, label);
            }
            DeviceAction::CreateCommandEncoder(id, desc) => {
                let (_, error) = self.device_create_command_encoder::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
            DeviceAction::Error { message, r#type } => {
                error_buf.init(ErrMsg {
                    message: &message,
                    r#type,
                });
            }
        }
    }

    fn texture_action<A: wgc::hal_api::HalApi>(
        &self,
        self_id: id::TextureId,
        action: TextureAction,
        mut error_buf: ErrorBuffer,
    ) {
        match action {
            TextureAction::CreateView(id, desc) => {
                let (_, error) = self.texture_create_view::<A>(self_id, &desc, id);
                if let Some(err) = error {
                    error_buf.init(err);
                }
            }
        }
    }

    fn command_encoder_action<A: wgc::hal_api::HalApi>(
        &self,
        self_id: id::CommandEncoderId,
        action: CommandEncoderAction,
        mut error_buf: ErrorBuffer,
    ) {
        match action {
            CommandEncoderAction::CopyBufferToBuffer {
                src,
                src_offset,
                dst,
                dst_offset,
                size,
            } => {
                if let Err(err) = self.command_encoder_copy_buffer_to_buffer::<A>(
                    self_id, src, src_offset, dst, dst_offset, size,
                ) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::CopyBufferToTexture { src, dst, size } => {
                if let Err(err) =
                    self.command_encoder_copy_buffer_to_texture::<A>(self_id, &src, &dst, &size)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::CopyTextureToBuffer { src, dst, size } => {
                if let Err(err) =
                    self.command_encoder_copy_texture_to_buffer::<A>(self_id, &src, &dst, &size)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::CopyTextureToTexture { src, dst, size } => {
                if let Err(err) =
                    self.command_encoder_copy_texture_to_texture::<A>(self_id, &src, &dst, &size)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::RunComputePass {
                base,
                timestamp_writes,
            } => {
                if let Err(err) = self.command_encoder_run_compute_pass_impl::<A>(
                    self_id,
                    base.as_ref(),
                    timestamp_writes.as_ref(),
                ) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::WriteTimestamp {
                query_set_id,
                query_index,
            } => {
                if let Err(err) =
                    self.command_encoder_write_timestamp::<A>(self_id, query_set_id, query_index)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::ResolveQuerySet {
                query_set_id,
                start_query,
                query_count,
                destination,
                destination_offset,
            } => {
                if let Err(err) = self.command_encoder_resolve_query_set::<A>(
                    self_id,
                    query_set_id,
                    start_query,
                    query_count,
                    destination,
                    destination_offset,
                ) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::RunRenderPass {
                base,
                target_colors,
                target_depth_stencil,
                timestamp_writes,
                occlusion_query_set_id,
            } => {
                if let Err(err) = self.command_encoder_run_render_pass_impl::<A>(
                    self_id,
                    base.as_ref(),
                    &target_colors,
                    target_depth_stencil.as_ref(),
                    timestamp_writes.as_ref(),
                    occlusion_query_set_id,
                ) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::ClearBuffer { dst, offset, size } => {
                if let Err(err) = self.command_encoder_clear_buffer::<A>(self_id, dst, offset, size)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::ClearTexture {
                dst,
                ref subresource_range,
            } => {
                if let Err(err) =
                    self.command_encoder_clear_texture::<A>(self_id, dst, subresource_range)
                {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::PushDebugGroup(marker) => {
                if let Err(err) = self.command_encoder_push_debug_group::<A>(self_id, &marker) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::PopDebugGroup => {
                if let Err(err) = self.command_encoder_pop_debug_group::<A>(self_id) {
                    error_buf.init(err);
                }
            }
            CommandEncoderAction::InsertDebugMarker(marker) => {
                if let Err(err) = self.command_encoder_insert_debug_marker::<A>(self_id, &marker) {
                    error_buf.init(err);
                }
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_device_action(
    global: &Global,
    self_id: id::DeviceId,
    byte_buf: &ByteBuf,
    error_buf: ErrorBuffer,
) {
    let action = bincode::deserialize(byte_buf.as_slice()).unwrap();
    gfx_select!(self_id => global.device_action(self_id, action, error_buf));
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_texture_action(
    global: &Global,
    self_id: id::TextureId,
    byte_buf: &ByteBuf,
    error_buf: ErrorBuffer,
) {
    let action = bincode::deserialize(byte_buf.as_slice()).unwrap();
    gfx_select!(self_id => global.texture_action(self_id, action, error_buf));
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_command_encoder_action(
    global: &Global,
    self_id: id::CommandEncoderId,
    byte_buf: &ByteBuf,
    error_buf: ErrorBuffer,
) {
    let action = bincode::deserialize(byte_buf.as_slice()).unwrap();
    gfx_select!(self_id => global.command_encoder_action(self_id, action, error_buf));
}

#[no_mangle]
pub extern "C" fn wgpu_server_device_create_encoder(
    global: &Global,
    self_id: id::DeviceId,
    desc: &wgt::CommandEncoderDescriptor<Option<&nsACString>>,
    new_id: id::CommandEncoderId,
    mut error_buf: ErrorBuffer,
) {
    let utf8_label = desc.label.map(|utf16| utf16.to_string());
    let label = utf8_label.as_ref().map(|s| Cow::from(&s[..]));

    let desc = desc.map_label(|_| label);
    let (_, error) =
        gfx_select!(self_id => global.device_create_command_encoder(self_id, &desc, new_id));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_encoder_finish(
    global: &Global,
    self_id: id::CommandEncoderId,
    desc: &wgt::CommandBufferDescriptor<Option<&nsACString>>,
    mut error_buf: ErrorBuffer,
) {
    let label = wgpu_string(desc.label);
    let desc = desc.map_label(|_| label);
    let (_, error) = gfx_select!(self_id => global.command_encoder_finish(self_id, &desc));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_encoder_drop(global: &Global, self_id: id::CommandEncoderId) {
    gfx_select!(self_id => global.command_encoder_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_render_bundle_drop(global: &Global, self_id: id::RenderBundleId) {
    gfx_select!(self_id => global.render_bundle_drop(self_id));
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_encoder_copy_texture_to_buffer(
    global: &Global,
    self_id: id::CommandEncoderId,
    source: &wgc::command::ImageCopyTexture,
    dst_buffer: wgc::id::BufferId,
    dst_layout: &crate::ImageDataLayout,
    size: &wgt::Extent3d,
    mut error_buf: ErrorBuffer,
) {
    let destination = wgc::command::ImageCopyBuffer {
        buffer: dst_buffer,
        layout: dst_layout.into_wgt(),
    };
    if let Err(err) = gfx_select!(self_id => global.command_encoder_copy_texture_to_buffer(self_id, source, &destination, size)) {
        error_buf.init(err);
    }
}

/// # Safety
///
/// This function is unsafe as there is no guarantee that the given pointer is
/// valid for `command_buffer_id_length` elements.
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_queue_submit(
    global: &Global,
    self_id: id::QueueId,
    command_buffer_ids: *const id::CommandBufferId,
    command_buffer_id_length: usize,
    mut error_buf: ErrorBuffer,
) {
    let command_buffers = slice::from_raw_parts(command_buffer_ids, command_buffer_id_length);
    let result = gfx_select!(self_id => global.queue_submit(self_id, command_buffers));
    if let Err(err) = result {
        error_buf.init(err);
    }
}

#[no_mangle]
pub unsafe extern "C" fn wgpu_server_on_submitted_work_done(
    global: &Global,
    self_id: id::QueueId,
    callback: wgc::device::queue::SubmittedWorkDoneClosureC,
) {
    gfx_select!(self_id => global.queue_on_submitted_work_done(self_id, wgc::device::queue::SubmittedWorkDoneClosure::from_c(callback))).unwrap();
}

/// # Safety
///
/// This function is unsafe as there is no guarantee that the given pointer is
/// valid for `data_length` elements.
#[no_mangle]
pub unsafe extern "C" fn wgpu_server_queue_write_action(
    global: &Global,
    self_id: id::QueueId,
    byte_buf: &ByteBuf,
    data: *const u8,
    data_length: usize,
    mut error_buf: ErrorBuffer,
) {
    let action: QueueWriteAction = bincode::deserialize(byte_buf.as_slice()).unwrap();
    let data = slice::from_raw_parts(data, data_length);
    let result = match action {
        QueueWriteAction::Buffer { dst, offset } => {
            gfx_select!(self_id => global.queue_write_buffer(self_id, dst, offset, data))
        }
        QueueWriteAction::Texture { dst, layout, size } => {
            gfx_select!(self_id => global.queue_write_texture(self_id, &dst, data, &layout, &size))
        }
    };
    if let Err(err) = result {
        error_buf.init(err);
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_bind_group_layout_drop(
    global: &Global,
    self_id: id::BindGroupLayoutId,
) {
    gfx_select!(self_id => global.bind_group_layout_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_pipeline_layout_drop(global: &Global, self_id: id::PipelineLayoutId) {
    gfx_select!(self_id => global.pipeline_layout_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_bind_group_drop(global: &Global, self_id: id::BindGroupId) {
    gfx_select!(self_id => global.bind_group_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_shader_module_drop(global: &Global, self_id: id::ShaderModuleId) {
    gfx_select!(self_id => global.shader_module_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_compute_pipeline_drop(
    global: &Global,
    self_id: id::ComputePipelineId,
) {
    gfx_select!(self_id => global.compute_pipeline_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_render_pipeline_drop(global: &Global, self_id: id::RenderPipelineId) {
    gfx_select!(self_id => global.render_pipeline_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_texture_destroy(
    global: &Global,
    self_id: id::TextureId,
) {
    let _ = gfx_select!(self_id => global.texture_destroy(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_texture_drop(global: &Global, self_id: id::TextureId) {
    gfx_select!(self_id => global.texture_drop(self_id, false));
}

#[no_mangle]
pub extern "C" fn wgpu_server_texture_view_drop(global: &Global, self_id: id::TextureViewId) {
    gfx_select!(self_id => global.texture_view_drop(self_id, false)).unwrap();
}

#[no_mangle]
pub extern "C" fn wgpu_server_sampler_drop(global: &Global, self_id: id::SamplerId) {
    gfx_select!(self_id => global.sampler_drop(self_id));
}

#[no_mangle]
pub extern "C" fn wgpu_server_compute_pipeline_get_bind_group_layout(
    global: &Global,
    self_id: id::ComputePipelineId,
    index: u32,
    assign_id: id::BindGroupLayoutId,
    mut error_buf: ErrorBuffer,
) {
    let (_, error) = gfx_select!(self_id => global.compute_pipeline_get_bind_group_layout(self_id, index, assign_id));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

#[no_mangle]
pub extern "C" fn wgpu_server_render_pipeline_get_bind_group_layout(
    global: &Global,
    self_id: id::RenderPipelineId,
    index: u32,
    assign_id: id::BindGroupLayoutId,
    mut error_buf: ErrorBuffer,
) {
    let (_, error) = gfx_select!(self_id => global.render_pipeline_get_bind_group_layout(self_id, index, assign_id));
    if let Some(err) = error {
        error_buf.init(err);
    }
}

/// Encode the freeing of the selected ID into a byte buf.
#[no_mangle]
pub extern "C" fn wgpu_server_adapter_free(id: id::AdapterId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::Adapter(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_device_free(id: id::DeviceId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::Device(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_shader_module_free(
    id: id::ShaderModuleId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::ShaderModule(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_pipeline_layout_free(
    id: id::PipelineLayoutId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::PipelineLayout(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_bind_group_layout_free(
    id: id::BindGroupLayoutId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::BindGroupLayout(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_bind_group_free(id: id::BindGroupId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::BindGroup(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_command_buffer_free(
    id: id::CommandBufferId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::CommandBuffer(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_render_bundle_free(
    id: id::RenderBundleId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::RenderBundle(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_render_pipeline_free(
    id: id::RenderPipelineId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::RenderPipeline(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_compute_pipeline_free(
    id: id::ComputePipelineId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::ComputePipeline(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_buffer_free(id: id::BufferId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::Buffer(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_texture_free(id: id::TextureId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::Texture(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_texture_view_free(
    id: id::TextureViewId,
    drop_byte_buf: &mut ByteBuf,
) {
    *drop_byte_buf = DropAction::TextureView(id).to_byte_buf();
}
#[no_mangle]
pub extern "C" fn wgpu_server_sampler_free(id: id::SamplerId, drop_byte_buf: &mut ByteBuf) {
    *drop_byte_buf = DropAction::Sampler(id).to_byte_buf();
}

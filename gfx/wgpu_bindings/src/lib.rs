/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::ErrorBufferType;
use wgc::id;

pub use wgc::command::{compute_ffi::*, render_ffi::*};

pub mod client;
pub mod error;
pub mod identity;
pub mod server;

pub use wgc::device::trace::Command as CommandEncoderAction;

use std::marker::PhantomData;
use std::{borrow::Cow, mem, slice};

use nsstring::nsACString;

type RawString = *const std::os::raw::c_char;

//TODO: figure out why 'a and 'b have to be different here
//TODO: remove this
fn cow_label<'a, 'b>(raw: &'a RawString) -> Option<Cow<'b, str>> {
    if raw.is_null() {
        None
    } else {
        let cstr = unsafe { std::ffi::CStr::from_ptr(*raw) };
        cstr.to_str().ok().map(Cow::Borrowed)
    }
}

// Hides the repeated boilerplate of turning a `Option<&nsACString>` into a `Option<Cow<str>`.
pub fn wgpu_string(gecko_string: Option<&nsACString>) -> Option<Cow<str>> {
    gecko_string.map(|s| s.to_utf8())
}

/// An equivalent of `&[T]` for ffi structures and function parameters.
#[repr(C)]
pub struct FfiSlice<'a, T> {
    // `data` may be null.
    pub data: *const T,
    pub length: usize,
    pub _marker: PhantomData<&'a T>,
}

impl<'a, T> FfiSlice<'a, T> {
    pub unsafe fn as_slice(&self) -> &'a [T] {
        if self.data.is_null() {
            // It is invalid to construct a rust slice with a null pointer.
            return &[];
        }

        std::slice::from_raw_parts(self.data, self.length)
    }
}

impl<'a, T> Copy for FfiSlice<'a, T> {}
impl<'a, T> Clone for FfiSlice<'a, T> {
    fn clone(&self) -> Self {
        *self
    }
}

#[repr(C)]
pub struct ByteBuf {
    data: *const u8,
    len: usize,
    capacity: usize,
}

impl ByteBuf {
    fn from_vec(vec: Vec<u8>) -> Self {
        if vec.is_empty() {
            ByteBuf {
                data: std::ptr::null(),
                len: 0,
                capacity: 0,
            }
        } else {
            let bb = ByteBuf {
                data: vec.as_ptr(),
                len: vec.len(),
                capacity: vec.capacity(),
            };
            mem::forget(vec);
            bb
        }
    }

    unsafe fn as_slice(&self) -> &[u8] {
        slice::from_raw_parts(self.data, self.len)
    }
}

#[repr(C)]
#[derive(serde::Serialize, serde::Deserialize)]
pub struct AdapterInformation<S> {
    id: id::AdapterId,
    limits: wgt::Limits,
    features: wgt::Features,
    name: S,
    vendor: u32,
    device: u32,
    device_type: wgt::DeviceType,
    driver: S,
    driver_info: S,
    backend: wgt::Backend,
}

#[derive(serde::Serialize, serde::Deserialize)]
struct ImplicitLayout<'a> {
    pipeline: id::PipelineLayoutId,
    bind_groups: Cow<'a, [id::BindGroupLayoutId]>,
}

#[derive(serde::Serialize, serde::Deserialize)]
enum DeviceAction<'a> {
    CreateTexture(
        id::TextureId,
        wgc::resource::TextureDescriptor<'a>,
        Option<SwapChainId>,
    ),
    CreateSampler(id::SamplerId, wgc::resource::SamplerDescriptor<'a>),
    CreateBindGroupLayout(
        id::BindGroupLayoutId,
        wgc::binding_model::BindGroupLayoutDescriptor<'a>,
    ),
    RenderPipelineGetBindGroupLayout(id::RenderPipelineId, u32, id::BindGroupLayoutId),
    ComputePipelineGetBindGroupLayout(id::ComputePipelineId, u32, id::BindGroupLayoutId),
    CreatePipelineLayout(
        id::PipelineLayoutId,
        wgc::binding_model::PipelineLayoutDescriptor<'a>,
    ),
    CreateBindGroup(id::BindGroupId, wgc::binding_model::BindGroupDescriptor<'a>),
    CreateShaderModule(
        id::ShaderModuleId,
        wgc::pipeline::ShaderModuleDescriptor<'a>,
        Cow<'a, str>,
    ),
    CreateComputePipeline(
        id::ComputePipelineId,
        wgc::pipeline::ComputePipelineDescriptor<'a>,
        Option<ImplicitLayout<'a>>,
    ),
    CreateRenderPipeline(
        id::RenderPipelineId,
        wgc::pipeline::RenderPipelineDescriptor<'a>,
        Option<ImplicitLayout<'a>>,
    ),
    CreateRenderBundle(
        id::RenderBundleId,
        wgc::command::RenderBundleEncoder,
        wgc::command::RenderBundleDescriptor<'a>,
    ),
    CreateRenderBundleError(id::RenderBundleId, wgc::Label<'a>),
    CreateCommandEncoder(
        id::CommandEncoderId,
        wgt::CommandEncoderDescriptor<wgc::Label<'a>>,
    ),
    Error {
        message: String,
        r#type: ErrorBufferType,
    },
}

#[derive(serde::Serialize, serde::Deserialize)]
enum QueueWriteAction {
    Buffer {
        dst: id::BufferId,
        offset: wgt::BufferAddress,
    },
    Texture {
        dst: wgt::ImageCopyTexture<id::TextureId>,
        layout: wgt::ImageDataLayout,
        size: wgt::Extent3d,
    },
}

#[derive(serde::Serialize, serde::Deserialize)]
enum TextureAction<'a> {
    CreateView(id::TextureViewId, wgc::resource::TextureViewDescriptor<'a>),
}

#[repr(C)]
#[derive(serde::Serialize, serde::Deserialize)]
enum DropAction {
    Adapter(id::AdapterId),
    Device(id::DeviceId),
    ShaderModule(id::ShaderModuleId),
    PipelineLayout(id::PipelineLayoutId),
    BindGroupLayout(id::BindGroupLayoutId),
    BindGroup(id::BindGroupId),
    CommandBuffer(id::CommandBufferId),
    RenderBundle(id::RenderBundleId),
    RenderPipeline(id::RenderPipelineId),
    ComputePipeline(id::ComputePipelineId),
    Buffer(id::BufferId),
    Texture(id::TextureId),
    TextureView(id::TextureViewId),
    Sampler(id::SamplerId),
}

impl DropAction {
    // helper function to construct byte bufs
    fn to_byte_buf(&self) -> ByteBuf {
        let mut data = Vec::new();
        bincode::serialize_into(&mut data, self).unwrap();
        ByteBuf::from_vec(data)
    }
}

#[repr(C)]
pub struct ImageDataLayout<'a> {
    pub offset: wgt::BufferAddress,
    pub bytes_per_row: Option<&'a u32>,
    pub rows_per_image: Option<&'a u32>,
}

impl<'a> ImageDataLayout<'a> {
    fn into_wgt(&self) -> wgt::ImageDataLayout {
        wgt::ImageDataLayout {
            offset: self.offset,
            bytes_per_row: self.bytes_per_row.map(|bpr| *bpr),
            rows_per_image: self.rows_per_image.map(|rpi| *rpi),
        }
    }
}

#[repr(C)]
#[derive(Copy, Clone, Debug, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct SwapChainId(pub u64);

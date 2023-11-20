/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use wgc::id;

pub type FactoryParam = *mut std::ffi::c_void;

//TODO: remove this in favor of `DropAction` that could be sent over IPC.
#[repr(C)]
pub struct IdentityRecyclerFactory {
    param: FactoryParam,
    free_adapter: extern "C" fn(id::AdapterId, FactoryParam),
    free_device: extern "C" fn(id::DeviceId, FactoryParam),
    free_pipeline_layout: extern "C" fn(id::PipelineLayoutId, FactoryParam),
    free_shader_module: extern "C" fn(id::ShaderModuleId, FactoryParam),
    free_bind_group_layout: extern "C" fn(id::BindGroupLayoutId, FactoryParam),
    free_bind_group: extern "C" fn(id::BindGroupId, FactoryParam),
    free_command_buffer: extern "C" fn(id::CommandBufferId, FactoryParam),
    free_render_bundle: extern "C" fn(id::RenderBundleId, FactoryParam),
    free_render_pipeline: extern "C" fn(id::RenderPipelineId, FactoryParam),
    free_compute_pipeline: extern "C" fn(id::ComputePipelineId, FactoryParam),
    free_query_set: extern "C" fn(id::QuerySetId, FactoryParam),
    free_buffer: extern "C" fn(id::BufferId, FactoryParam),
    free_staging_buffer: extern "C" fn(id::StagingBufferId, FactoryParam),
    free_texture: extern "C" fn(id::TextureId, FactoryParam),
    free_texture_view: extern "C" fn(id::TextureViewId, FactoryParam),
    free_sampler: extern "C" fn(id::SamplerId, FactoryParam),
    free_surface: extern "C" fn(id::SurfaceId, FactoryParam),
}

impl<I: wgc::id::TypedId> wgc::identity::IdentityHandlerFactory<I> for IdentityRecyclerFactory {
    type Input = I;
    fn input_to_id(id_in: Self::Input) -> I {
        id_in
    }
    fn autogenerate_ids() -> bool {
        false
    }
}

impl wgc::identity::GlobalIdentityHandlerFactory for IdentityRecyclerFactory {}

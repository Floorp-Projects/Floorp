/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use wgc::id;

pub type FactoryParam = *mut std::ffi::c_void;

#[derive(Debug)]
pub struct IdentityRecycler<I> {
    fun: extern "C" fn(I, FactoryParam),
    param: FactoryParam,
    kind: &'static str,
}

impl<I: id::TypedId + Clone + std::fmt::Debug> wgc::identity::IdentityHandler<I>
    for IdentityRecycler<I>
{
    type Input = I;
    fn process(&self, id: I, _backend: wgt::Backend) -> I {
        log::debug!("process {} {:?}", self.kind, id);
        //debug_assert_eq!(id.unzip().2, backend);
        id
    }
    fn free(&self, id: I) {
        log::debug!("free {} {:?}", self.kind, id);
        (self.fun)(id, self.param);
    }
}

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

impl wgc::identity::IdentityHandlerFactory<id::AdapterId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::AdapterId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_adapter,
            param: self.param,
            kind: "adapter",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::DeviceId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::DeviceId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_device,
            param: self.param,
            kind: "device",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::PipelineLayoutId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::PipelineLayoutId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_pipeline_layout,
            param: self.param,
            kind: "pipeline_layout",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::ShaderModuleId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::ShaderModuleId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_shader_module,
            param: self.param,
            kind: "shader_module",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::BindGroupLayoutId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::BindGroupLayoutId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_bind_group_layout,
            param: self.param,
            kind: "bind_group_layout",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::BindGroupId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::BindGroupId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_bind_group,
            param: self.param,
            kind: "bind_group",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::CommandBufferId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::CommandBufferId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_command_buffer,
            param: self.param,
            kind: "command_buffer",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::RenderBundleId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::RenderBundleId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_render_bundle,
            param: self.param,
            kind: "render_bundle",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::RenderPipelineId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::RenderPipelineId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_render_pipeline,
            param: self.param,
            kind: "render_pipeline",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::ComputePipelineId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::ComputePipelineId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_compute_pipeline,
            param: self.param,
            kind: "compute_pipeline",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::QuerySetId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::QuerySetId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_query_set,
            param: self.param,
            kind: "query_set",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::BufferId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::BufferId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_buffer,
            param: self.param,
            kind: "buffer",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::StagingBufferId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::StagingBufferId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_staging_buffer,
            param: self.param,
            kind: "staging buffer",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::TextureId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::TextureId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_texture,
            param: self.param,
            kind: "texture",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::TextureViewId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::TextureViewId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_texture_view,
            param: self.param,
            kind: "texture_view",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::SamplerId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::SamplerId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_sampler,
            param: self.param,
            kind: "sampler",
        }
    }
}
impl wgc::identity::IdentityHandlerFactory<id::SurfaceId> for IdentityRecyclerFactory {
    type Filter = IdentityRecycler<id::SurfaceId>;
    fn spawn(&self) -> Self::Filter {
        IdentityRecycler {
            fun: self.free_surface,
            param: self.param,
            kind: "surface",
        }
    }
}

impl wgc::identity::GlobalIdentityHandlerFactory for IdentityRecyclerFactory {
    fn ids_are_generated_in_wgpu() -> bool {
        false
    }
}

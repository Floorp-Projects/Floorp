/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//! Types needed to marshal [`server`](crate::server) errors back to C++ in Firefox. The main type
//! of this module is [`ErrorBuffer`](crate::server::ErrorBuffer).

use std::{
    error::Error,
    fmt::{self, Display, Formatter},
    os::raw::c_char,
    ptr,
};

use serde::{Deserialize, Serialize};

/// A non-owning representation of `mozilla::webgpu::ErrorBuffer` in C++, passed as an argument to
/// other functions in [this module](self).
///
/// C++ callers of Rust functions (presumably in `WebGPUParent.cpp`) that expect one of these
/// structs can create a `mozilla::webgpu::ErrorBuffer` object, and call its `ToFFI` method to
/// construct a value of this type, available to C++ as `mozilla::webgpu::ffi::WGPUErrorBuffer`. If
/// we catch a `Result::Err` in other functions of [this module](self), the error is converted to
/// this type.
#[repr(C)]
pub struct ErrorBuffer {
    /// The type of error that `string` is associated with. If this location is set to
    /// [`ErrorBufferType::None`] after being passed as an argument to a function in [this module](self),
    /// then the remaining fields are guaranteed to not have been altered by that function from
    /// their original state.
    r#type: *mut ErrorBufferType,
    /// The (potentially truncated) message associated with this error. A fixed-capacity,
    /// null-terminated UTF-8 string buffer owned by C++.
    ///
    /// When we convert WGPU errors to this type, we render the error as a string, copying into
    /// `message` up to `capacity - 1`, and null-terminate it.
    message: *mut c_char,
    message_capacity: usize,
}

impl ErrorBuffer {
    /// Fill this buffer with the textual representation of `error`.
    ///
    /// If the error message is too long, truncate it to `self.capacity`. In either case, the error
    /// message is always terminated by a zero byte.
    ///
    /// Note that there is no explicit indication of the message's length, only the terminating zero
    /// byte. If the textual form of `error` itself includes a zero byte (as Rust strings can), then
    /// the C++ code receiving this error message has no way to distinguish that from the
    /// terminating zero byte, and will see the message as shorter than it is.
    pub(crate) fn init(&mut self, error: impl HasErrorBufferType) {
        use std::fmt::Write;

        let mut message = format!("{}", error);
        let mut e = error.source();
        while let Some(source) = e {
            write!(message, ", caused by: {}", source).unwrap();
            e = source.source();
        }

        let err_ty = error.error_type();
        // SAFETY: We presume the pointer provided by the caller is safe to write to.
        unsafe { *self.r#type = err_ty };

        if matches!(err_ty, ErrorBufferType::None) {
            log::warn!("{message}");
            return;
        }

        assert_ne!(self.message_capacity, 0);
        let length = if message.len() >= self.message_capacity {
            log::warn!(
                "Error message's length {} reached capacity {}, truncating",
                message.len(),
                self.message_capacity
            );
            self.message_capacity - 1
        } else {
            message.len()
        };
        unsafe {
            ptr::copy_nonoverlapping(message.as_ptr(), self.message as *mut u8, length);
            *self.message.add(length) = 0;
        }
    }
}

/// Corresponds to an optional discriminant of [`GPUError`] type in the WebGPU API. Strongly
/// correlates to [`GPUErrorFilter`]s.
///
/// [`GPUError`]: https://gpuweb.github.io/gpuweb/#gpuerror
/// [`GPUErrorFilter`]: https://gpuweb.github.io/gpuweb/#enumdef-gpuerrorfilter
#[repr(u8)]
#[derive(Clone, Copy, Debug, Deserialize, Serialize)]
pub(crate) enum ErrorBufferType {
    None = 0,
    DeviceLost = 1,
    Internal = 2,
    OutOfMemory = 3,
    Validation = 4,
}

/// A trait for querying the [`ErrorBufferType`] classification of an error. Used by
/// [`ErrorBuffer::init`](crate::server::ErrorBuffer::init).
pub(crate) trait HasErrorBufferType: Error {
    fn error_type(&self) -> ErrorBufferType;
}

/// Representation an error whose error message is already rendered as a [`&str`], and has no error
/// sources. Used for convenience in [`server`](crate::server) code.
#[derive(Clone, Debug)]
pub(crate) struct ErrMsg<'a> {
    pub(crate) message: &'a str,
    pub(crate) r#type: ErrorBufferType,
}

impl Display for ErrMsg<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let Self { message, r#type: _ } = self;
        write!(f, "{message}")
    }
}

impl Error for ErrMsg<'_> {}

impl HasErrorBufferType for ErrMsg<'_> {
    fn error_type(&self) -> ErrorBufferType {
        self.r#type
    }
}

/// Encapsulates implementations of [`HasErrorType`] for [`wgpu_core`] types.
mod foreign {
    use wgc::{
        binding_model::{
            CreateBindGroupError, CreateBindGroupLayoutError, CreatePipelineLayoutError,
            GetBindGroupLayoutError,
        },
        command::{
            ClearError, CommandEncoderError, ComputePassError, CopyError, CreateRenderBundleError,
            QueryError, QueryUseError, RenderBundleError, RenderPassError, ResolveError,
            TransferError,
        },
        device::{
            queue::{QueueSubmitError, QueueWriteError},
            DeviceError,
        },
        instance::{RequestAdapterError, RequestDeviceError},
        pipeline::{
            CreateComputePipelineError, CreateRenderPipelineError, CreateShaderModuleError,
        },
        resource::{
            BufferAccessError, CreateBufferError, CreateSamplerError, CreateTextureError,
            CreateTextureViewError, DestroyError,
        },
    };

    use super::{ErrorBufferType, HasErrorBufferType};

    impl HasErrorBufferType for RequestAdapterError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                RequestAdapterError::NotFound | RequestAdapterError::InvalidSurface(_) => {
                    ErrorBufferType::Validation
                }

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for RequestDeviceError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                RequestDeviceError::OutOfMemory => ErrorBufferType::OutOfMemory,

                RequestDeviceError::DeviceLost => ErrorBufferType::DeviceLost,

                RequestDeviceError::Internal
                | RequestDeviceError::InvalidAdapter
                | RequestDeviceError::NoGraphicsQueue => ErrorBufferType::Internal,

                RequestDeviceError::UnsupportedFeature(_)
                | RequestDeviceError::LimitsExceeded(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateBufferError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateBufferError::Device(e) => e.error_type(),
                CreateBufferError::AccessError(e) => e.error_type(),

                CreateBufferError::UnalignedSize
                | CreateBufferError::InvalidUsage(_)
                | CreateBufferError::UsageMismatch(_)
                | CreateBufferError::MaxBufferSize { .. }
                | CreateBufferError::MissingDownlevelFlags(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for BufferAccessError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                BufferAccessError::Device(e) => e.error_type(),

                BufferAccessError::Failed
                | BufferAccessError::Invalid
                | BufferAccessError::Destroyed
                | BufferAccessError::AlreadyMapped
                | BufferAccessError::MapAlreadyPending
                | BufferAccessError::MissingBufferUsage(_)
                | BufferAccessError::NotMapped
                | BufferAccessError::UnalignedRange
                | BufferAccessError::UnalignedOffset { .. }
                | BufferAccessError::UnalignedRangeSize { .. }
                | BufferAccessError::OutOfBoundsUnderrun { .. }
                | BufferAccessError::OutOfBoundsOverrun { .. }
                | BufferAccessError::NegativeRange { .. }
                | BufferAccessError::MapAborted => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateTextureError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateTextureError::Device(e) => e.error_type(),

                CreateTextureError::InvalidUsage(_)
                | CreateTextureError::InvalidDimension(_)
                | CreateTextureError::InvalidDepthDimension(_, _)
                | CreateTextureError::InvalidCompressedDimension(_, _)
                | CreateTextureError::InvalidMipLevelCount { .. }
                | CreateTextureError::InvalidFormatUsages(_, _, _)
                | CreateTextureError::InvalidViewFormat(_, _)
                | CreateTextureError::InvalidDimensionUsages(_, _)
                | CreateTextureError::InvalidMultisampledStorageBinding
                | CreateTextureError::InvalidMultisampledFormat(_)
                | CreateTextureError::InvalidSampleCount(..)
                | CreateTextureError::MultisampledNotRenderAttachment
                | CreateTextureError::MissingFeatures(_, _)
                | CreateTextureError::MissingDownlevelFlags(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateSamplerError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateSamplerError::Device(e) => e.error_type(),

                CreateSamplerError::InvalidLodMinClamp(_)
                | CreateSamplerError::InvalidLodMaxClamp { .. }
                | CreateSamplerError::InvalidAnisotropy(_)
                | CreateSamplerError::InvalidFilterModeWithAnisotropy { .. }
                | CreateSamplerError::TooManyObjects
                | CreateSamplerError::MissingFeatures(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateBindGroupLayoutError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateBindGroupLayoutError::Device(e) => e.error_type(),

                CreateBindGroupLayoutError::ConflictBinding(_)
                | CreateBindGroupLayoutError::Entry { .. }
                | CreateBindGroupLayoutError::TooManyBindings(_)
                | CreateBindGroupLayoutError::InvalidBindingIndex { .. }
                | CreateBindGroupLayoutError::InvalidVisibility(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreatePipelineLayoutError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreatePipelineLayoutError::Device(e) => e.error_type(),

                CreatePipelineLayoutError::InvalidBindGroupLayout(_)
                | CreatePipelineLayoutError::MisalignedPushConstantRange { .. }
                | CreatePipelineLayoutError::MissingFeatures(_)
                | CreatePipelineLayoutError::MoreThanOnePushConstantRangePerStage { .. }
                | CreatePipelineLayoutError::PushConstantRangeTooLarge { .. }
                | CreatePipelineLayoutError::TooManyBindings(_)
                | CreatePipelineLayoutError::TooManyGroups { .. } => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateBindGroupError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateBindGroupError::Device(e) => e.error_type(),

                CreateBindGroupError::InvalidLayout
                | CreateBindGroupError::InvalidBuffer(_)
                | CreateBindGroupError::InvalidTextureView(_)
                | CreateBindGroupError::InvalidTexture(_)
                | CreateBindGroupError::InvalidSampler(_)
                | CreateBindGroupError::BindingArrayPartialLengthMismatch { .. }
                | CreateBindGroupError::BindingArrayLengthMismatch { .. }
                | CreateBindGroupError::BindingArrayZeroLength
                | CreateBindGroupError::BindingRangeTooLarge { .. }
                | CreateBindGroupError::BindingSizeTooSmall { .. }
                | CreateBindGroupError::BindingZeroSize(_)
                | CreateBindGroupError::BindingsNumMismatch { .. }
                | CreateBindGroupError::DuplicateBinding(_)
                | CreateBindGroupError::MissingBindingDeclaration(_)
                | CreateBindGroupError::MissingBufferUsage(_)
                | CreateBindGroupError::MissingTextureUsage(_)
                | CreateBindGroupError::SingleBindingExpected
                | CreateBindGroupError::UnalignedBufferOffset(_, _, _)
                | CreateBindGroupError::BufferRangeTooLarge { .. }
                | CreateBindGroupError::WrongBindingType { .. }
                | CreateBindGroupError::InvalidTextureMultisample { .. }
                | CreateBindGroupError::InvalidTextureSampleType { .. }
                | CreateBindGroupError::InvalidTextureDimension { .. }
                | CreateBindGroupError::InvalidStorageTextureFormat { .. }
                | CreateBindGroupError::InvalidStorageTextureMipLevelCount { .. }
                | CreateBindGroupError::WrongSamplerComparison { .. }
                | CreateBindGroupError::WrongSamplerFiltering { .. }
                | CreateBindGroupError::DepthStencilAspect
                | CreateBindGroupError::StorageReadNotSupported(_)
                | CreateBindGroupError::ResourceUsageConflict(_) => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateShaderModuleError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateShaderModuleError::Device(e) => e.error_type(),

                CreateShaderModuleError::Generation => ErrorBufferType::Internal,

                CreateShaderModuleError::Parsing(_)
                | CreateShaderModuleError::Validation(_)
                | CreateShaderModuleError::MissingFeatures(_)
                | CreateShaderModuleError::InvalidGroupIndex { .. } => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateComputePipelineError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateComputePipelineError::Device(e) => e.error_type(),

                CreateComputePipelineError::Internal(_) => ErrorBufferType::Internal,

                CreateComputePipelineError::InvalidLayout
                | CreateComputePipelineError::Implicit(_)
                | CreateComputePipelineError::Stage(_)
                | CreateComputePipelineError::MissingDownlevelFlags(_) => {
                    ErrorBufferType::Validation
                }

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CreateRenderPipelineError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateRenderPipelineError::Device(e) => e.error_type(),

                CreateRenderPipelineError::Internal { .. } => ErrorBufferType::Internal,

                CreateRenderPipelineError::ColorAttachment(_)
                | CreateRenderPipelineError::InvalidLayout
                | CreateRenderPipelineError::Implicit(_)
                | CreateRenderPipelineError::ColorState(_, _)
                | CreateRenderPipelineError::DepthStencilState(_)
                | CreateRenderPipelineError::InvalidSampleCount(_)
                | CreateRenderPipelineError::TooManyVertexBuffers { .. }
                | CreateRenderPipelineError::TooManyVertexAttributes { .. }
                | CreateRenderPipelineError::VertexStrideTooLarge { .. }
                | CreateRenderPipelineError::UnalignedVertexStride { .. }
                | CreateRenderPipelineError::InvalidVertexAttributeOffset { .. }
                | CreateRenderPipelineError::ShaderLocationClash(_)
                | CreateRenderPipelineError::StripIndexFormatForNonStripTopology { .. }
                | CreateRenderPipelineError::ConservativeRasterizationNonFillPolygonMode
                | CreateRenderPipelineError::MissingFeatures(_)
                | CreateRenderPipelineError::MissingDownlevelFlags(_)
                | CreateRenderPipelineError::Stage { .. }
                | CreateRenderPipelineError::UnalignedShader { .. } => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for RenderBundleError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. May
            // need some upstream work to do this properly.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for DeviceError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                DeviceError::Invalid | DeviceError::WrongDevice => ErrorBufferType::Validation,
                DeviceError::InvalidQueueId => ErrorBufferType::Validation,
                DeviceError::Lost => ErrorBufferType::DeviceLost,
                DeviceError::OutOfMemory => ErrorBufferType::OutOfMemory,
                DeviceError::ResourceCreationFailed => ErrorBufferType::Internal,
                _ => ErrorBufferType::Internal,
            }
        }
    }

    impl HasErrorBufferType for CreateTextureViewError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CreateTextureViewError::OutOfMemory => ErrorBufferType::OutOfMemory,

                CreateTextureViewError::InvalidTexture
                | CreateTextureViewError::InvalidTextureViewDimension { .. }
                | CreateTextureViewError::InvalidMultisampledTextureViewDimension(_)
                | CreateTextureViewError::InvalidCubemapTextureDepth { .. }
                | CreateTextureViewError::InvalidCubemapArrayTextureDepth { .. }
                | CreateTextureViewError::InvalidCubeTextureViewSize
                | CreateTextureViewError::ZeroMipLevelCount
                | CreateTextureViewError::ZeroArrayLayerCount
                | CreateTextureViewError::TooManyMipLevels { .. }
                | CreateTextureViewError::TooManyArrayLayers { .. }
                | CreateTextureViewError::InvalidArrayLayerCount { .. }
                | CreateTextureViewError::InvalidAspect { .. }
                | CreateTextureViewError::FormatReinterpretation { .. } => {
                    ErrorBufferType::Validation
                }

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for CopyError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                CopyError::Encoder(e) => e.error_type(),
                CopyError::Transfer(e) => e.error_type(),

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for TransferError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                TransferError::MemoryInitFailure(e) => e.error_type(),

                TransferError::InvalidBuffer(_)
                | TransferError::InvalidTexture(_)
                | TransferError::SameSourceDestinationBuffer
                | TransferError::MissingCopySrcUsageFlag
                | TransferError::MissingCopyDstUsageFlag(_, _)
                | TransferError::MissingRenderAttachmentUsageFlag(_)
                | TransferError::BufferOverrun { .. }
                | TransferError::TextureOverrun { .. }
                | TransferError::InvalidTextureAspect { .. }
                | TransferError::InvalidTextureMipLevel { .. }
                | TransferError::InvalidDimensionExternal(_)
                | TransferError::UnalignedBufferOffset(_)
                | TransferError::UnalignedCopySize(_)
                | TransferError::UnalignedCopyWidth
                | TransferError::UnalignedCopyHeight
                | TransferError::UnalignedCopyOriginX
                | TransferError::UnalignedCopyOriginY
                | TransferError::UnalignedBytesPerRow
                | TransferError::UnspecifiedBytesPerRow
                | TransferError::UnspecifiedRowsPerImage
                | TransferError::InvalidBytesPerRow
                | TransferError::InvalidCopySize
                | TransferError::InvalidRowsPerImage
                | TransferError::CopySrcMissingAspects
                | TransferError::CopyDstMissingAspects
                | TransferError::CopyAspectNotOne
                | TransferError::CopyFromForbiddenTextureFormat { .. }
                | TransferError::CopyToForbiddenTextureFormat { .. }
                | TransferError::ExternalCopyToForbiddenTextureFormat(_)
                | TransferError::InvalidDepthTextureExtent
                | TransferError::TextureFormatsNotCopyCompatible { .. }
                | TransferError::MissingDownlevelFlags(_)
                | TransferError::InvalidSampleCount { .. }
                | TransferError::InvalidMipLevel { .. } => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for ComputePassError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for QueryError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                QueryError::Encoder(e) => e.error_type(),
                QueryError::Use(e) => e.error_type(),
                QueryError::Resolve(e) => e.error_type(),

                QueryError::InvalidBuffer(_) | QueryError::InvalidQuerySet(_) => {
                    ErrorBufferType::Validation
                }

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for QueryUseError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for ResolveError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for RenderPassError {
        fn error_type(&self) -> ErrorBufferType {
            // TODO: This type's `inner` member has an `OutOfMemory` variant. We definitely need to
            // expose this upstream, or move this implementation upstream.
            //
            // Bug for tracking: https://bugzilla.mozilla.org/show_bug.cgi?id=1840926
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for ClearError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for CommandEncoderError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for QueueSubmitError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                QueueSubmitError::Queue(e) => e.error_type(),
                QueueSubmitError::Unmap(e) => e.error_type(),

                QueueSubmitError::StuckGpu => ErrorBufferType::Internal, // TODO: validate
                QueueSubmitError::DestroyedBuffer(_)
                | QueueSubmitError::DestroyedTexture(_)
                | QueueSubmitError::BufferStillMapped(_)
                | QueueSubmitError::SurfaceOutputDropped
                | QueueSubmitError::SurfaceUnconfigured => ErrorBufferType::Validation,

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for QueueWriteError {
        fn error_type(&self) -> ErrorBufferType {
            match self {
                QueueWriteError::Queue(e) => e.error_type(),
                QueueWriteError::Transfer(e) => e.error_type(),
                QueueWriteError::MemoryInitFailure(e) => e.error_type(),

                // N.B: forced non-exhaustiveness
                _ => ErrorBufferType::Validation,
            }
        }
    }

    impl HasErrorBufferType for GetBindGroupLayoutError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for CreateRenderBundleError {
        fn error_type(&self) -> ErrorBufferType {
            // We can't classify this ourselves, because inner error classification is private. We
            // may need some upstream work to do this properly. For now, we trust that this opaque
            // type only ever represents `Validation`.
            ErrorBufferType::Validation
        }
    }

    impl HasErrorBufferType for DestroyError {
        fn error_type(&self) -> ErrorBufferType {
            ErrorBufferType::Validation
        }
    }
}

//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ErrorStrings.h: Contains mapping of commonly used error messages

#ifndef LIBANGLE_ERRORSTRINGS_H_
#define LIBANGLE_ERRORSTRINGS_H_

#define ERRMSG(name, message) \
    static const constexpr char *kError##name = static_cast<const char *>(message);
#define ANGLE_VALIDATION_ERR(context, error, errorName) \
    context->handleError(error << kError##errorName)

namespace gl
{
ERRMSG(BlitDimensionsOutOfRange, "BlitFramebuffer dimensions out of 32-bit integer range.");
ERRMSG(BlitExtensionDepthStencilWholeBufferBlit,
       "Only whole-buffer depth and stencil blits are supported by this extension.");
ERRMSG(BlitExtensionFormatMismatch,
       "Attempting to blit and the read and draw buffer formats don't match.");
ERRMSG(BlitExtensionFromInvalidAttachmentType,
       "Blits are only supported from 2D texture, renderbuffer or default framebuffer attachments "
       "in this extension.");
ERRMSG(BlitExtensionLinear, "Linear blit not supported in this extension.");
ERRMSG(BlitExtensionMultisampledDepthOrStencil,
       "Multisampled depth/stencil blit is not supported by this extension.");
ERRMSG(BlitExtensionMultisampledWholeBufferBlit,
       "Only whole-buffer blit is supported from a multisampled read buffer in this extension.");
ERRMSG(BlitExtensionNotAvailable, "Blit extension not available.");
ERRMSG(BlitExtensionScaleOrFlip,
       "Scaling and flipping in BlitFramebufferANGLE not supported by this implementation.");
ERRMSG(BlitExtensionToInvalidAttachmentType,
       "Blits are only supported to 2D texture, renderbuffer or default framebuffer attachments in "
       "this extension.");
ERRMSG(BlitFeedbackLoop, "Blit feedback loop: the read and draw framebuffers are the same.");
ERRMSG(BlitFramebufferMissing, "Read and draw framebuffers must both exist for a blit to succeed.");
ERRMSG(BlitFromMultiview, "Attempt to read from a multi-view framebuffer.");
ERRMSG(BlitDepthOrStencilFormatMismatch,
       "Depth/stencil buffer format combination not allowed for blit.");
ERRMSG(BlitIntegerWithLinearFilter,
       "Cannot use GL_LINEAR filter when blitting a integer framebuffer.");
ERRMSG(BlitInvalidFilter, "Invalid blit filter.");
ERRMSG(BlitInvalidMask, "Invalid blit mask.");
ERRMSG(BlitMissingColor,
       "Attempt to read from a missing color attachment of a complete framebuffer.");
ERRMSG(BlitMissingDepthOrStencil,
       "Attempt to read from a missing depth/stencil attachment of a complete framebuffer.");
ERRMSG(BlitOnlyNearestForNonColor,
       "Only nearest filtering can be used when blitting buffers other than the color buffer.");
ERRMSG(BlitToMultiview, "Attempt to write to a multi-view framebuffer.");
ERRMSG(BlitTypeMismatchFixedOrFloat,
       "If the read buffer contains fixed-point or floating-point values, the draw buffer must as "
       "well.");
ERRMSG(BlitTypeMismatchFixedPoint,
       "If the read buffer contains fixed-point values, the draw buffer must as well.");
ERRMSG(BlitTypeMismatchSignedInteger,
       "If the read buffer contains signed integer values the draw buffer must as well.");
ERRMSG(BlitTypeMismatchUnsignedInteger,
       "If the read buffer contains unsigned integer values the draw buffer must as well.");
ERRMSG(BlitMultisampledBoundsMismatch,
       "Attempt to blit from a multisampled framebuffer and the bounds don't match with the draw "
       "framebuffer.");
ERRMSG(BlitMultisampledFormatOrBoundsMismatch,
       "Attempt to blit from a multisampled framebuffer and the bounds or format of the color "
       "buffer don't match with the draw framebuffer.");
ERRMSG(BlitSameImageColor, "Read and write color attachments cannot be the same image.");
ERRMSG(BlitSameImageDepthOrStencil,
       "Read and write depth stencil attachments cannot be the same image.");
ERRMSG(BufferBoundForTransformFeedback, "Buffer is bound for transform feedback.");
ERRMSG(BufferNotBound, "A buffer must be bound.");
ERRMSG(BufferMapped, "An active buffer is mapped");
ERRMSG(CompressedTextureDimensionsMustMatchData,
       "Compressed texture dimensions must exactly match the dimensions of the data passed in.");
ERRMSG(CompressedTexturesNotAttachable, "Compressed textures cannot be attached to a framebuffer.");
ERRMSG(CubemapFacesEqualDimensions, "Each cubemap face must have equal width and height.");
ERRMSG(CubemapIncomplete,
       "Texture is not cubemap complete. All cubemaps faces must be defined and be the same size.");
ERRMSG(DefaultFramebufferInvalidAttachment,
       "Invalid attachment when the default framebuffer is bound.");
ERRMSG(DefaultFramebufferTarget, "It is invalid to change default FBO's attachments");
ERRMSG(DispatchIndirectBufferNotBound, "Dispatch indirect buffer must be bound.");
ERRMSG(DrawBufferTypeMismatch,
       "Fragment shader output type does not match the bound framebuffer attachment type.");
ERRMSG(DrawFramebufferIncomplete, "Draw framebuffer is incomplete");
ERRMSG(ElementArrayBufferBoundForTransformFeedback,
       "It is undefined behavior to use an element array buffer that is bound for transform "
       "feedback.");
ERRMSG(EnumNotSupported, "Enum is not currently supported.");
ERRMSG(EnumRequiresGLES31, "Enum requires GLES 3.1");
ERRMSG(ES31Required, "OpenGL ES 3.1 Required");
ERRMSG(ES3Required, "OpenGL ES 3.0 Required.");
ERRMSG(ExceedsMaxElement, "Element value exceeds maximum element index.");
ERRMSG(ExpectedProgramName, "Expected a program name, but found a shader name.");
ERRMSG(ExpectedShaderName, "Expected a shader name, but found a program name.");
ERRMSG(ExtensionNotEnabled, "Extension is not enabled.");
ERRMSG(FeedbackLoop, "Feedback loop formed between Framebuffer and active Texture.");
ERRMSG(FramebufferIncompleteAttachment,
       "Attachment type must be compatible with attachment object.");
ERRMSG(GenerateMipmapNotAllowed, "Texture format does not support mipmap generation.");
ERRMSG(GeometryShaderExtensionNotEnabled, "GL_EXT_geometry_shader extension not enabled.");
ERRMSG(GLES1Only, "GLES1-only function.");
ERRMSG(ImmutableTextureBound,
       "The value of TEXTURE_IMMUTABLE_FORMAT for the texture currently bound to target on the "
       "active texture unit is true.");
ERRMSG(IncompatibleDrawModeAgainstGeometryShader,
       "Primitive mode is incompatible with the input primitive type of the geometry shader.");
ERRMSG(IndexExceedsMaxActiveUniform, "Index exceeds program active uniform count.");
ERRMSG(IndexExceedsMaxDrawBuffer, "Index exceeds MAX_DRAW_BUFFERS.");
ERRMSG(IndexExceedsMaxVertexAttribute, "Index exceeds MAX_VERTEX_ATTRIBS.");
ERRMSG(InsufficientBufferSize, "Insufficient buffer size.");
ERRMSG(InsufficientVertexBufferSize, "Vertex buffer is not big enough for the draw call");
ERRMSG(IntegerOverflow, "Integer overflow.");
ERRMSG(InternalFormatRequiresTexture2DArray,
       "internalformat is an ETC2/EAC format and target is not GL_TEXTURE_2D_ARRAY.");
ERRMSG(InvalidAttachment, "Invalid Attachment Type.");
ERRMSG(InvalidBlendEquation, "Invalid blend equation.");
ERRMSG(InvalidBlendFunction, "Invalid blend function.");
ERRMSG(InvalidBorder, "Border must be 0.");
ERRMSG(InvalidBufferTypes, "Invalid buffer target enum.");
ERRMSG(InvalidBufferUsage, "Invalid buffer usage enum.");
ERRMSG(InvalidClearMask, "Invalid mask bits.");
ERRMSG(InvalidClientState, "Invalid client vertex array type.");
ERRMSG(InvalidClipPlane, "Invalid clip plane.");
ERRMSG(InvalidCombinedImageUnit,
       "Specified unit must be in [GL_TEXTURE0, GL_TEXTURE0 + GL_MAX_COMBINED_IMAGE_UNITS)");
ERRMSG(InvalidConstantColor,
       "CONSTANT_COLOR (or ONE_MINUS_CONSTANT_COLOR) and CONSTANT_ALPHA (or "
       "ONE_MINUS_CONSTANT_ALPHA) cannot be used together as source and destination factors in the "
       "blend function.");
ERRMSG(InvalidCoverMode, "Invalid cover mode.");
ERRMSG(InvalidCullMode, "Cull mode not recognized.");
ERRMSG(InvalidDebugSeverity, "Invalid debug severity.");
ERRMSG(InvalidDebugSource, "Invalid debug source.");
ERRMSG(InvalidDebugType, "Invalid debug type.");
ERRMSG(InvalidDepthRange, "Near value cannot be greater than far.");
ERRMSG(InvalidDrawMode, "Invalid draw mode.");
ERRMSG(InvalidDrawModeTransformFeedback,
       "Draw mode must match current transform feedback object's draw mode.");
ERRMSG(InvalidFence, "Invalid fence object.");
ERRMSG(InvalidFenceState, "Fence must be set.");
ERRMSG(InvalidFillMode, "Invalid fill mode.");
ERRMSG(InvalidFilterTexture, "Texture only supports NEAREST and LINEAR filtering.");
ERRMSG(InvalidFogDensity, "Invalid fog density (must be nonnegative).");
ERRMSG(InvalidFogMode, "Invalid fog mode.");
ERRMSG(InvalidFogParameter, "Invalid fog parameter.");
ERRMSG(InvalidFormat, "Invalid format.");
ERRMSG(InvalidFramebufferTarget, "Invalid framebuffer target.");
ERRMSG(InvalidFramebufferTextureLevel, "Mipmap level must be 0 when attaching a texture.");
ERRMSG(InvalidFramebufferAttachmentParameter, "Invalid parameter name for framebuffer attachment.");
ERRMSG(InvalidFramebufferLayer,
       "Framebuffer layer cannot be less than 0 or greater than GL_MAX_FRAMEBUFFER_LAYERS_EXT.");
ERRMSG(InvalidImageUnit,
       "Image unit cannot be greater than or equal to the value of MAX_IMAGE_UNITS.");
ERRMSG(InvalidInternalFormat, "Invalid internal format.");
ERRMSG(InvalidLight, "Invalid light.");
ERRMSG(InvalidLightModelParameter, "Invalid light model parameter.");
ERRMSG(InvalidLightParameter, "Invalid light parameter.");
ERRMSG(InvalidLogicOp, "Invalid logical operation.");
ERRMSG(InvalidMaterialFace, "Invalid material face.");
ERRMSG(InvalidMaterialParameter, "Invalid material parameter.");
ERRMSG(InvalidMatrixMode, "Invalid matrix mode.");
ERRMSG(InvalidMemoryBarrierBit, "Invalid memory barrier bit.");
ERRMSG(InvalidMipLevel, "Level of detail outside of range.");
ERRMSG(InvalidMultitextureUnit,
       "Specified unit must be in [GL_TEXTURE0, GL_TEXTURE0 + GL_MAX_TEXTURE_UNITS)");
ERRMSG(InvalidMultisampledFramebufferOperation, "Invalid operation on multisampled framebuffer");
ERRMSG(InvalidName, "Invalid name.");
ERRMSG(InvalidNameCharacters, "Name contains invalid characters.");
ERRMSG(InvalidPname, "Invalid pname.");
ERRMSG(InvalidPointerQuery, "Invalid pointer query.");
ERRMSG(InvalidPointParameter, "Invalid point parameter.");
ERRMSG(InvalidPointParameterValue, "Invalid point parameter value (must be non-negative).");
ERRMSG(InvalidPointSizeValue, "Invalid point size (must be positive).");
ERRMSG(InvalidPrecision, "Invalid or unsupported precision type.");
ERRMSG(InvalidProgramName, "Program object expected.");
ERRMSG(InvalidProjectionMatrix,
       "Invalid projection matrix. Left/right, top/bottom, near/far intervals cannot be zero, and "
       "near/far cannot be less than zero.");
ERRMSG(InvalidQueryId, "Invalid query Id.");
ERRMSG(InvalidQueryTarget, "Invalid query target.");
ERRMSG(InvalidQueryType, "Invalid query type.");
ERRMSG(InvalidRange, "Invalid range.");
ERRMSG(InvalidRenderbufferInternalFormat, "Invalid renderbuffer internalformat.");
ERRMSG(InvalidRenderbufferTarget, "Invalid renderbuffer target.");
ERRMSG(InvalidRenderbufferTextureParameter, "Invalid parameter name for renderbuffer attachment.");
ERRMSG(InvalidRenderbufferWidthHeight,
       "Renderbuffer width and height cannot be negative and cannot exceed maximum texture size.");
ERRMSG(InvalidSampleMaskNumber,
       "MaskNumber cannot be greater than or equal to the value of MAX_SAMPLE_MASK_WORDS.");
ERRMSG(InvalidSampler, "Sampler is not valid");
ERRMSG(InvalidShaderName, "Shader object expected.");
ERRMSG(InvalidShaderType, "Invalid shader type.");
ERRMSG(InvalidShadingModel, "Invalid shading model.");
ERRMSG(InvalidStencil, "Invalid stencil.");
ERRMSG(InvalidStencilBitMask, "Invalid stencil bit mask.");
ERRMSG(InvalidTarget, "Invalid target.");
ERRMSG(InvalidTextureCombine, "Invalid texture combine mode.");
ERRMSG(InvalidTextureCombineSrc, "Invalid texture combine source.");
ERRMSG(InvalidTextureCombineOp, "Invalid texture combine operand.");
ERRMSG(InvalidTextureEnvMode, "Invalid texture environment mode.");
ERRMSG(InvalidTextureEnvParameter, "Invalid texture environment parameter.");
ERRMSG(InvalidTextureEnvScale, "Invalid texture environment scale.");
ERRMSG(InvalidTextureEnvTarget, "Invalid texture environment target.");
ERRMSG(InvalidTextureFilterParam, "Texture filter not recognized.");
ERRMSG(InvalidTextureName, "Not a valid texture object name.");
ERRMSG(InvalidTextureRange, "Cannot be less than 0 or greater than maximum number of textures.");
ERRMSG(InvalidTextureTarget, "Invalid or unsupported texture target.");
ERRMSG(InvalidTextureWrap, "Texture wrap mode not recognized.");
ERRMSG(InvalidType, "Invalid type.");
ERRMSG(InvalidTypePureInt, "Invalid type, should be integer");
ERRMSG(InvalidUnpackAlignment, "Unpack alignment must be 1, 2, 4, or 8.");
ERRMSG(InvalidVertexAttrSize, "Vertex attribute size must be 1, 2, 3, or 4.");
ERRMSG(InvalidVertexPointerSize, "Size for built-in vertex attribute is outside allowed range.");
ERRMSG(InvalidVertexPointerStride, "Invalid stride for built-in vertex attribute.");
ERRMSG(InvalidVertexPointerType, "Invalid type for built-in vertex attribute.");
ERRMSG(InvalidWidth, "Invalid width.");
ERRMSG(InvalidWrapModeTexture, "Invalid wrap mode for texture type.");
ERRMSG(LevelNotZero, "Texture level must be zero.");
ERRMSG(LightParameterOutOfRange, "Light parameter out of range.");
ERRMSG(MaterialParameterOutOfRange, "Material parameter out of range.");
ERRMSG(MatrixStackOverflow, "Current matrix stack is full.");
ERRMSG(MatrixStackUnderflow, "Current matrix stack has only a single matrix.");
ERRMSG(MismatchedByteCountType, "Buffer size does not align with data type.");
ERRMSG(MismatchedFormat, "Format must match internal format.");
ERRMSG(MismatchedTargetAndFormat, "Invalid texture target and format combination.");
ERRMSG(MismatchedTypeAndFormat, "Invalid format and type combination.");
ERRMSG(MismatchedVariableProgram, "Variable is not part of the current program.");
ERRMSG(MissingReadAttachment, "Missing read attachment.");
ERRMSG(MustHaveElementArrayBinding, "Must have element array buffer binding.");
ERRMSG(MultiviewMismatch,
       "The number of views in the active program and draw "
       "framebuffer does not match.");
ERRMSG(MultiviewTransformFeedback,
       "There is an active transform feedback object when "
       "the number of views in the active draw framebuffer "
       "is greater than 1.");
ERRMSG(MultiviewTimerQuery,
       "There is an active query for target "
       "GL_TIME_ELAPSED_EXT when the number of views in the "
       "active draw framebuffer is greater than 1.");
ERRMSG(MultisampleArrayExtensionRequired, "GL_ANGLE_texture_multisample_array not enabled.");
ERRMSG(NameBeginsWithGL, "Attributes that begin with 'gl_' are not allowed.");
ERRMSG(NegativeAttachments, "Negative number of attachments.");
ERRMSG(NegativeBufferSize, "Negative buffer size.");
ERRMSG(NegativeCount, "Negative count.");
ERRMSG(NegativeLength, "Negative length.");
ERRMSG(NegativeMaxCount, "Negative maxcount.");
ERRMSG(NegativeOffset, "Negative offset.");
ERRMSG(NegativePrimcount, "Primcount must be greater than or equal to zero.");
ERRMSG(NegativeSize, "Cannot have negative height or width.");
ERRMSG(NegativeStart, "Cannot have negative start.");
ERRMSG(NegativeStride, "Cannot have negative stride.");
ERRMSG(NoActiveComputeShaderStage, "No active compute shader stage in this program.");
ERRMSG(NoActiveGeometryShaderStage, "No active geometry shader stage in this program.");
ERRMSG(
    NoActiveGraphicsShaderStage,
    "It is a undefined behaviour to render without vertex shader stage or fragment shader stage.");
ERRMSG(NoActiveProgramWithComputeShader, "No active program for the compute shader stage.");
ERRMSG(NonPositiveDrawTextureDimension,
       "Both width and height argument of drawn texture must be positive.");
ERRMSG(NoSuchPath, "No such path object.");
ERRMSG(NoTransformFeedbackOutputVariables,
       "The active program has specified no output variables to record.");
ERRMSG(NoZeroDivisor, "At least one enabled attribute must have a divisor of zero.");
ERRMSG(NVFenceNotSupported, "GL_NV_fence is not supported");
ERRMSG(ObjectNotGenerated, "Object cannot be used because it has not been generated.");
ERRMSG(OffsetMustBeMultipleOfType, "Offset must be a multiple of the passed in datatype.");
ERRMSG(OffsetMustBeMultipleOfUint,
       "Offset must be a multiple of the size, in basic machine units, of uint");
ERRMSG(OutsideOfBounds, "Parameter outside of bounds.");
ERRMSG(ParamOverflow, "The provided parameters overflow with the provided buffer.");
ERRMSG(PixelDataNotNull, "Pixel data must be null.");
ERRMSG(PixelDataNull, "Pixel data cannot be null.");
ERRMSG(PixelPackBufferBoundForTransformFeedback,
       "It is undefined behavior to use a pixel pack buffer that is bound for transform feedback.");
ERRMSG(
    PixelUnpackBufferBoundForTransformFeedback,
    "It is undefined behavior to use a pixel unpack buffer that is bound for transform feedback.");
ERRMSG(PointSizeArrayExtensionNotEnabled, "GL_OES_point_size_array not enabled.");
ERRMSG(ProgramDoesNotExist, "Program doesn't exist.");
ERRMSG(ProgramNotBound, "A program must be bound.");
ERRMSG(ProgramNotLinked, "Program not linked.");
ERRMSG(QueryActive, "Query is active.");
ERRMSG(QueryExtensionNotEnabled, "Query extension not enabled.");
ERRMSG(ReadBufferNone, "Read buffer is GL_NONE.");
ERRMSG(RenderableInternalFormat,
       "SizedInternalformat must be color-renderable, depth-renderable, or stencil-renderable.");
ERRMSG(RenderbufferNotBound, "A renderbuffer must be bound.");
ERRMSG(ResourceMaxTextureSize, "Desired resource size is greater than max texture size.");
ERRMSG(SamplesZero, "Samples may not be zero.");
ERRMSG(SamplesOutOfRange,
       "Samples must not be greater than maximum supported value for the format.");
ERRMSG(ShaderAttachmentHasShader, "Shader attachment already has a shader.");
ERRMSG(ShaderSourceInvalidCharacters, "Shader source contains invalid characters.");
ERRMSG(ShaderToDetachMustBeAttached,
       "Shader to be detached must be currently attached to the program.");
ERRMSG(SourceTextureTooSmall, "The specified dimensions are outside of the bounds of the texture.");
ERRMSG(StencilReferenceMaskOrMismatch,
       "Stencil reference and mask values must be the same for front facing and back facing "
       "triangles.");
ERRMSG(StrideMustBeMultipleOfType, "Stride must be a multiple of the passed in datatype.");
ERRMSG(TargetMustBeTexture2DMultisampleArrayANGLE,
       "Target must be TEXTURE_2D_MULTISAMPLE_ARRAY_ANGLE.");
ERRMSG(TextureNotBound, "A texture must be bound.");
ERRMSG(TextureNotPow2, "The texture is a non-power-of-two texture.");
ERRMSG(TextureTargetRequiresES31, "Texture target requires at least OpenGL ES 3.1.");
ERRMSG(TextureTypeConflict, "Two textures of different types use the same sampler location.");
ERRMSG(TextureWidthOrHeightOutOfRange,
       "Width and height must be less than or equal to GL_MAX_TEXTURE_SIZE.");
ERRMSG(TransformFeedbackBufferDoubleBound,
       "A transform feedback buffer that would be written to is also bound to a "
       "non-transform-feedback target, which would cause undefined behavior.");
ERRMSG(TransformFeedbackBufferTooSmall, "Not enough space in bound transform feedback buffers.");
ERRMSG(TransformFeedbackDoesNotExist, "Transform feedback object that does not exist.");
ERRMSG(TypeMismatch,
       "Passed in texture target and format must match the one originally used to define the "
       "texture.");
ERRMSG(TypeNotUnsignedShortByte, "Only UNSIGNED_SHORT and UNSIGNED_BYTE types are supported.");
ERRMSG(UniformBufferBoundForTransformFeedback,
       "It is undefined behavior to use an uniform buffer that is bound for transform feedback.");
ERRMSG(UniformBufferTooSmall,
       "It is undefined behaviour to use a uniform buffer that is too small.");
ERRMSG(UniformBufferUnbound,
       "It is undefined behaviour to have a used but unbound uniform buffer.");
ERRMSG(UniformSizeMismatch, "Uniform size does not match uniform method.");
ERRMSG(UnknownParameter, "Unknown parameter value.");
ERRMSG(UnsizedInternalFormatUnsupported,
       "Internalformat is one of the unsupported unsized base internalformats.");
ERRMSG(UnsupportedDrawModeForTransformFeedback,
       "The draw command is unsupported when transform feedback is active and not paused.");
ERRMSG(VertexArrayNoBuffer, "An enabled vertex array has no buffer.");
ERRMSG(VertexArrayNoBufferPointer, "An enabled vertex array has no buffer and no pointer.");
ERRMSG(VertexBufferBoundForTransformFeedback,
       "It is undefined behavior to use a vertex buffer that is bound for transform feedback.");
ERRMSG(VertexShaderTypeMismatch,
       "Vertex shader input type does not match the type of the bound vertex attribute.")
ERRMSG(ViewportNegativeSize, "Viewport size cannot be negative.");
ERRMSG(Webgl2NameLengthLimitExceeded, "Location lengths must not be greater than 1024 characters.");
ERRMSG(WebglBindAttribLocationReservedPrefix,
       "Attributes that begin with 'webgl_', or '_webgl_' are not allowed.");
ERRMSG(WebglNameLengthLimitExceeded,
       "Location name lengths must not be greater than 256 characters.");
ERRMSG(ZeroBoundToTarget, "Zero is bound to target.");
}
#undef ERRMSG
#endif  // LIBANGLE_ERRORSTRINGS_H_

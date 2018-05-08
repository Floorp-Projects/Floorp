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
ERRMSG(BufferBoundForTransformFeedback, "Buffer is bound for transform feedback.");
ERRMSG(BufferNotBound, "A buffer must be bound.");
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
ERRMSG(InvalidFormat, "Invalid format.");
ERRMSG(InvalidFramebufferTarget, "Invalid framebuffer target.");
ERRMSG(InvalidFramebufferTextureLevel, "Mipmap level must be 0 when attaching a texture.");
ERRMSG(InvalidFramebufferAttachmentParameter, "Invalid parameter name for framebuffer attachment.");
ERRMSG(InvalidImageUnit,
       "Image unit cannot be greater than or equal to the value of MAX_IMAGE_UNITS.");
ERRMSG(InvalidInternalFormat, "Invalid internal format.");
ERRMSG(InvalidMatrixMode, "Invalid matrix mode.");
ERRMSG(InvalidMemoryBarrierBit, "Invalid memory barrier bit.");
ERRMSG(InvalidMipLevel, "Level of detail outside of range.");
ERRMSG(InvalidMultitextureUnit,
       "Specified unit must be in [GL_TEXTURE0, GL_TEXTURE0 + GL_MAX_TEXTURE_UNITS)");
ERRMSG(InvalidName, "Invalid name.");
ERRMSG(InvalidNameCharacters, "Name contains invalid characters.");
ERRMSG(InvalidPname, "Invalid pname.");
ERRMSG(InvalidPrecision, "Invalid or unsupported precision type.");
ERRMSG(InvalidProgramName, "Program object expected.");
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
ERRMSG(InvalidStencil, "Invalid stencil.");
ERRMSG(InvalidStencilBitMask, "Invalid stencil bit mask.");
ERRMSG(InvalidTarget, "Invalid target.");
ERRMSG(InvalidTextureFilterParam, "Texture filter not recognized.");
ERRMSG(InvalidTextureRange, "Cannot be less than 0 or greater than maximum number of textures.");
ERRMSG(InvalidTextureTarget, "Invalid or unsupported texture target.");
ERRMSG(InvalidTextureWrap, "Texture wrap mode not recognized.");
ERRMSG(InvalidType, "Invalid type.");
ERRMSG(InvalidTypePureInt, "Invalid type, should be integer");
ERRMSG(InvalidUnpackAlignment, "Unpack alignment must be 1, 2, 4, or 8.");
ERRMSG(InvalidVertexAttrSize, "Vertex attribute size must be 1, 2, 3, or 4.");
ERRMSG(InvalidWidth, "Invalid width.");
ERRMSG(InvalidWrapModeTexture, "Invalid wrap mode for texture type.");
ERRMSG(LevelNotZero, "Texture level must be zero.");
ERRMSG(MatrixStackOverflow, "Current matrix stack is full.");
ERRMSG(MatrixStackUnderflow, "Current matrix stack has only a single matrix.");
ERRMSG(MismatchedByteCountType, "Buffer size does not align with data type.");
ERRMSG(MismatchedFormat, "Format must match internal format.");
ERRMSG(MismatchedTargetAndFormat, "Invalid texture target and format combination.");
ERRMSG(MismatchedTypeAndFormat, "Invalid format and type combination.");
ERRMSG(MismatchedVariableProgram, "Variable is not part of the current program.");
ERRMSG(MissingReadAttachment, "Missing read attachment.");
ERRMSG(MustHaveElementArrayBinding, "Must have element array buffer binding.");
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
ERRMSG(NoActiveProgramWithComputeShader, "No active program for the compute shader stage.");
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
ERRMSG(ProgramDoesNotExist, "Program doesn't exist.");
ERRMSG(ProgramNotBound, "A program must be bound.");
ERRMSG(ProgramNotLinked, "Program not linked.");
ERRMSG(QueryActive, "Query is active.");
ERRMSG(QueryExtensionNotEnabled, "Query extension not enabled.");
ERRMSG(ReadBufferNone, "Read buffer is GL_NONE.");
ERRMSG(RenderbufferNotBound, "A renderbuffer must be bound.");
ERRMSG(ResourceMaxTextureSize, "Desired resource size is greater than max texture size.");
ERRMSG(ShaderAttachmentHasShader, "Shader attachment already has a shader.");
ERRMSG(ShaderSourceInvalidCharacters, "Shader source contains invalid characters.");
ERRMSG(ShaderToDetachMustBeAttached,
       "Shader to be detached must be currently attached to the program.");
ERRMSG(SourceTextureTooSmall, "The specified dimensions are outside of the bounds of the texture.");
ERRMSG(StencilReferenceMaskOrMismatch,
       "Stencil reference and mask values must be the same for front facing and back facing "
       "triangles.");
ERRMSG(StrideMustBeMultipleOfType, "Stride must be a multiple of the passed in datatype.");
ERRMSG(TextureNotBound, "A texture must be bound.");
ERRMSG(TextureNotPow2, "The texture is a non-power-of-two texture.");
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
ERRMSG(UniformSizeMismatch, "Uniform size does not match uniform method.");
ERRMSG(UnknownParameter, "Unknown parameter value.");
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
}
#undef ERRMSG
#endif  // LIBANGLE_ERRORSTRINGS_H_

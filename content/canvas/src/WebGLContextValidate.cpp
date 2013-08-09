/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLBuffer.h"
#include "WebGLVertexAttribData.h"
#include "WebGLShader.h"
#include "WebGLProgram.h"
#include "WebGLUniformLocation.h"
#include "WebGLFramebuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"
#include "WebGLVertexArray.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"

#include "jsfriendapi.h"

#include "angle/ShaderLang.h"

#include <algorithm>

#include "mozilla/Services.h"
#include "nsIObserverService.h"

using namespace mozilla;

/*
 * Pull data out of the program, post-linking
 */
bool
WebGLProgram::UpdateInfo()
{
    mIdentifierMap = nullptr;
    mIdentifierReverseMap = nullptr;
    mUniformInfoMap = nullptr;

    mAttribMaxNameLength = 0;

    for (size_t i = 0; i < mAttachedShaders.Length(); i++)
        mAttribMaxNameLength = std::max(mAttribMaxNameLength, mAttachedShaders[i]->mAttribMaxNameLength);

    GLint attribCount;
    mContext->gl->fGetProgramiv(mGLName, LOCAL_GL_ACTIVE_ATTRIBUTES, &attribCount);

    if (!mAttribsInUse.SetLength(mContext->mGLMaxVertexAttribs)) {
        mContext->ErrorOutOfMemory("updateInfo: out of memory to allocate %d attribs", mContext->mGLMaxVertexAttribs);
        return false;
    }

    for (size_t i = 0; i < mAttribsInUse.Length(); i++)
        mAttribsInUse[i] = false;

    nsAutoArrayPtr<char> nameBuf(new char[mAttribMaxNameLength]);

    for (int i = 0; i < attribCount; ++i) {
        GLint attrnamelen;
        GLint attrsize;
        GLenum attrtype;
        mContext->gl->fGetActiveAttrib(mGLName, i, mAttribMaxNameLength, &attrnamelen, &attrsize, &attrtype, nameBuf);
        if (attrnamelen > 0) {
            GLint loc = mContext->gl->fGetAttribLocation(mGLName, nameBuf);
            NS_ABORT_IF_FALSE(loc >= 0, "major oops in managing the attributes of a WebGL program");
            if (loc < mContext->mGLMaxVertexAttribs) {
                mAttribsInUse[loc] = true;
            } else {
                mContext->GenerateWarning("program exceeds MAX_VERTEX_ATTRIBS");
                return false;
            }
        }
    }

    if (!mUniformInfoMap) {
        mUniformInfoMap = new CStringToUniformInfoMap;
        mUniformInfoMap->Init();
        for (size_t i = 0; i < mAttachedShaders.Length(); i++) {
            for (size_t j = 0; j < mAttachedShaders[i]->mUniforms.Length(); j++) {
	        const WebGLMappedIdentifier& uniform = mAttachedShaders[i]->mUniforms[j];
	        const WebGLUniformInfo& info = mAttachedShaders[i]->mUniformInfos[j];
	        mUniformInfoMap->Put(uniform.mapped, info);
            }
        }
    }

    return true;
}

bool WebGLContext::ValidateCapabilityEnum(WebGLenum cap, const char *info)
{
    switch (cap) {
        case LOCAL_GL_BLEND:
        case LOCAL_GL_CULL_FACE:
        case LOCAL_GL_DEPTH_TEST:
        case LOCAL_GL_DITHER:
        case LOCAL_GL_POLYGON_OFFSET_FILL:
        case LOCAL_GL_SAMPLE_ALPHA_TO_COVERAGE:
        case LOCAL_GL_SAMPLE_COVERAGE:
        case LOCAL_GL_SCISSOR_TEST:
        case LOCAL_GL_STENCIL_TEST:
            return true;
        default:
            ErrorInvalidEnumInfo(info, cap);
            return false;
    }
}

bool WebGLContext::ValidateBlendEquationEnum(WebGLenum mode, const char *info)
{
    switch (mode) {
        case LOCAL_GL_FUNC_ADD:
        case LOCAL_GL_FUNC_SUBTRACT:
        case LOCAL_GL_FUNC_REVERSE_SUBTRACT:
            return true;
        case LOCAL_GL_MIN:
        case LOCAL_GL_MAX:
            if (IsWebGL2()) {
                // http://www.opengl.org/registry/specs/EXT/blend_minmax.txt
                return true;
            }
            break;
        default:
            break;
    }

    ErrorInvalidEnumInfo(info, mode);
    return false;
}

bool WebGLContext::ValidateBlendFuncDstEnum(WebGLenum factor, const char *info)
{
    switch (factor) {
        case LOCAL_GL_ZERO:
        case LOCAL_GL_ONE:
        case LOCAL_GL_SRC_COLOR:
        case LOCAL_GL_ONE_MINUS_SRC_COLOR:
        case LOCAL_GL_DST_COLOR:
        case LOCAL_GL_ONE_MINUS_DST_COLOR:
        case LOCAL_GL_SRC_ALPHA:
        case LOCAL_GL_ONE_MINUS_SRC_ALPHA:
        case LOCAL_GL_DST_ALPHA:
        case LOCAL_GL_ONE_MINUS_DST_ALPHA:
        case LOCAL_GL_CONSTANT_COLOR:
        case LOCAL_GL_ONE_MINUS_CONSTANT_COLOR:
        case LOCAL_GL_CONSTANT_ALPHA:
        case LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA:
            return true;
        default:
            ErrorInvalidEnumInfo(info, factor);
            return false;
    }
}

bool WebGLContext::ValidateBlendFuncSrcEnum(WebGLenum factor, const char *info)
{
    if (factor == LOCAL_GL_SRC_ALPHA_SATURATE)
        return true;
    else
        return ValidateBlendFuncDstEnum(factor, info);
}

bool WebGLContext::ValidateBlendFuncEnumsCompatibility(WebGLenum sfactor, WebGLenum dfactor, const char *info)
{
    bool sfactorIsConstantColor = sfactor == LOCAL_GL_CONSTANT_COLOR ||
                                    sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool sfactorIsConstantAlpha = sfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                    sfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    bool dfactorIsConstantColor = dfactor == LOCAL_GL_CONSTANT_COLOR ||
                                    dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_COLOR;
    bool dfactorIsConstantAlpha = dfactor == LOCAL_GL_CONSTANT_ALPHA ||
                                    dfactor == LOCAL_GL_ONE_MINUS_CONSTANT_ALPHA;
    if ( (sfactorIsConstantColor && dfactorIsConstantAlpha) ||
         (dfactorIsConstantColor && sfactorIsConstantAlpha) ) {
        ErrorInvalidOperation("%s are mutually incompatible, see section 6.8 in the WebGL 1.0 spec", info);
        return false;
    } else {
        return true;
    }
}

bool WebGLContext::ValidateTextureTargetEnum(WebGLenum target, const char *info)
{
    switch (target) {
        case LOCAL_GL_TEXTURE_2D:
        case LOCAL_GL_TEXTURE_CUBE_MAP:
            return true;
        default:
            ErrorInvalidEnumInfo(info, target);
            return false;
    }
}

bool WebGLContext::ValidateComparisonEnum(WebGLenum target, const char *info)
{
    switch (target) {
        case LOCAL_GL_NEVER:
        case LOCAL_GL_LESS:
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
            return true;
        default:
            ErrorInvalidEnumInfo(info, target);
            return false;
    }
}

bool WebGLContext::ValidateStencilOpEnum(WebGLenum action, const char *info)
{
    switch (action) {
        case LOCAL_GL_KEEP:
        case LOCAL_GL_ZERO:
        case LOCAL_GL_REPLACE:
        case LOCAL_GL_INCR:
        case LOCAL_GL_INCR_WRAP:
        case LOCAL_GL_DECR:
        case LOCAL_GL_DECR_WRAP:
        case LOCAL_GL_INVERT:
            return true;
        default:
            ErrorInvalidEnumInfo(info, action);
            return false;
    }
}

bool WebGLContext::ValidateFaceEnum(WebGLenum face, const char *info)
{
    switch (face) {
        case LOCAL_GL_FRONT:
        case LOCAL_GL_BACK:
        case LOCAL_GL_FRONT_AND_BACK:
            return true;
        default:
            ErrorInvalidEnumInfo(info, face);
            return false;
    }
}

bool WebGLContext::ValidateBufferUsageEnum(WebGLenum target, const char *info)
{
    switch (target) {
        case LOCAL_GL_STREAM_DRAW:
        case LOCAL_GL_STATIC_DRAW:
        case LOCAL_GL_DYNAMIC_DRAW:
            return true;
        default:
            ErrorInvalidEnumInfo(info, target);
            return false;
    }
}

bool WebGLContext::ValidateDrawModeEnum(WebGLenum mode, const char *info)
{
    switch (mode) {
        case LOCAL_GL_TRIANGLES:
        case LOCAL_GL_TRIANGLE_STRIP:
        case LOCAL_GL_TRIANGLE_FAN:
        case LOCAL_GL_POINTS:
        case LOCAL_GL_LINE_STRIP:
        case LOCAL_GL_LINE_LOOP:
        case LOCAL_GL_LINES:
            return true;
        default:
            ErrorInvalidEnumInfo(info, mode);
            return false;
    }
}

bool WebGLContext::ValidateGLSLVariableName(const nsAString& name, const char *info)
{
    if (name.IsEmpty())
        return false;

    const uint32_t maxSize = 256;
    if (name.Length() > maxSize) {
        ErrorInvalidValue("%s: identifier is %d characters long, exceeds the maximum allowed length of %d characters",
                          info, name.Length(), maxSize);
        return false;
    }

    if (!ValidateGLSLString(name, info)) {
        return false;
    }

    nsString prefix1 = NS_LITERAL_STRING("webgl_");
    nsString prefix2 = NS_LITERAL_STRING("_webgl_");

    if (Substring(name, 0, prefix1.Length()).Equals(prefix1) ||
        Substring(name, 0, prefix2.Length()).Equals(prefix2))
    {
        ErrorInvalidOperation("%s: string contains a reserved GLSL prefix", info);
        return false;
    }

    return true;
}

bool WebGLContext::ValidateGLSLString(const nsAString& string, const char *info)
{
    for (uint32_t i = 0; i < string.Length(); ++i) {
        if (!ValidateGLSLCharacter(string.CharAt(i))) {
             ErrorInvalidValue("%s: string contains the illegal character '%d'", info, string.CharAt(i));
             return false;
        }
    }

    return true;
}

bool WebGLContext::ValidateTexImage2DTarget(WebGLenum target, WebGLsizei width, WebGLsizei height,
                                            const char* info)
{
    switch (target) {
        case LOCAL_GL_TEXTURE_2D:
            break;
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            if (width != height) {
                ErrorInvalidValue("%s: with cube map targets, width and height must be equal", info);
                return false;
            }
            break;
        default:
            ErrorInvalidEnum("%s: invalid target enum 0x%x", info, target);
            return false;
    }

    return true;
}

bool WebGLContext::ValidateCompressedTextureSize(WebGLenum target, WebGLint level,
                                                 WebGLenum format,
                                                 WebGLsizei width, WebGLsizei height, uint32_t byteLength, const char* info)
{
    if (!ValidateLevelWidthHeightForTarget(target, level, width, height, info)) {
        return false;
    }

    // negative width and height must already have been handled above
    MOZ_ASSERT(width >= 0 && height >= 0);

    CheckedUint32 required_byteLength = 0;

    switch (format) {
        case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case LOCAL_GL_ATC_RGB:
        {
            required_byteLength = ((CheckedUint32(width) + 3) / 4) * ((CheckedUint32(height) + 3) / 4) * 8;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
        case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
        {
            required_byteLength = ((CheckedUint32(width) + 3) / 4) * ((CheckedUint32(height) + 3) / 4) * 16;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        {
            required_byteLength = CheckedUint32(std::max(width, 8)) * CheckedUint32(std::max(height, 8)) / 2;
            break;
        }
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        {
            required_byteLength = CheckedUint32(std::max(width, 16)) * CheckedUint32(std::max(height, 8)) / 4;
            break;
        }
    }

    if (!required_byteLength.isValid() || required_byteLength.value() != byteLength) {
        ErrorInvalidValue("%s: data size does not match dimensions", info);
        return false;
    }

    switch (format) {
        case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
        {
            if (level == 0 && width % 4 == 0 && height % 4 == 0) {
                break;
            }
            if (level > 0
                && (width == 0 || width == 1 || width == 2 || width % 4 == 0)
                && (height == 0 || height == 1 || height == 2 || height % 4 == 0))
            {
                break;
            }
            ErrorInvalidOperation("%s: level parameter does not match width and height", info);
            return false;
        }
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
        case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
        {
            if (!is_pot_assuming_nonnegative(width) ||
                !is_pot_assuming_nonnegative(height))
            {
                ErrorInvalidValue("%s: width and height must be powers of two", info);
                return false;
            }
        }
    }

    return true;
}

bool WebGLContext::ValidateLevelWidthHeightForTarget(WebGLenum target, WebGLint level, WebGLsizei width,
                                                     WebGLsizei height, const char* info)
{
    WebGLsizei maxTextureSize = MaxTextureSizeForTarget(target);

    if (level < 0) {
        ErrorInvalidValue("%s: level must be >= 0", info);
        return false;
    }

    WebGLsizei maxAllowedSize = maxTextureSize >> level;

    if (!maxAllowedSize) {
        ErrorInvalidValue("%s: 2^level exceeds maximum texture size", info);
        return false;
    }

    if (width < 0 || height < 0) {
        ErrorInvalidValue("%s: width and height must be >= 0", info);
        return false;
    }

    if (width > maxAllowedSize || height > maxAllowedSize) {
        ErrorInvalidValue("%s: the maximum texture size for level %d is %d", info, level, maxAllowedSize);
        return false;
    }

    return true;
}

uint32_t WebGLContext::GetBitsPerTexel(WebGLenum format, WebGLenum type)
{
    // If there is no defined format or type, we're not taking up any memory
    if (!format || !type) {
        return 0;
    }

    if (format == LOCAL_GL_DEPTH_COMPONENT) {
        if (type == LOCAL_GL_UNSIGNED_SHORT)
            return 2;
        else if (type == LOCAL_GL_UNSIGNED_INT)
            return 4;
    } else if (format == LOCAL_GL_DEPTH_STENCIL) {
        if (type == LOCAL_GL_UNSIGNED_INT_24_8_EXT)
            return 4;
    }

    if (type == LOCAL_GL_UNSIGNED_BYTE || type == LOCAL_GL_FLOAT) {
        int multiplier = type == LOCAL_GL_FLOAT ? 32 : 8;
        switch (format) {
            case LOCAL_GL_ALPHA:
            case LOCAL_GL_LUMINANCE:
                return 1 * multiplier;
            case LOCAL_GL_LUMINANCE_ALPHA:
                return 2 * multiplier;
            case LOCAL_GL_RGB:
                return 3 * multiplier;
            case LOCAL_GL_RGBA:
                return 4 * multiplier;
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_2BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_2BPPV1:
                return 2;
            case LOCAL_GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT1_EXT:
            case LOCAL_GL_ATC_RGB:
            case LOCAL_GL_COMPRESSED_RGB_PVRTC_4BPPV1:
            case LOCAL_GL_COMPRESSED_RGBA_PVRTC_4BPPV1:
                return 4;
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
            case LOCAL_GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            case LOCAL_GL_ATC_RGBA_EXPLICIT_ALPHA:
            case LOCAL_GL_ATC_RGBA_INTERPOLATED_ALPHA:
                return 8;
            default:
                break;
        }
    } else if (type == LOCAL_GL_UNSIGNED_SHORT_4_4_4_4 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_5_5_1 ||
               type == LOCAL_GL_UNSIGNED_SHORT_5_6_5)
    {
        return 16;
    }

    NS_ABORT();
    return 0;
}

bool WebGLContext::ValidateTexFormatAndType(WebGLenum format, WebGLenum type, int jsArrayType,
                                              uint32_t *texelSize, const char *info)
{
    if (IsExtensionEnabled(WEBGL_depth_texture)) {
        if (format == LOCAL_GL_DEPTH_COMPONENT) {
            if (jsArrayType != -1) {
                if ((type == LOCAL_GL_UNSIGNED_SHORT && jsArrayType != js::ArrayBufferView::TYPE_UINT16) ||
                    (type == LOCAL_GL_UNSIGNED_INT && jsArrayType != js::ArrayBufferView::TYPE_UINT32)) {
                    ErrorInvalidOperation("%s: invalid typed array type for given texture data type", info);
                    return false;
                }
            }

            switch(type) {
                case LOCAL_GL_UNSIGNED_SHORT:
                    *texelSize = 2;
                    break;
                case LOCAL_GL_UNSIGNED_INT:
                    *texelSize = 4;
                    break;
                default:
                    ErrorInvalidOperation("%s: invalid type 0x%x", info, type);
                    return false;
            }

            return true;

        } else if (format == LOCAL_GL_DEPTH_STENCIL) {
            if (type != LOCAL_GL_UNSIGNED_INT_24_8_EXT) {
                ErrorInvalidOperation("%s: invalid format 0x%x", info, format);
                return false;
            }
            if (jsArrayType != -1) {
                if (jsArrayType != js::ArrayBufferView::TYPE_UINT32) {
                    ErrorInvalidOperation("%s: invalid typed array type for given texture data type", info);
                    return false;
                }
            }

            *texelSize = 4;
            return true;
        }
    }


    if (type == LOCAL_GL_UNSIGNED_BYTE ||
        (IsExtensionEnabled(OES_texture_float) && type == LOCAL_GL_FLOAT))
    {
        if (jsArrayType != -1) {
            if ((type == LOCAL_GL_UNSIGNED_BYTE && jsArrayType != js::ArrayBufferView::TYPE_UINT8) ||
                (type == LOCAL_GL_FLOAT && jsArrayType != js::ArrayBufferView::TYPE_FLOAT32))
            {
                ErrorInvalidOperation("%s: invalid typed array type for given texture data type", info);
                return false;
            }
        }

        int texMultiplier = type == LOCAL_GL_FLOAT ? 4 : 1;
        switch (format) {
            case LOCAL_GL_ALPHA:
            case LOCAL_GL_LUMINANCE:
                *texelSize = 1 * texMultiplier;
                return true;
            case LOCAL_GL_LUMINANCE_ALPHA:
                *texelSize = 2 * texMultiplier;
                return true;
            case LOCAL_GL_RGB:
                *texelSize = 3 * texMultiplier;
                return true;
            case LOCAL_GL_RGBA:
                *texelSize = 4 * texMultiplier;
                return true;
            default:
                break;
        }

        ErrorInvalidEnum("%s: invalid format 0x%x", info, format);
        return false;
    }

    switch (type) {
        case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
        case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
            if (jsArrayType != -1 && jsArrayType != js::ArrayBufferView::TYPE_UINT16) {
                ErrorInvalidOperation("%s: invalid typed array type for given texture data type", info);
                return false;
            }

            if (format == LOCAL_GL_RGBA) {
                *texelSize = 2;
                return true;
            }
            ErrorInvalidOperation("%s: mutually incompatible format and type", info);
            return false;

        case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
            if (jsArrayType != -1 && jsArrayType != js::ArrayBufferView::TYPE_UINT16) {
                ErrorInvalidOperation("%s: invalid typed array type for given texture data type", info);
                return false;
            }

            if (format == LOCAL_GL_RGB) {
                *texelSize = 2;
                return true;
            }
            ErrorInvalidOperation("%s: mutually incompatible format and type", info);
            return false;

        default:
            break;
        }

    ErrorInvalidEnum("%s: invalid type 0x%x", info, type);
    return false;
}

bool
WebGLContext::ValidateUniformLocation(const char* info, WebGLUniformLocation *location_object)
{
    if (!ValidateObjectAllowNull(info, location_object))
        return false;
    if (!location_object)
        return false;
    /* the need to check specifically for !mCurrentProgram here is explained in bug 657556 */
    if (!mCurrentProgram) {
        ErrorInvalidOperation("%s: no program is currently bound", info);
        return false;
    }
    if (mCurrentProgram != location_object->Program()) {
        ErrorInvalidOperation("%s: this uniform location doesn't correspond to the current program", info);
        return false;
    }
    if (mCurrentProgram->Generation() != location_object->ProgramGeneration()) {
        ErrorInvalidOperation("%s: This uniform location is obsolete since the program has been relinked", info);
        return false;
    }
    return true;
}

bool
WebGLContext::ValidateSamplerUniformSetter(const char* info, WebGLUniformLocation *location, WebGLint value)
{
    if (location->Info().type != SH_SAMPLER_2D &&
        location->Info().type != SH_SAMPLER_CUBE)
    {
        return true;
    }

    if (value >= 0 && value < mGLMaxTextureUnits)
        return true;

    ErrorInvalidValue("%s: this uniform location is a sampler, but %d is not a valid texture unit",
                      info, value);
    return false;
}

bool
WebGLContext::ValidateAttribArraySetter(const char* name, uint32_t cnt, uint32_t arrayLength)
{
    if (!IsContextStable()) {
        return false;
    }
    if (arrayLength < cnt) {
        ErrorInvalidOperation("%s: array must be >= %d elements", name, cnt);
        return false;
    }
    return true;
}

bool
WebGLContext::ValidateUniformArraySetter(const char* name, uint32_t expectedElemSize, WebGLUniformLocation *location_object,
                                         GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength)
{
    if (!IsContextStable())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    uint32_t uniformElemSize = location_object->ElementSize();
    if (expectedElemSize != uniformElemSize) {
        ErrorInvalidOperation("%s: this function expected a uniform of element size %d,"
                              " got a uniform of element size %d", name,
                              expectedElemSize,
                              uniformElemSize);
        return false;
    }
    if (arrayLength == 0 ||
        arrayLength % expectedElemSize)
    {
        ErrorInvalidValue("%s: expected an array of length a multiple"
                          " of %d, got an array of length %d", name,
                          expectedElemSize,
                          arrayLength);
        return false;
    }
    const WebGLUniformInfo& info = location_object->Info();
    if (!info.isArray &&
        arrayLength != expectedElemSize) {
        ErrorInvalidOperation("%s: expected an array of length exactly"
                              " %d (since this uniform is not an array"
                              " uniform), got an array of length %d", name,
                              expectedElemSize,
                              arrayLength);
        return false;
    }
    numElementsToUpload =
        std::min(info.arraySize, arrayLength / expectedElemSize);
    return true;
}

bool
WebGLContext::ValidateUniformMatrixArraySetter(const char* name, int dim, WebGLUniformLocation *location_object,
                                              GLint& location, uint32_t& numElementsToUpload, uint32_t arrayLength,
                                              WebGLboolean aTranspose)
{
    uint32_t expectedElemSize = (dim)*(dim);
    if (!IsContextStable())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    uint32_t uniformElemSize = location_object->ElementSize();
    if (expectedElemSize != uniformElemSize) {
        ErrorInvalidOperation("%s: this function expected a uniform of element size %d,"
                              " got a uniform of element size %d", name,
                              expectedElemSize,
                              uniformElemSize);
        return false;
    }
    if (arrayLength == 0 ||
        arrayLength % expectedElemSize)
    {
        ErrorInvalidValue("%s: expected an array of length a multiple"
                          " of %d, got an array of length %d", name,
                          expectedElemSize,
                          arrayLength);
        return false;
    }
    const WebGLUniformInfo& info = location_object->Info();
    if (!info.isArray &&
        arrayLength != expectedElemSize) {
        ErrorInvalidOperation("%s: expected an array of length exactly"
                              " %d (since this uniform is not an array"
                              " uniform), got an array of length %d", name,
                              expectedElemSize,
                              arrayLength);
        return false;
    }
    if (aTranspose) {
        ErrorInvalidValue("%s: transpose must be FALSE as per the "
                          "OpenGL ES 2.0 spec", name);
        return false;
    }
    numElementsToUpload =
        std::min(info.arraySize, arrayLength / (expectedElemSize));
    return true;
}

bool
WebGLContext::ValidateUniformSetter(const char* name, WebGLUniformLocation *location_object, GLint& location)
{
    if (!IsContextStable())
        return false;
    if (!ValidateUniformLocation(name, location_object))
        return false;
    location = location_object->Location();
    return true;
}

bool WebGLContext::ValidateAttribIndex(WebGLuint index, const char *info)
{
    return mBoundVertexArray->EnsureAttribIndex(index, info);
}

bool WebGLContext::ValidateStencilParamsForDrawCall()
{
  const char *msg = "%s set different front and back stencil %s. Drawing in this configuration is not allowed.";
  if (mStencilRefFront != mStencilRefBack) {
      ErrorInvalidOperation(msg, "stencilFuncSeparate", "reference values");
      return false;
  }
  if (mStencilValueMaskFront != mStencilValueMaskBack) {
      ErrorInvalidOperation(msg, "stencilFuncSeparate", "value masks");
      return false;
  }
  if (mStencilWriteMaskFront != mStencilWriteMaskBack) {
      ErrorInvalidOperation(msg, "stencilMaskSeparate", "write masks");
      return false;
  }
  return true;
}

static inline int32_t floorPOT(int32_t x)
{
    MOZ_ASSERT(x > 0);
    int32_t pot = 1;
    while (pot < 0x40000000) {
        if (x < pot*2)
            break;
        pot *= 2;
    }
    return pot;
}

bool
WebGLContext::InitAndValidateGL()
{
    if (!gl) return false;

    GLenum error = gl->fGetError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during OpenGL context initialization, before WebGL initialization!", error);
        return false;
    }

    mMinCapability = Preferences::GetBool("webgl.min_capability_mode", false);
    mDisableExtensions = Preferences::GetBool("webgl.disable-extensions", false);
    mLoseContextOnHeapMinimize = Preferences::GetBool("webgl.lose-context-on-heap-minimize", false);
    mCanLoseContextInForeground = Preferences::GetBool("webgl.can-lose-context-in-foreground", true);

    if (MinCapabilityMode()) {
      mDisableFragHighP = true;
    }

    mActiveTexture = 0;
    mWebGLError = LOCAL_GL_NO_ERROR;

    mBound2DTextures.Clear();
    mBoundCubeMapTextures.Clear();

    mBoundArrayBuffer = nullptr;
    mCurrentProgram = nullptr;

    mBoundFramebuffer = nullptr;
    mBoundRenderbuffer = nullptr;

    MakeContextCurrent();

    // on desktop OpenGL, we always keep vertex attrib 0 array enabled
    if (!gl->IsGLES2()) {
        gl->fEnableVertexAttribArray(0);
    }

    if (MinCapabilityMode()) {
        mGLMaxVertexAttribs = MINVALUE_GL_MAX_VERTEX_ATTRIBS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_ATTRIBS, &mGLMaxVertexAttribs);
    }
    if (mGLMaxVertexAttribs < 8) {
        GenerateWarning("GL_MAX_VERTEX_ATTRIBS: %d is < 8!", mGLMaxVertexAttribs);
        return false;
    }

    // Note: GL_MAX_TEXTURE_UNITS is fixed at 4 for most desktop hardware,
    // even though the hardware supports much more.  The
    // GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS value is the accurate value.
    if (MinCapabilityMode()) {
        mGLMaxTextureUnits = MINVALUE_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mGLMaxTextureUnits);
    }
    if (mGLMaxTextureUnits < 8) {
        GenerateWarning("GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS: %d is < 8!", mGLMaxTextureUnits);
        return false;
    }

    mBound2DTextures.SetLength(mGLMaxTextureUnits);
    mBoundCubeMapTextures.SetLength(mGLMaxTextureUnits);

    if (MinCapabilityMode()) {
        mGLMaxTextureSize = MINVALUE_GL_MAX_TEXTURE_SIZE;
        mGLMaxCubeMapTextureSize = MINVALUE_GL_MAX_CUBE_MAP_TEXTURE_SIZE;
        mGLMaxRenderbufferSize = MINVALUE_GL_MAX_RENDERBUFFER_SIZE;
        mGLMaxTextureImageUnits = MINVALUE_GL_MAX_TEXTURE_IMAGE_UNITS;
        mGLMaxVertexTextureImageUnits = MINVALUE_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS;
    } else {
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &mGLMaxTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_CUBE_MAP_TEXTURE_SIZE, &mGLMaxCubeMapTextureSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_RENDERBUFFER_SIZE, &mGLMaxRenderbufferSize);
        gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_IMAGE_UNITS, &mGLMaxTextureImageUnits);
        gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &mGLMaxVertexTextureImageUnits);
    }

    mGLMaxTextureSize = floorPOT(mGLMaxTextureSize);
    mGLMaxRenderbufferSize = floorPOT(mGLMaxRenderbufferSize);

    if (MinCapabilityMode()) {
        mGLMaxFragmentUniformVectors = MINVALUE_GL_MAX_FRAGMENT_UNIFORM_VECTORS;
        mGLMaxVertexUniformVectors = MINVALUE_GL_MAX_VERTEX_UNIFORM_VECTORS;
        mGLMaxVaryingVectors = MINVALUE_GL_MAX_VARYING_VECTORS;
    } else {
        if (gl->HasES2Compatibility()) {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_VECTORS, &mGLMaxFragmentUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_VECTORS, &mGLMaxVertexUniformVectors);
            gl->fGetIntegerv(LOCAL_GL_MAX_VARYING_VECTORS, &mGLMaxVaryingVectors);
        } else {
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, &mGLMaxFragmentUniformVectors);
            mGLMaxFragmentUniformVectors /= 4;
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_UNIFORM_COMPONENTS, &mGLMaxVertexUniformVectors);
            mGLMaxVertexUniformVectors /= 4;

            // we are now going to try to read GL_MAX_VERTEX_OUTPUT_COMPONENTS and GL_MAX_FRAGMENT_INPUT_COMPONENTS,
            // however these constants only entered the OpenGL standard at OpenGL 3.2. So we will try reading,
            // and check OpenGL error for INVALID_ENUM.

            // before we start, we check that no error already occurred, to prevent hiding it in our subsequent error handling
            error = gl->GetAndClearError();
            if (error != LOCAL_GL_NO_ERROR) {
                GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
                return false;
            }

            // On the public_webgl list, "problematic GetParameter pnames" thread, the following formula was given:
            //   mGLMaxVaryingVectors = min (GL_MAX_VERTEX_OUTPUT_COMPONENTS, GL_MAX_FRAGMENT_INPUT_COMPONENTS) / 4
            GLint maxVertexOutputComponents,
                  minFragmentInputComponents;
            gl->fGetIntegerv(LOCAL_GL_MAX_VERTEX_OUTPUT_COMPONENTS, &maxVertexOutputComponents);
            gl->fGetIntegerv(LOCAL_GL_MAX_FRAGMENT_INPUT_COMPONENTS, &minFragmentInputComponents);

            error = gl->GetAndClearError();
            switch (error) {
                case LOCAL_GL_NO_ERROR:
                    mGLMaxVaryingVectors = std::min(maxVertexOutputComponents, minFragmentInputComponents) / 4;
                    break;
                case LOCAL_GL_INVALID_ENUM:
                    mGLMaxVaryingVectors = 16; // = 64/4, 64 is the min value for maxVertexOutputComponents in OpenGL 3.2 spec
                    break;
                default:
                    GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
                    return false;
            }   
        }
    }

    // Always 1 for GLES2
    mMaxFramebufferColorAttachments = 1;

    if (!gl->IsGLES2()) {
        // gl_PointSize is always available in ES2 GLSL, but has to be
        // specifically enabled on desktop GLSL.
        gl->fEnable(LOCAL_GL_VERTEX_PROGRAM_POINT_SIZE);

        // gl_PointCoord is always available in ES2 GLSL and in newer desktop GLSL versions, but apparently
        // not in OpenGL 2 and apparently not (due to a driver bug) on certain NVIDIA setups. See:
        //   http://www.opengl.org/discussion_boards/ubbthreads.php?ubb=showflat&Number=261472
        // Note that this used to cause crashes on old ATI drivers... hopefully not a significant
        // problem anymore. See bug 602183.
        gl->fEnable(LOCAL_GL_POINT_SPRITE);
    }

#ifdef XP_MACOSX
    if (gl->WorkAroundDriverBugs() &&
        gl->Vendor() == gl::GLContext::VendorATI) {
        // The Mac ATI driver, in all known OSX version up to and including 10.8,
        // renders points sprites upside-down. Apple bug 11778921
        gl->fPointParameterf(LOCAL_GL_POINT_SPRITE_COORD_ORIGIN, LOCAL_GL_LOWER_LEFT);
    }
#endif

    // Check the shader validator pref
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), false);

    mShaderValidation =
        Preferences::GetBool("webgl.shader_validator", mShaderValidation);

    // initialize shader translator
    if (mShaderValidation) {
        if (!ShInitialize()) {
            GenerateWarning("GLSL translator initialization failed!");
            return false;
        }
    }

    // Mesa can only be detected with the GL_VERSION string, of the form "2.1 Mesa 7.11.0"
    mIsMesa = strstr((const char *)(gl->fGetString(LOCAL_GL_VERSION)), "Mesa");

    // notice that the point of calling GetAndClearError here is not only to check for error,
    // it is also to reset the error flags so that a subsequent WebGL getError call will give the correct result.
    error = gl->GetAndClearError();
    if (error != LOCAL_GL_NO_ERROR) {
        GenerateWarning("GL error 0x%x occurred during WebGL context initialization!", error);
        return false;
    }

    if (IsWebGL2() &&
        (!IsExtensionSupported(OES_vertex_array_object) ||
         !IsExtensionSupported(WEBGL_draw_buffers) ||
         !gl->IsExtensionSupported(gl::GLContext::EXT_gpu_shader4) ||
         !gl->IsExtensionSupported(gl::GLContext::EXT_blend_minmax) ||
         !gl->IsExtensionSupported(gl::GLContext::XXX_draw_instanced) ||
         !gl->IsExtensionSupported(gl::GLContext::XXX_instanced_arrays) ||
         (gl->IsGLES2() && !gl->IsExtensionSupported(gl::GLContext::EXT_occlusion_query_boolean))
        ))
    {
        // Todo: Bug 898404: Only allow WebGL2 on GL>=3.0 on desktop GL.
        return false;
    }

    mMemoryPressureObserver
        = new WebGLMemoryPressureObserver(this);
    nsCOMPtr<nsIObserverService> observerService
        = mozilla::services::GetObserverService();
    if (observerService) {
        observerService->AddObserver(mMemoryPressureObserver,
                                     "memory-pressure",
                                     false);
    }

    mDefaultVertexArray = new WebGLVertexArray(this);
    mDefaultVertexArray->mAttribBuffers.SetLength(mGLMaxVertexAttribs);
    mBoundVertexArray = mDefaultVertexArray;

    if (IsWebGL2()) {
        EnableExtension(OES_vertex_array_object);
        EnableExtension(WEBGL_draw_buffers);

        MOZ_ASSERT(IsExtensionEnabled(OES_vertex_array_object));
        MOZ_ASSERT(IsExtensionEnabled(WEBGL_draw_buffers));
    }

    return true;
}

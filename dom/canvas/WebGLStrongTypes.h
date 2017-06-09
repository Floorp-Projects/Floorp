/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_STRONG_TYPES_H_
#define WEBGL_STRONG_TYPES_H_

#include <algorithm>

#include "GLDefs.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"

/* Usage:
 * ===========
 *
 * To create a new type from a set of GLenums do the following:
 *
 *   STRONG_GLENUM_BEGIN(TypeName)
 *     STRONG_GLENUM_VALUE(ENUM1),
 *     STRONG_GLENUM_VALUE(ENUM2),
 *     ...
 *   STRONG_GLENUM_END()
 *
 * where TypeName is the name you want to give the type. Now simply use TypeName
 * instead of GLenum. The enum values must be given without GL_ prefix.
 *
 * ~~~~~~~~~~~~~~~~
 * Important Notes:
 * ~~~~~~~~~~~~~~~~
 *
 * Boolean operators (==, !=) are provided in an effort to prevent some mistakes
 * when using constants. For example we want to make sure that GL_ENUM_X is
 * a valid value for the type in code like:
 *
 *   if (myNewType == STRONG_GLENUM_VALUE(SOME_ENUM))
 *       ...
 *
 * The operators will assert that STRONG_GLENUM_VALUE(SOME_ENUM) is a value that
 * myNewType can have.
 *
 * ----
 *
 * A get() method is provided to allow access to the underlying GLenum. This
 * method should ideally only be called when passing parameters to the gl->fXXXX
 * functions, and be used very minimally everywhere else.
 *
 * Definitely XXX - DO NOT DO - XXX:
 *
 *   if (myNewType.get() == STRONG_GLENUM_VALUE(SOME_ENUM))
 *       ...
 *
 * As that undermines the debug checks that were implemented in the ==, and !=
 * operators. If you see code like this it should be treated with suspicion.
 *
 * Background:
 * ===========
 *
 * This macro is the first step in an effort to make the WebGL code safer.
 * Many of the different functions take GLenum as their parameter which leads
 * to bugs because of subtle differences in the enums purpose. For example there
 * are two types of 'texture targets'. One is the texture binding locations:
 *
 *   GL_TEXTURE_2D
 *   GL_TEXTURE_CUBE_MAP
 *
 * Yet, this is not the same as texture image targets:
 *
 *   GL_TEXTURE_2D
 *   GL_TEXTURE_CUBE_MAP_POSITIVE_X
 *   GL_TEXTURE_CUBE_MAP_NEGATIVE_X
 *   GL_TEXTURE_CUBE_MAP_POSITIVE_Y
 *   ...
 *
 * This subtle distinction has already led to many bugs in the texture code
 * because of invalid assumptions about what type goes where. The macro below
 * is an attempt at fixing this by providing a small wrapper around GLenum that
 * validates its values.
 *
 * Comparison between STRONG_GLENUM's vs. enum classes
 * ===================================================
 *
 * The present STRONG_GLENUM's differ from ordinary enum classes
 * in that they assert at runtime that their values are legal, and in that they
 * allow implicit conversion from integers to STRONG_GLENUM's but disallow
 * implicit conversion from STRONG_GLENUM's to integers (enum classes are the
 * opposite).
 *
 * When to use GLenum's vs. STRONG_GLENUM's vs. enum classes
 * =========================================================
 *
 * Rule of thumb:
 * * For unchecked GLenum constants, such as WebGL method parameters that
 *   haven't been validated yet, use GLenum.
 * * For already-validated GLenum constants, use STRONG_GLENUM's.
 * * For custom constants that aren't GL enum values, use enum classes.
 */

template<typename Details>
class StrongGLenum final
{
private:
    static const GLenum NonexistantGLenum = 0xdeaddead;

    GLenum mValue;

    static void AssertOnceThatEnumValuesAreSorted() {
#ifdef DEBUG
        static bool alreadyChecked = false;
        if (alreadyChecked) {
            return;
        }
        for (size_t i = 1; i < Details::valuesCount(); i++) {
            MOZ_ASSERT(Details::values()[i] > Details::values()[i - 1],
                       "GLenum values should be sorted in ascending order");
        }
        alreadyChecked = true;
#endif
    }

public:
    StrongGLenum()
#ifdef DEBUG
        : mValue(NonexistantGLenum)
#endif
    {
        AssertOnceThatEnumValuesAreSorted();
    }

    MOZ_IMPLICIT StrongGLenum(GLenum value)
        : mValue(value)
    {
        AssertOnceThatEnumValuesAreSorted();
        MOZ_ASSERT(IsValueLegal(mValue));
    }

    GLenum get() const {
        MOZ_ASSERT(mValue != NonexistantGLenum);
        return mValue;
    }

    bool operator==(const StrongGLenum& other) const {
        return get() == other.get();
    }

    bool operator!=(const StrongGLenum& other) const {
        return get() != other.get();
    }

    bool operator<=(const StrongGLenum& other) const {
        return get() <= other.get();
    }

    bool operator>=(const StrongGLenum& other) const {
        return get() >= other.get();
    }

    static bool IsValueLegal(GLenum value) {
        if (value > UINT16_MAX) {
            return false;
        }
        return std::binary_search(Details::values(),
                                  Details::values() + Details::valuesCount(),
                                  uint16_t(value));
    }
};

template<typename Details>
bool operator==(GLenum a, StrongGLenum<Details> b)
{
    return a == b.get();
}

template<typename Details>
bool operator!=(GLenum a, StrongGLenum<Details> b)
{
    return a != b.get();
}

template<typename Details>
bool operator==(StrongGLenum<Details> a, GLenum b)
{
    return a.get() == b;
}

template<typename Details>
bool operator!=(StrongGLenum<Details> a, GLenum b)
{
    return a.get() != b;
}

#define STRONG_GLENUM_BEGIN(NAME) \
    const uint16_t NAME##Values[] = {

#define STRONG_GLENUM_VALUE(VALUE) LOCAL_GL_##VALUE

#define STRONG_GLENUM_END(NAME) \
    }; \
    struct NAME##Details { \
        static size_t valuesCount() { return MOZ_ARRAY_LENGTH(NAME##Values); } \
        static const uint16_t* values() { return NAME##Values; } \
    }; \
    typedef StrongGLenum<NAME##Details> NAME;

////////////////////////////////////////////////////////////////////////////////
// Add types below.

STRONG_GLENUM_BEGIN(TexImageTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(TEXTURE_2D),
    STRONG_GLENUM_VALUE(TEXTURE_3D),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_X),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_X),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_Y),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_Y),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_POSITIVE_Z),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP_NEGATIVE_Z),
    STRONG_GLENUM_VALUE(TEXTURE_2D_ARRAY),
STRONG_GLENUM_END(TexImageTarget)

STRONG_GLENUM_BEGIN(TexTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(TEXTURE_2D),
    STRONG_GLENUM_VALUE(TEXTURE_3D),
    STRONG_GLENUM_VALUE(TEXTURE_CUBE_MAP),
    STRONG_GLENUM_VALUE(TEXTURE_2D_ARRAY),
STRONG_GLENUM_END(TexTarget)

STRONG_GLENUM_BEGIN(TexType)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(BYTE),
    STRONG_GLENUM_VALUE(UNSIGNED_BYTE),
    STRONG_GLENUM_VALUE(SHORT),
    STRONG_GLENUM_VALUE(UNSIGNED_SHORT),
    STRONG_GLENUM_VALUE(INT),
    STRONG_GLENUM_VALUE(UNSIGNED_INT),
    STRONG_GLENUM_VALUE(FLOAT),
    STRONG_GLENUM_VALUE(HALF_FLOAT),
    STRONG_GLENUM_VALUE(UNSIGNED_SHORT_4_4_4_4),
    STRONG_GLENUM_VALUE(UNSIGNED_SHORT_5_5_5_1),
    STRONG_GLENUM_VALUE(UNSIGNED_SHORT_5_6_5),
    STRONG_GLENUM_VALUE(UNSIGNED_INT_2_10_10_10_REV),
    STRONG_GLENUM_VALUE(UNSIGNED_INT_24_8),
    STRONG_GLENUM_VALUE(UNSIGNED_INT_10F_11F_11F_REV),
    STRONG_GLENUM_VALUE(UNSIGNED_INT_5_9_9_9_REV),
    STRONG_GLENUM_VALUE(HALF_FLOAT_OES),
    STRONG_GLENUM_VALUE(FLOAT_32_UNSIGNED_INT_24_8_REV),
STRONG_GLENUM_END(TexType)

STRONG_GLENUM_BEGIN(TexMinFilter)
    STRONG_GLENUM_VALUE(NEAREST),
    STRONG_GLENUM_VALUE(LINEAR),
    STRONG_GLENUM_VALUE(NEAREST_MIPMAP_NEAREST),
    STRONG_GLENUM_VALUE(LINEAR_MIPMAP_NEAREST),
    STRONG_GLENUM_VALUE(NEAREST_MIPMAP_LINEAR),
    STRONG_GLENUM_VALUE(LINEAR_MIPMAP_LINEAR),
STRONG_GLENUM_END(TexMinFilter)

STRONG_GLENUM_BEGIN(TexMagFilter)
    STRONG_GLENUM_VALUE(NEAREST),
    STRONG_GLENUM_VALUE(LINEAR),
STRONG_GLENUM_END(TexMagFilter)

STRONG_GLENUM_BEGIN(TexWrap)
    STRONG_GLENUM_VALUE(REPEAT),
    STRONG_GLENUM_VALUE(CLAMP_TO_EDGE),
    STRONG_GLENUM_VALUE(MIRRORED_REPEAT),
STRONG_GLENUM_END(TexWrap)

STRONG_GLENUM_BEGIN(TexCompareMode)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(COMPARE_REF_TO_TEXTURE),
STRONG_GLENUM_END(TexCompareMode)

STRONG_GLENUM_BEGIN(TexCompareFunc)
    STRONG_GLENUM_VALUE(NEVER),
    STRONG_GLENUM_VALUE(LESS),
    STRONG_GLENUM_VALUE(EQUAL),
    STRONG_GLENUM_VALUE(LEQUAL),
    STRONG_GLENUM_VALUE(GREATER),
    STRONG_GLENUM_VALUE(NOTEQUAL),
    STRONG_GLENUM_VALUE(GEQUAL),
    STRONG_GLENUM_VALUE(ALWAYS),
STRONG_GLENUM_END(TexCompareFunc)

STRONG_GLENUM_BEGIN(TexFormat)
    STRONG_GLENUM_VALUE(NONE),            // 0x0000
    STRONG_GLENUM_VALUE(DEPTH_COMPONENT), // 0x1902
    STRONG_GLENUM_VALUE(RED),             // 0x1903
    STRONG_GLENUM_VALUE(ALPHA),           // 0x1906
    STRONG_GLENUM_VALUE(RGB),             // 0x1907
    STRONG_GLENUM_VALUE(RGBA),            // 0x1908
    STRONG_GLENUM_VALUE(LUMINANCE),       // 0x1909
    STRONG_GLENUM_VALUE(LUMINANCE_ALPHA), // 0x190A
    STRONG_GLENUM_VALUE(RG),              // 0x8227
    STRONG_GLENUM_VALUE(RG_INTEGER),      // 0x8228
    STRONG_GLENUM_VALUE(DEPTH_STENCIL),   // 0x84F9
    STRONG_GLENUM_VALUE(SRGB),            // 0x8C40
    STRONG_GLENUM_VALUE(SRGB_ALPHA),      // 0x8C42
    STRONG_GLENUM_VALUE(RED_INTEGER),     // 0x8D94
    STRONG_GLENUM_VALUE(RGB_INTEGER),     // 0x8D98
    STRONG_GLENUM_VALUE(RGBA_INTEGER),    // 0x8D99
STRONG_GLENUM_END(TexFormat)

STRONG_GLENUM_BEGIN(TexInternalFormat)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(DEPTH_COMPONENT),
    STRONG_GLENUM_VALUE(RED),
    STRONG_GLENUM_VALUE(ALPHA),
    STRONG_GLENUM_VALUE(RGB),
    STRONG_GLENUM_VALUE(RGBA),
    STRONG_GLENUM_VALUE(LUMINANCE),
    STRONG_GLENUM_VALUE(LUMINANCE_ALPHA),
    STRONG_GLENUM_VALUE(ALPHA8),
    STRONG_GLENUM_VALUE(LUMINANCE8),
    STRONG_GLENUM_VALUE(LUMINANCE8_ALPHA8),
    STRONG_GLENUM_VALUE(RGB8),
    STRONG_GLENUM_VALUE(RGBA4),
    STRONG_GLENUM_VALUE(RGB5_A1),
    STRONG_GLENUM_VALUE(RGBA8),
    STRONG_GLENUM_VALUE(RGB10_A2),
    STRONG_GLENUM_VALUE(DEPTH_COMPONENT16),
    STRONG_GLENUM_VALUE(DEPTH_COMPONENT24),
    STRONG_GLENUM_VALUE(RG),
    STRONG_GLENUM_VALUE(RG_INTEGER),
    STRONG_GLENUM_VALUE(R8),
    STRONG_GLENUM_VALUE(RG8),
    STRONG_GLENUM_VALUE(R16F),
    STRONG_GLENUM_VALUE(R32F),
    STRONG_GLENUM_VALUE(RG16F),
    STRONG_GLENUM_VALUE(RG32F),
    STRONG_GLENUM_VALUE(R8I),
    STRONG_GLENUM_VALUE(R8UI),
    STRONG_GLENUM_VALUE(R16I),
    STRONG_GLENUM_VALUE(R16UI),
    STRONG_GLENUM_VALUE(R32I),
    STRONG_GLENUM_VALUE(R32UI),
    STRONG_GLENUM_VALUE(RG8I),
    STRONG_GLENUM_VALUE(RG8UI),
    STRONG_GLENUM_VALUE(RG16I),
    STRONG_GLENUM_VALUE(RG16UI),
    STRONG_GLENUM_VALUE(RG32I),
    STRONG_GLENUM_VALUE(RG32UI),
    STRONG_GLENUM_VALUE(COMPRESSED_RGB_S3TC_DXT1_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_RGBA_S3TC_DXT1_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_RGBA_S3TC_DXT3_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_RGBA_S3TC_DXT5_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_SRGB_S3TC_DXT1_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT),
    STRONG_GLENUM_VALUE(COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT),
    STRONG_GLENUM_VALUE(DEPTH_STENCIL),
    STRONG_GLENUM_VALUE(ATC_RGBA_INTERPOLATED_ALPHA),
    STRONG_GLENUM_VALUE(RGBA32F),
    STRONG_GLENUM_VALUE(RGB32F),
    STRONG_GLENUM_VALUE(ALPHA32F_EXT),
    STRONG_GLENUM_VALUE(LUMINANCE32F_EXT),
    STRONG_GLENUM_VALUE(LUMINANCE_ALPHA32F_EXT),
    STRONG_GLENUM_VALUE(RGBA16F),
    STRONG_GLENUM_VALUE(RGB16F),
    STRONG_GLENUM_VALUE(ALPHA16F_EXT),
    STRONG_GLENUM_VALUE(LUMINANCE16F_EXT),
    STRONG_GLENUM_VALUE(LUMINANCE_ALPHA16F_EXT),
    STRONG_GLENUM_VALUE(DEPTH24_STENCIL8),
    STRONG_GLENUM_VALUE(COMPRESSED_RGB_PVRTC_4BPPV1),
    STRONG_GLENUM_VALUE(COMPRESSED_RGB_PVRTC_2BPPV1),
    STRONG_GLENUM_VALUE(COMPRESSED_RGBA_PVRTC_4BPPV1),
    STRONG_GLENUM_VALUE(COMPRESSED_RGBA_PVRTC_2BPPV1),
    STRONG_GLENUM_VALUE(R11F_G11F_B10F),
    STRONG_GLENUM_VALUE(RGB9_E5),
    STRONG_GLENUM_VALUE(SRGB),
    STRONG_GLENUM_VALUE(SRGB8),
    STRONG_GLENUM_VALUE(SRGB_ALPHA),
    STRONG_GLENUM_VALUE(SRGB8_ALPHA8),
    STRONG_GLENUM_VALUE(ATC_RGB),
    STRONG_GLENUM_VALUE(ATC_RGBA_EXPLICIT_ALPHA),
    STRONG_GLENUM_VALUE(DEPTH_COMPONENT32F),
    STRONG_GLENUM_VALUE(DEPTH32F_STENCIL8),
    STRONG_GLENUM_VALUE(RGB565),
    STRONG_GLENUM_VALUE(ETC1_RGB8_OES),
    STRONG_GLENUM_VALUE(RGBA32UI),
    STRONG_GLENUM_VALUE(RGB32UI),
    STRONG_GLENUM_VALUE(RGBA16UI),
    STRONG_GLENUM_VALUE(RGB16UI),
    STRONG_GLENUM_VALUE(RGBA8UI),
    STRONG_GLENUM_VALUE(RGB8UI),
    STRONG_GLENUM_VALUE(RGBA32I),
    STRONG_GLENUM_VALUE(RGB32I),
    STRONG_GLENUM_VALUE(RGBA16I),
    STRONG_GLENUM_VALUE(RGB16I),
    STRONG_GLENUM_VALUE(RGBA8I),
    STRONG_GLENUM_VALUE(RGB8I),
    STRONG_GLENUM_VALUE(RED_INTEGER),
    STRONG_GLENUM_VALUE(RGB_INTEGER),
    STRONG_GLENUM_VALUE(RGBA_INTEGER),
    STRONG_GLENUM_VALUE(R8_SNORM),
    STRONG_GLENUM_VALUE(RG8_SNORM),
    STRONG_GLENUM_VALUE(RGB8_SNORM),
    STRONG_GLENUM_VALUE(RGBA8_SNORM),
    STRONG_GLENUM_VALUE(RGB10_A2UI),
STRONG_GLENUM_END(TexInternalFormat)

STRONG_GLENUM_BEGIN(FBTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(READ_FRAMEBUFFER),
    STRONG_GLENUM_VALUE(DRAW_FRAMEBUFFER),
    STRONG_GLENUM_VALUE(FRAMEBUFFER),
STRONG_GLENUM_END(FBTarget)

STRONG_GLENUM_BEGIN(RBTarget)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(RENDERBUFFER),
STRONG_GLENUM_END(RBTarget)

STRONG_GLENUM_BEGIN(FBStatus)
    STRONG_GLENUM_VALUE(FRAMEBUFFER_COMPLETE),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_INCOMPLETE_ATTACHMENT),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_INCOMPLETE_DIMENSIONS),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_INCOMPLETE_READ_BUFFER),
    STRONG_GLENUM_VALUE(FRAMEBUFFER_UNSUPPORTED),
STRONG_GLENUM_END(FBStatus)

STRONG_GLENUM_BEGIN(RBParam)
    STRONG_GLENUM_VALUE(RENDERBUFFER_SAMPLES),
    STRONG_GLENUM_VALUE(RENDERBUFFER_WIDTH),
    STRONG_GLENUM_VALUE(RENDERBUFFER_HEIGHT),
    STRONG_GLENUM_VALUE(RENDERBUFFER_INTERNAL_FORMAT),
    STRONG_GLENUM_VALUE(RENDERBUFFER_RED_SIZE),
    STRONG_GLENUM_VALUE(RENDERBUFFER_GREEN_SIZE),
    STRONG_GLENUM_VALUE(RENDERBUFFER_BLUE_SIZE),
    STRONG_GLENUM_VALUE(RENDERBUFFER_ALPHA_SIZE),
    STRONG_GLENUM_VALUE(RENDERBUFFER_DEPTH_SIZE),
    STRONG_GLENUM_VALUE(RENDERBUFFER_STENCIL_SIZE),
STRONG_GLENUM_END(RBParam)

STRONG_GLENUM_BEGIN(VAOBinding)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(VERTEX_ARRAY_BINDING),
STRONG_GLENUM_END(VAOBinding)

STRONG_GLENUM_BEGIN(BufferBinding)
    STRONG_GLENUM_VALUE(NONE),                      // 0x0000
    STRONG_GLENUM_VALUE(ARRAY_BUFFER),              // 0x8892
    STRONG_GLENUM_VALUE(ELEMENT_ARRAY_BUFFER),      // 0x8893
    STRONG_GLENUM_VALUE(PIXEL_PACK_BUFFER),         // 0x88EB
    STRONG_GLENUM_VALUE(PIXEL_UNPACK_BUFFER),       // 0x88EC
    STRONG_GLENUM_VALUE(UNIFORM_BUFFER),            // 0x8A11
    STRONG_GLENUM_VALUE(TRANSFORM_FEEDBACK_BUFFER), // 0x8C8E
STRONG_GLENUM_END(BufferBinding)

STRONG_GLENUM_BEGIN(QueryBinding)
    STRONG_GLENUM_VALUE(NONE),
    STRONG_GLENUM_VALUE(TIME_ELAPSED_EXT),
    STRONG_GLENUM_VALUE(TIMESTAMP_EXT),
STRONG_GLENUM_END(QueryBinding)

#endif // WEBGL_STRONG_TYPES_H_

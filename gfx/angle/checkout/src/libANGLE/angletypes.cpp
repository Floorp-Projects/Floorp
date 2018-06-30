//
// Copyright (c) 2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#include "libANGLE/angletypes.h"
#include "libANGLE/Program.h"
#include "libANGLE/VertexAttribute.h"
#include "libANGLE/State.h"
#include "libANGLE/VertexArray.h"

namespace gl
{

PrimitiveType GetPrimitiveType(GLenum drawMode)
{
    switch (drawMode)
    {
        case GL_POINTS:
            return PRIMITIVE_POINTS;
        case GL_LINES:
            return PRIMITIVE_LINES;
        case GL_LINE_STRIP:
            return PRIMITIVE_LINE_STRIP;
        case GL_LINE_LOOP:
            return PRIMITIVE_LINE_LOOP;
        case GL_TRIANGLES:
            return PRIMITIVE_TRIANGLES;
        case GL_TRIANGLE_STRIP:
            return PRIMITIVE_TRIANGLE_STRIP;
        case GL_TRIANGLE_FAN:
            return PRIMITIVE_TRIANGLE_FAN;
        default:
            UNREACHABLE();
            return PRIMITIVE_TYPE_MAX;
    }
}

RasterizerState::RasterizerState()
{
    memset(this, 0, sizeof(RasterizerState));

    rasterizerDiscard   = false;
    cullFace            = false;
    cullMode            = CullFaceMode::Back;
    frontFace           = GL_CCW;
    polygonOffsetFill   = false;
    polygonOffsetFactor = 0.0f;
    polygonOffsetUnits  = 0.0f;
    pointDrawMode       = false;
    multiSample         = false;
}

bool operator==(const RasterizerState &a, const RasterizerState &b)
{
    return memcmp(&a, &b, sizeof(RasterizerState)) == 0;
}

bool operator!=(const RasterizerState &a, const RasterizerState &b)
{
    return !(a == b);
}

BlendState::BlendState()
{
    memset(this, 0, sizeof(BlendState));

    blend                 = false;
    sourceBlendRGB        = GL_ONE;
    sourceBlendAlpha      = GL_ONE;
    destBlendRGB          = GL_ZERO;
    destBlendAlpha        = GL_ZERO;
    blendEquationRGB      = GL_FUNC_ADD;
    blendEquationAlpha    = GL_FUNC_ADD;
    sampleAlphaToCoverage = false;
    dither                = true;
}

BlendState::BlendState(const BlendState &other)
{
    memcpy(this, &other, sizeof(BlendState));
}

bool operator==(const BlendState &a, const BlendState &b)
{
    return memcmp(&a, &b, sizeof(BlendState)) == 0;
}

bool operator!=(const BlendState &a, const BlendState &b)
{
    return !(a == b);
}

DepthStencilState::DepthStencilState()
{
    memset(this, 0, sizeof(DepthStencilState));

    depthTest                = false;
    depthFunc                = GL_LESS;
    depthMask                = true;
    stencilTest              = false;
    stencilFunc              = GL_ALWAYS;
    stencilMask              = static_cast<GLuint>(-1);
    stencilWritemask         = static_cast<GLuint>(-1);
    stencilBackFunc          = GL_ALWAYS;
    stencilBackMask          = static_cast<GLuint>(-1);
    stencilBackWritemask     = static_cast<GLuint>(-1);
    stencilFail              = GL_KEEP;
    stencilPassDepthFail     = GL_KEEP;
    stencilPassDepthPass     = GL_KEEP;
    stencilBackFail          = GL_KEEP;
    stencilBackPassDepthFail = GL_KEEP;
    stencilBackPassDepthPass = GL_KEEP;
}

DepthStencilState::DepthStencilState(const DepthStencilState &other)
{
    memcpy(this, &other, sizeof(DepthStencilState));
}

bool operator==(const DepthStencilState &a, const DepthStencilState &b)
{
    return memcmp(&a, &b, sizeof(DepthStencilState)) == 0;
}

bool operator!=(const DepthStencilState &a, const DepthStencilState &b)
{
    return !(a == b);
}

SamplerState::SamplerState()
{
    memset(this, 0, sizeof(SamplerState));

    minFilter     = GL_NEAREST_MIPMAP_LINEAR;
    magFilter     = GL_LINEAR;
    wrapS         = GL_REPEAT;
    wrapT         = GL_REPEAT;
    wrapR         = GL_REPEAT;
    maxAnisotropy = 1.0f;
    minLod        = -1000.0f;
    maxLod        = 1000.0f;
    compareMode   = GL_NONE;
    compareFunc   = GL_LEQUAL;
    sRGBDecode    = GL_DECODE_EXT;
}

SamplerState::SamplerState(const SamplerState &other) = default;

// static
SamplerState SamplerState::CreateDefaultForTarget(TextureType type)
{
    SamplerState state;

    // According to OES_EGL_image_external and ARB_texture_rectangle: For external textures, the
    // default min filter is GL_LINEAR and the default s and t wrap modes are GL_CLAMP_TO_EDGE.
    if (type == TextureType::External || type == TextureType::Rectangle)
    {
        state.minFilter = GL_LINEAR;
        state.wrapS     = GL_CLAMP_TO_EDGE;
        state.wrapT     = GL_CLAMP_TO_EDGE;
    }

    return state;
}

ImageUnit::ImageUnit()
    : texture(), level(0), layered(false), layer(0), access(GL_READ_ONLY), format(GL_R32UI)
{
}

ImageUnit::ImageUnit(const ImageUnit &other) = default;

ImageUnit::~ImageUnit() = default;

static void MinMax(int a, int b, int *minimum, int *maximum)
{
    if (a < b)
    {
        *minimum = a;
        *maximum = b;
    }
    else
    {
        *minimum = b;
        *maximum = a;
    }
}

Rectangle Rectangle::removeReversal() const
{
    Rectangle unreversed = *this;
    if (isReversedX())
    {
        unreversed.x     = unreversed.x + unreversed.width;
        unreversed.width = -unreversed.width;
    }
    if (isReversedY())
    {
        unreversed.y      = unreversed.y + unreversed.height;
        unreversed.height = -unreversed.height;
    }
    return unreversed;
}

bool ClipRectangle(const Rectangle &source, const Rectangle &clip, Rectangle *intersection)
{
    int minSourceX, maxSourceX, minSourceY, maxSourceY;
    MinMax(source.x, source.x + source.width, &minSourceX, &maxSourceX);
    MinMax(source.y, source.y + source.height, &minSourceY, &maxSourceY);

    int minClipX, maxClipX, minClipY, maxClipY;
    MinMax(clip.x, clip.x + clip.width, &minClipX, &maxClipX);
    MinMax(clip.y, clip.y + clip.height, &minClipY, &maxClipY);

    if (minSourceX >= maxClipX || maxSourceX <= minClipX || minSourceY >= maxClipY || maxSourceY <= minClipY)
    {
        return false;
    }
    if (intersection)
    {
        intersection->x = std::max(minSourceX, minClipX);
        intersection->y = std::max(minSourceY, minClipY);
        intersection->width  = std::min(maxSourceX, maxClipX) - std::max(minSourceX, minClipX);
        intersection->height = std::min(maxSourceY, maxClipY) - std::max(minSourceY, minClipY);
    }
    return true;
}

bool Box::operator==(const Box &other) const
{
    return (x == other.x && y == other.y && z == other.z &&
            width == other.width && height == other.height && depth == other.depth);
}

bool Box::operator!=(const Box &other) const
{
    return !(*this == other);
}

bool operator==(const Offset &a, const Offset &b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool operator!=(const Offset &a, const Offset &b)
{
    return !(a == b);
}

bool operator==(const Extents &lhs, const Extents &rhs)
{
    return lhs.width == rhs.width && lhs.height == rhs.height && lhs.depth == rhs.depth;
}

bool operator!=(const Extents &lhs, const Extents &rhs)
{
    return !(lhs == rhs);
}

ComponentTypeMask::ComponentTypeMask()
{
    mTypeMask.reset();
}

ComponentTypeMask::ComponentTypeMask(const ComponentTypeMask &other) = default;

ComponentTypeMask::~ComponentTypeMask() = default;

void ComponentTypeMask::reset()
{
    mTypeMask.reset();
}

bool ComponentTypeMask::none()
{
    return mTypeMask.none();
}

void ComponentTypeMask::setIndex(GLenum type, size_t index)
{
    ASSERT(index <= MAX_COMPONENT_TYPE_MASK_INDEX);

    mTypeMask &= ~(0x10001 << index);

    uint32_t m = 0;
    switch (type)
    {
        case GL_INT:
            m = 0x00001;
            break;
        case GL_UNSIGNED_INT:
            m = 0x10000;
            break;
        case GL_FLOAT:
            m = 0x10001;
            break;
        case GL_NONE:
            m = 0x00000;
            break;
        default:
            UNREACHABLE();
    }

    mTypeMask |= m << index;
}

unsigned long ComponentTypeMask::to_ulong() const
{
    return mTypeMask.to_ulong();
}

void ComponentTypeMask::from_ulong(unsigned long mask)
{
    mTypeMask = angle::BitSet<MAX_COMPONENT_TYPE_MASK_INDEX * 2>(mask);
}

bool ComponentTypeMask::Validate(unsigned long outputTypes,
                                 unsigned long inputTypes,
                                 unsigned long outputMask,
                                 unsigned long inputMask)
{
    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS <= MAX_COMPONENT_TYPE_MASK_INDEX,
                  "Output/input masks should fit into 16 bits - 1 bit per draw buffer. The "
                  "corresponding type masks should fit into 32 bits - 2 bits per draw buffer.");
    static_assert(MAX_VERTEX_ATTRIBS <= MAX_COMPONENT_TYPE_MASK_INDEX,
                  "Output/input masks should fit into 16 bits - 1 bit per attrib. The "
                  "corresponding type masks should fit into 32 bits - 2 bits per attrib.");

    // For performance reasons, draw buffer and attribute type validation is done using bit masks.
    // We store two bits representing the type split, with the low bit in the lower 16 bits of the
    // variable, and the high bit in the upper 16 bits of the variable. This is done so we can AND
    // with the elswewhere used DrawBufferMask or AttributeMask.

    // OR the masks with themselves, shifted 16 bits. This is to match our split type bits.
    outputMask |= (outputMask << MAX_COMPONENT_TYPE_MASK_INDEX);
    inputMask |= (inputMask << MAX_COMPONENT_TYPE_MASK_INDEX);

    // To validate:
    // 1. Remove any indexes that are not enabled in the input (& inputMask)
    // 2. Remove any indexes that exist in output, but not in input (& outputMask)
    // 3. Use == to verify equality
    return (outputTypes & inputMask) == ((inputTypes & outputMask) & inputMask);
}

GLsizeiptr GetBoundBufferAvailableSize(const OffsetBindingPointer<Buffer> &binding)
{
    Buffer *buffer = binding.get();
    if (buffer)
    {
        if (binding.getSize() == 0)
            return static_cast<GLsizeiptr>(buffer->getSize());
        angle::CheckedNumeric<GLintptr> offset       = binding.getOffset();
        angle::CheckedNumeric<GLsizeiptr> size       = binding.getSize();
        angle::CheckedNumeric<GLsizeiptr> bufferSize = buffer->getSize();
        auto end                                     = offset + size;
        auto clampedSize                             = size;
        auto difference                              = end - bufferSize;
        if (!difference.IsValid())
        {
            return 0;
        }
        if (difference.ValueOrDie() > 0)
        {
            clampedSize = size - difference;
        }
        return clampedSize.ValueOrDefault(0);
    }
    else
    {
        return 0;
    }
}

}  // namespace gl

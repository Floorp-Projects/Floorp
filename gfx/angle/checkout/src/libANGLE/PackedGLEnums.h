// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PackedGLEnums_autogen.h:
//   Declares ANGLE-specific enums classes for GLEnum and functions operating
//   on them.

#ifndef LIBANGLE_PACKEDGLENUMS_H_
#define LIBANGLE_PACKEDGLENUMS_H_

#include "libANGLE/PackedGLEnums_autogen.h"

#include <array>
#include <bitset>
#include <cstddef>

#include <EGL/egl.h>

#include "common/bitset_utils.h"

namespace angle
{

// Return the number of elements of a packed enum, including the InvalidEnum element.
template <typename E>
constexpr size_t EnumSize()
{
    using UnderlyingType = typename std::underlying_type<E>::type;
    return static_cast<UnderlyingType>(E::EnumCount);
}

// Implementation of AllEnums which allows iterating over all the possible values for a packed enums
// like so:
//     for (auto value : AllEnums<MyPackedEnum>()) {
//         // Do something with the enum.
//     }

template <typename E>
class EnumIterator final
{
  private:
    using UnderlyingType = typename std::underlying_type<E>::type;

  public:
    EnumIterator(E value) : mValue(static_cast<UnderlyingType>(value)) {}
    EnumIterator &operator++()
    {
        mValue++;
        return *this;
    }
    bool operator==(const EnumIterator &other) const { return mValue == other.mValue; }
    bool operator!=(const EnumIterator &other) const { return mValue != other.mValue; }
    E operator*() const { return static_cast<E>(mValue); }

  private:
    UnderlyingType mValue;
};

template <typename E>
struct AllEnums
{
    EnumIterator<E> begin() const { return {static_cast<E>(0)}; }
    EnumIterator<E> end() const { return {E::InvalidEnum}; }
};

// PackedEnumMap<E, T> is like an std::array<T, E::EnumCount> but is indexed with enum values. It
// implements all of the std::array interface except with enum values instead of indices.
template <typename E, typename T>
class PackedEnumMap
{
  private:
    using UnderlyingType          = typename std::underlying_type<E>::type;
    using Storage                 = std::array<T, EnumSize<E>()>;

    Storage mData;

  public:
    // types:
    using value_type      = T;
    using pointer         = T *;
    using const_pointer   = const T *;
    using reference       = T &;
    using const_reference = const T &;

    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using iterator               = typename Storage::iterator;
    using const_iterator         = typename Storage::const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // No explicit construct/copy/destroy for aggregate type
    void fill(const T &u) { mData.fill(u); }
    void swap(PackedEnumMap<E, T> &a) noexcept { mData.swap(a.mData); }

    // iterators:
    iterator begin() noexcept { return mData.begin(); }
    const_iterator begin() const noexcept { return mData.begin(); }
    iterator end() noexcept { return mData.end(); }
    const_iterator end() const noexcept { return mData.end(); }

    reverse_iterator rbegin() noexcept { return mData.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return mData.rbegin(); }
    reverse_iterator rend() noexcept { return mData.rend(); }
    const_reverse_iterator rend() const noexcept { return mData.rend(); }

    // capacity:
    constexpr size_type size() const noexcept { return mData.size(); }
    constexpr size_type max_size() const noexcept { return mData.max_size(); }
    constexpr bool empty() const noexcept { return mData.empty(); }

    // element access:
    reference operator[](E n) { return mData[static_cast<UnderlyingType>(n)]; }
    const_reference operator[](E n) const { return mData[static_cast<UnderlyingType>(n)]; }
    const_reference at(E n) const { return mData.at(static_cast<UnderlyingType>(n)); }
    reference at(E n) { return mData.at(static_cast<UnderlyingType>(n)); }

    reference front() { return mData.front(); }
    const_reference front() const { return mData.front(); }
    reference back() { return mData.back(); }
    const_reference back() const { return mData.back(); }

    T *data() noexcept { return mData.data(); }
    const T *data() const noexcept { return mData.data(); }
};

// PackedEnumBitSetE> is like an std::bitset<E::EnumCount> but is indexed with enum values. It
// implements the std::bitset interface except with enum values instead of indices.
template <typename E>
using PackedEnumBitSet = BitSetT<EnumSize<E>(), uint32_t, E>;

}  // namespace angle

namespace gl
{

TextureType TextureTargetToType(TextureTarget target);
TextureTarget NonCubeTextureTypeToTarget(TextureType type);

TextureTarget CubeFaceIndexToTextureTarget(size_t face);
size_t CubeMapTextureTargetToFaceIndex(TextureTarget target);

constexpr TextureTarget kCubeMapTextureTargetMin = TextureTarget::CubeMapPositiveX;
constexpr TextureTarget kCubeMapTextureTargetMax = TextureTarget::CubeMapNegativeZ;
constexpr TextureTarget kAfterCubeMapTextureTargetMax =
    static_cast<TextureTarget>(static_cast<uint8_t>(kCubeMapTextureTargetMax) + 1);
struct AllCubeFaceTextureTargets
{
    angle::EnumIterator<TextureTarget> begin() const { return kCubeMapTextureTargetMin; }
    angle::EnumIterator<TextureTarget> end() const { return kAfterCubeMapTextureTargetMax; }
};

constexpr ShaderType kGLES2ShaderTypeMin = ShaderType::Vertex;
constexpr ShaderType kGLES2ShaderTypeMax = ShaderType::Fragment;
constexpr ShaderType kAfterGLES2ShaderTypeMax =
    static_cast<ShaderType>(static_cast<uint8_t>(kGLES2ShaderTypeMax) + 1);
struct AllGLES2ShaderTypes
{
    angle::EnumIterator<ShaderType> begin() const { return kGLES2ShaderTypeMin; }
    angle::EnumIterator<ShaderType> end() const { return kAfterGLES2ShaderTypeMax; }
};

constexpr ShaderType kShaderTypeMin = ShaderType::Vertex;
constexpr ShaderType kShaderTypeMax = ShaderType::Compute;
constexpr ShaderType kAfterShaderTypeMax =
    static_cast<ShaderType>(static_cast<uint8_t>(kShaderTypeMax) + 1);
struct AllShaderTypes
{
    angle::EnumIterator<ShaderType> begin() const { return kShaderTypeMin; }
    angle::EnumIterator<ShaderType> end() const { return kAfterShaderTypeMax; }
};

using ShaderBitSet = angle::PackedEnumBitSet<ShaderType>;

TextureType SamplerTypeToTextureType(GLenum samplerType);

}  // namespace gl

namespace egl_gl
{
gl::TextureTarget EGLCubeMapTargetToCubeMapTarget(EGLenum eglTarget);
gl::TextureTarget EGLImageTargetToTextureTarget(EGLenum eglTarget);
gl::TextureType EGLTextureTargetToTextureType(EGLenum eglTarget);
}  // namespace egl_gl

#endif  // LIBANGLE_PACKEDGLENUMS_H_

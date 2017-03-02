/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERMESSAGEUTILS_H
#define GFX_WEBRENDERMESSAGEUTILS_H

#include "chrome/common/ipc_message_utils.h"

#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::wr::ByteBuffer>
{
  typedef mozilla::wr::ByteBuffer paramType;

  static void
  Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mLength);
    aMsg->WriteBytes(aParam.mData, aParam.mLength);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, paramType* aResult)
  {
    size_t length;
    return ReadParam(aMsg, aIter, &length)
        && aResult->Allocate(length)
        && aMsg->ReadBytesInto(aIter, aResult->mData, length);
  }
};

template<>
struct ParamTraits<mozilla::wr::ImageKey>
{
  static void
  Write(Message* aMsg, const mozilla::wr::ImageKey& aParam)
  {
    WriteParam(aMsg, aParam.mNamespace);
    WriteParam(aMsg, aParam.mHandle);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::ImageKey* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mNamespace)
        && ReadParam(aMsg, aIter, &aResult->mHandle);
  }
};

template<>
struct ParamTraits<mozilla::wr::FontKey>
{
  static void
  Write(Message* aMsg, const mozilla::wr::FontKey& aParam)
  {
    WriteParam(aMsg, aParam.mNamespace);
    WriteParam(aMsg, aParam.mHandle);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::FontKey* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mNamespace)
        && ReadParam(aMsg, aIter, &aResult->mHandle);
  }
};

template<>
struct ParamTraits<mozilla::wr::PipelineId>
{
  static void
  Write(Message* aMsg, const mozilla::wr::PipelineId& aParam)
  {
    WriteParam(aMsg, aParam.mNamespace);
    WriteParam(aMsg, aParam.mHandle);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::PipelineId* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mNamespace)
        && ReadParam(aMsg, aIter, &aResult->mHandle);
  }
};

template<>
struct ParamTraits<WrImageFormat>
  : public ContiguousEnumSerializer<
        WrImageFormat,
        WrImageFormat::Invalid,
        WrImageFormat::Sentinel>
{
};

template<>
struct ParamTraits<WrBorderStyle>
  : public ContiguousEnumSerializer<
        WrBorderStyle,
        WrBorderStyle::None,
        WrBorderStyle::Sentinel>
{
};

template<>
struct ParamTraits<WrColor>
{
  static void
  Write(Message* aMsg, const WrColor& aParam)
  {
    WriteParam(aMsg, aParam.r);
    WriteParam(aMsg, aParam.g);
    WriteParam(aMsg, aParam.b);
    WriteParam(aMsg, aParam.a);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrColor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->r)
        && ReadParam(aMsg, aIter, &aResult->g)
        && ReadParam(aMsg, aIter, &aResult->b)
        && ReadParam(aMsg, aIter, &aResult->a);
  }
};

template<>
struct ParamTraits<WrGlyphInstance>
{
  static void
  Write(Message* aMsg, const WrGlyphInstance& aParam)
  {
    WriteParam(aMsg, aParam.index);
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrGlyphInstance* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->index)
        && ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y);
  }
};

template<>
struct ParamTraits<WrGlyphArray>
{
  static void
  Write(Message* aMsg, const WrGlyphArray& aParam)
  {
    WriteParam(aMsg, aParam.color);
    size_t length = aParam.glyphs.Length();

    WriteParam(aMsg, length);

    for (size_t i = 0; i < length; i++) {
      WriteParam(aMsg, aParam.glyphs[i]);
    }
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrGlyphArray* aResult)
  {
    if (!ReadParam(aMsg, aIter, &aResult->color)) {
      return false;
    }

    size_t length;
    if (!ReadParam(aMsg, aIter, &length)) {
      return false;
    }

    aResult->glyphs.SetLength(length);

    for (size_t i = 0; i < length; i++) {
      if (!ReadParam(aMsg, aIter, &aResult->glyphs[i])) {
        return false;
      }
    }

    return true;
  }
};

template<>
struct ParamTraits<WrGradientStop>
{
  static void
  Write(Message* aMsg, const WrGradientStop& aParam)
  {
    WriteParam(aMsg, aParam.offset);
    WriteParam(aMsg, aParam.color);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrGradientStop* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->offset)
        && ReadParam(aMsg, aIter, &aResult->color);
  }
};

template<>
struct ParamTraits<WrBorderSide>
{
  static void
  Write(Message* aMsg, const WrBorderSide& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.color);
    WriteParam(aMsg, aParam.style);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrBorderSide* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->color)
        && ReadParam(aMsg, aIter, &aResult->style);
  }
};

template<>
struct ParamTraits<WrLayoutSize>
{
  static void
  Write(Message* aMsg, const WrLayoutSize& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrLayoutSize* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<WrBorderRadius>
{
  static void
  Write(Message* aMsg, const WrBorderRadius& aParam)
  {
    WriteParam(aMsg, aParam.top_left);
    WriteParam(aMsg, aParam.top_right);
    WriteParam(aMsg, aParam.bottom_left);
    WriteParam(aMsg, aParam.bottom_right);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrBorderRadius* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->top_left)
        && ReadParam(aMsg, aIter, &aResult->top_right)
        && ReadParam(aMsg, aIter, &aResult->bottom_left)
        && ReadParam(aMsg, aIter, &aResult->bottom_right);
  }
};

template<>
struct ParamTraits<WrRect>
{
  static void
  Write(Message* aMsg, const WrRect& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrRect* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y)
        && ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<WrPoint>
{
  static void
  Write(Message* aMsg, const WrPoint& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrPoint* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x) &&
           ReadParam(aMsg, aIter, &aResult->y);
  }
};

template<>
struct ParamTraits<WrImageMask>
{
  static void
  Write(Message* aMsg, const WrImageMask& aParam)
  {
    WriteParam(aMsg, aParam.image);
    WriteParam(aMsg, aParam.rect);
    WriteParam(aMsg, aParam.repeat);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrImageMask* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->image)
        && ReadParam(aMsg, aIter, &aResult->rect)
        && ReadParam(aMsg, aIter, &aResult->repeat);
  }
};

template<>
struct ParamTraits<WrBuiltDisplayListDescriptor>
{
  static void
  Write(Message* aMsg, const WrBuiltDisplayListDescriptor& aParam)
  {
    WriteParam(aMsg, aParam.display_list_items_size);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrBuiltDisplayListDescriptor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->display_list_items_size);
  }
};

template<>
struct ParamTraits<WrAuxiliaryListsDescriptor>
{
  static void
  Write(Message* aMsg, const WrAuxiliaryListsDescriptor& aParam)
  {
    WriteParam(aMsg, aParam.gradient_stops_size);
    WriteParam(aMsg, aParam.complex_clip_regions_size);
    WriteParam(aMsg, aParam.filters_size);
    WriteParam(aMsg, aParam.glyph_instances_size);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrAuxiliaryListsDescriptor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->gradient_stops_size)
        && ReadParam(aMsg, aIter, &aResult->complex_clip_regions_size)
        && ReadParam(aMsg, aIter, &aResult->filters_size)
        && ReadParam(aMsg, aIter, &aResult->glyph_instances_size);
  }
};


template<>
struct ParamTraits<WrImageRendering>
  : public ContiguousEnumSerializer<
        WrImageRendering,
        WrImageRendering::Auto,
        WrImageRendering::Sentinel>
{
};

template<>
struct ParamTraits<WrMixBlendMode>
  : public ContiguousEnumSerializer<
        WrMixBlendMode,
        WrMixBlendMode::Normal,
        WrMixBlendMode::Sentinel>
{
};

template<>
struct ParamTraits<WrBoxShadowClipMode>
  : public ContiguousEnumSerializer<
        WrBoxShadowClipMode,
        WrBoxShadowClipMode::None,
        WrBoxShadowClipMode::Sentinel>
{
};

template<>
struct ParamTraits<WrGradientExtendMode>
  : public ContiguousEnumSerializer<
        WrGradientExtendMode,
        WrGradientExtendMode::Clamp,
        WrGradientExtendMode::Sentinel>
{
};

} // namespace IPC

#endif // GFX_WEBRENDERMESSAGEUTILS_H

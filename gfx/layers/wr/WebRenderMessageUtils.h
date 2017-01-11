/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERMESSAGEUTILS_H
#define GFX_WEBRENDERMESSAGEUTILS_H

#include "chrome/common/ipc_message_utils.h"

#include "mozilla/gfx/webrender.h"
#include "mozilla/layers/WebRenderTypes.h"

namespace IPC {

template<>
struct ParamTraits<mozilla::gfx::ByteBuffer>
{
  typedef mozilla::gfx::ByteBuffer paramType;

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
struct ParamTraits<WRImageFormat>
  : public ContiguousEnumSerializer<
        WRImageFormat,
        WRImageFormat::Invalid,
        WRImageFormat::RGBAF32>
{
};

template<>
struct ParamTraits<WRImageKey>
{
  static void
  Write(Message* aMsg, const WRImageKey& aParam)
  {
    WriteParam(aMsg, aParam.a);
    WriteParam(aMsg, aParam.b);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRImageKey* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->a)
        && ReadParam(aMsg, aIter, &aResult->b);
  }
};

template<>
struct ParamTraits<WRBorderStyle>
  : public ContiguousEnumSerializer<
        WRBorderStyle,
        WRBorderStyle::None,
        WRBorderStyle::Outset>
{
};

template<>
struct ParamTraits<WRColor>
{
  static void
  Write(Message* aMsg, const WRColor& aParam)
  {
    WriteParam(aMsg, aParam.r);
    WriteParam(aMsg, aParam.g);
    WriteParam(aMsg, aParam.b);
    WriteParam(aMsg, aParam.a);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRColor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->r)
        && ReadParam(aMsg, aIter, &aResult->g)
        && ReadParam(aMsg, aIter, &aResult->b)
        && ReadParam(aMsg, aIter, &aResult->a);
  }
};

template<>
struct ParamTraits<WRGlyphInstance>
{
  static void
  Write(Message* aMsg, const WRGlyphInstance& aParam)
  {
    WriteParam(aMsg, aParam.index);
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRGlyphInstance* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->index)
        && ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y);
  }
};

template<>
struct ParamTraits<WRGlyphArray>
{
  static void
  Write(Message* aMsg, const WRGlyphArray& aParam)
  {
    WriteParam(aMsg, aParam.color);
    size_t length = aParam.glyphs.Length();

    WriteParam(aMsg, length);

    for (size_t i = 0; i < length; i++) {
      WriteParam(aMsg, aParam.glyphs[i]);
    }
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRGlyphArray* aResult)
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
struct ParamTraits<WRBorderSide>
{
  static void
  Write(Message* aMsg, const WRBorderSide& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.color);
    WriteParam(aMsg, aParam.style);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRBorderSide* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->color)
        && ReadParam(aMsg, aIter, &aResult->style);
  }
};

template<>
struct ParamTraits<WRLayoutSize>
{
  static void
  Write(Message* aMsg, const WRLayoutSize& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRLayoutSize* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<WRRect>
{
  static void
  Write(Message* aMsg, const WRRect& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRRect* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y)
        && ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<WRImageMask>
{
  static void
  Write(Message* aMsg, const WRImageMask& aParam)
  {
    WriteParam(aMsg, aParam.image);
    WriteParam(aMsg, aParam.rect);
    WriteParam(aMsg, aParam.repeat);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WRImageMask* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->image)
        && ReadParam(aMsg, aIter, &aResult->rect)
        && ReadParam(aMsg, aIter, &aResult->repeat);
  }
};

} // namespace IPC

#endif // GFX_WEBRENDERMESSAGEUTILS_H

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
struct ParamTraits<mozilla::wr::ExternalImageId>
{
  static void
  Write(Message* aMsg, const mozilla::wr::ExternalImageId& aParam)
  {
    WriteParam(aMsg, aParam.mHandle);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::ExternalImageId* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mHandle);
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
struct ParamTraits<WrSize>
{
  static void
  Write(Message* aMsg, const WrSize& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrSize* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
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
struct ParamTraits<WrBuiltDisplayListDescriptor>
{
  static void
  Write(Message* aMsg, const WrBuiltDisplayListDescriptor& aParam)
  {
    WriteParam(aMsg, aParam.display_list_items_size);
    WriteParam(aMsg, aParam.builder_start_time);
    WriteParam(aMsg, aParam.builder_finish_time);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, WrBuiltDisplayListDescriptor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->display_list_items_size)
        && ReadParam(aMsg, aIter, &aResult->builder_start_time)
        && ReadParam(aMsg, aIter, &aResult->builder_finish_time);
  }
};

} // namespace IPC

#endif // GFX_WEBRENDERMESSAGEUTILS_H

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
struct ParamTraits<mozilla::wr::WrImageFormat>
  : public ContiguousEnumSerializer<
        mozilla::wr::WrImageFormat,
        mozilla::wr::WrImageFormat::Invalid,
        mozilla::wr::WrImageFormat::Sentinel>
{
};

template<>
struct ParamTraits<mozilla::wr::WrSize>
{
  static void
  Write(Message* aMsg, const mozilla::wr::WrSize& aParam)
  {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::WrSize* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<mozilla::wr::WrRect>
{
  static void
  Write(Message* aMsg, const mozilla::wr::WrRect& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::WrRect* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x)
        && ReadParam(aMsg, aIter, &aResult->y)
        && ReadParam(aMsg, aIter, &aResult->width)
        && ReadParam(aMsg, aIter, &aResult->height);
  }
};

template<>
struct ParamTraits<mozilla::wr::WrPoint>
{
  static void
  Write(Message* aMsg, const mozilla::wr::WrPoint& aParam)
  {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::WrPoint* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->x) &&
           ReadParam(aMsg, aIter, &aResult->y);
  }
};

template<>
struct ParamTraits<mozilla::wr::WrImageMask>
{
  static void
  Write(Message* aMsg, const mozilla::wr::WrImageMask& aParam)
  {
    WriteParam(aMsg, aParam.image);
    WriteParam(aMsg, aParam.rect);
    WriteParam(aMsg, aParam.repeat);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::WrImageMask* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->image)
        && ReadParam(aMsg, aIter, &aResult->rect)
        && ReadParam(aMsg, aIter, &aResult->repeat);
  }
};

template<>
struct ParamTraits<mozilla::wr::WrImageRendering>
  : public ContiguousEnumSerializer<
        mozilla::wr::WrImageRendering,
        mozilla::wr::WrImageRendering::Auto,
        mozilla::wr::WrImageRendering::Sentinel>
{
};

template<>
struct ParamTraits<mozilla::wr::WrMixBlendMode>
  : public ContiguousEnumSerializer<
        mozilla::wr::WrMixBlendMode,
        mozilla::wr::WrMixBlendMode::Normal,
        mozilla::wr::WrMixBlendMode::Sentinel>
{
};

template<>
struct ParamTraits<mozilla::wr::WrBuiltDisplayListDescriptor>
{
  static void
  Write(Message* aMsg, const mozilla::wr::WrBuiltDisplayListDescriptor& aParam)
  {
    WriteParam(aMsg, aParam.builder_start_time);
    WriteParam(aMsg, aParam.builder_finish_time);
  }

  static bool
  Read(const Message* aMsg, PickleIterator* aIter, mozilla::wr::WrBuiltDisplayListDescriptor* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->builder_start_time)
        && ReadParam(aMsg, aIter, &aResult->builder_finish_time);
  }
};

} // namespace IPC

#endif // GFX_WEBRENDERMESSAGEUTILS_H

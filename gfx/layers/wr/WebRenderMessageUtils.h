/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERMESSAGEUTILS_H
#define GFX_WEBRENDERMESSAGEUTILS_H

#include "chrome/common/ipc_message_utils.h"

#include "ipc/IPCMessageUtils.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::wr::ByteBuffer> {
  typedef mozilla::wr::ByteBuffer paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mLength);
    aMsg->WriteBytes(aParam.mData, aParam.mLength);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    size_t length;
    return ReadParam(aMsg, aIter, &length) && aResult->Allocate(length) &&
           aMsg->ReadBytesInto(aIter, aResult->mData, length);
  }
};

template <>
struct ParamTraits<mozilla::wr::ImageDescriptor> {
  typedef mozilla::wr::ImageDescriptor paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.format);
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
    WriteParam(aMsg, aParam.stride);
    WriteParam(aMsg, aParam.opacity);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->format) &&
           ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height) &&
           ReadParam(aMsg, aIter, &aResult->stride) &&
           ReadParam(aMsg, aIter, &aResult->opacity);
  }
};

template <>
struct ParamTraits<mozilla::wr::IdNamespace>
    : public PlainOldDataSerializer<mozilla::wr::IdNamespace> {};

template <>
struct ParamTraits<mozilla::wr::ImageKey>
    : public PlainOldDataSerializer<mozilla::wr::ImageKey> {};

template <>
struct ParamTraits<mozilla::wr::BlobImageKey>
    : public PlainOldDataSerializer<mozilla::wr::BlobImageKey> {};

template <>
struct ParamTraits<mozilla::wr::FontKey>
    : public PlainOldDataSerializer<mozilla::wr::FontKey> {};

template <>
struct ParamTraits<mozilla::wr::FontInstanceKey>
    : public PlainOldDataSerializer<mozilla::wr::FontInstanceKey> {};

template <>
struct ParamTraits<mozilla::wr::FontInstanceOptions>
    : public PlainOldDataSerializer<mozilla::wr::FontInstanceOptions> {};

template <>
struct ParamTraits<mozilla::wr::FontInstancePlatformOptions>
    : public PlainOldDataSerializer<mozilla::wr::FontInstancePlatformOptions> {
};

template <>
struct ParamTraits<mozilla::wr::ExternalImageId>
    : public PlainOldDataSerializer<mozilla::wr::ExternalImageId> {};

template <>
struct ParamTraits<mozilla::wr::PipelineId>
    : public PlainOldDataSerializer<mozilla::wr::PipelineId> {};

template <>
struct ParamTraits<mozilla::wr::ImageFormat>
    : public ContiguousEnumSerializer<mozilla::wr::ImageFormat,
                                      mozilla::wr::ImageFormat::R8,
                                      mozilla::wr::ImageFormat::Sentinel> {};

template <>
struct ParamTraits<mozilla::wr::LayoutSize>
    : public PlainOldDataSerializer<mozilla::wr::LayoutSize> {};

template <>
struct ParamTraits<mozilla::wr::LayoutRect>
    : public PlainOldDataSerializer<mozilla::wr::LayoutRect> {};

template <>
struct ParamTraits<mozilla::wr::LayoutPoint>
    : public PlainOldDataSerializer<mozilla::wr::LayoutPoint> {};

template <>
struct ParamTraits<mozilla::wr::RenderRoot>
    : public ContiguousEnumSerializerInclusive<
          mozilla::wr::RenderRoot, mozilla::wr::RenderRoot::Default,
          mozilla::wr::kHighestRenderRoot> {};

template <>
struct ParamTraits<mozilla::wr::ImageRendering>
    : public ContiguousEnumSerializer<mozilla::wr::ImageRendering,
                                      mozilla::wr::ImageRendering::Auto,
                                      mozilla::wr::ImageRendering::Sentinel> {};

template <>
struct ParamTraits<mozilla::wr::MixBlendMode>
    : public ContiguousEnumSerializer<mozilla::wr::MixBlendMode,
                                      mozilla::wr::MixBlendMode::Normal,
                                      mozilla::wr::MixBlendMode::Sentinel> {};

template <>
struct ParamTraits<mozilla::wr::BuiltDisplayListDescriptor>
    : public PlainOldDataSerializer<mozilla::wr::BuiltDisplayListDescriptor> {};

template <>
struct ParamTraits<mozilla::wr::WebRenderError>
    : public ContiguousEnumSerializer<mozilla::wr::WebRenderError,
                                      mozilla::wr::WebRenderError::INITIALIZE,
                                      mozilla::wr::WebRenderError::Sentinel> {};

template <>
struct ParamTraits<mozilla::wr::MemoryReport>
    : public PlainOldDataSerializer<mozilla::wr::MemoryReport> {};

template <>
struct ParamTraits<mozilla::wr::OpacityType>
    : public PlainOldDataSerializer<mozilla::wr::OpacityType> {};

template <>
struct ParamTraits<mozilla::wr::ExternalImageKeyPair>
    : public PlainOldDataSerializer<mozilla::wr::ExternalImageKeyPair> {};

}  // namespace IPC

#endif  // GFX_WEBRENDERMESSAGEUTILS_H

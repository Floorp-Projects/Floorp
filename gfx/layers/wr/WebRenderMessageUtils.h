/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_WEBRENDERMESSAGEUTILS_H
#define GFX_WEBRENDERMESSAGEUTILS_H

#include "chrome/common/ipc_message_utils.h"

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "mozilla/webrender/webrender_ffi.h"
#include "mozilla/webrender/WebRenderTypes.h"
#include "mozilla/dom/MediaIPCUtils.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::wr::ByteBuffer> {
  typedef mozilla::wr::ByteBuffer paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mLength);
    aWriter->WriteBytes(aParam.mData, aParam.mLength);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    size_t length;
    return ReadParam(aReader, &length) && aResult->Allocate(length) &&
           aReader->ReadBytesInto(aResult->mData, length);
  }
};

template <>
struct ParamTraits<mozilla::wr::ImageDescriptor> {
  typedef mozilla::wr::ImageDescriptor paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.format);
    WriteParam(aWriter, aParam.width);
    WriteParam(aWriter, aParam.height);
    WriteParam(aWriter, aParam.stride);
    WriteParam(aWriter, aParam.opacity);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->format) &&
           ReadParam(aReader, &aResult->width) &&
           ReadParam(aReader, &aResult->height) &&
           ReadParam(aReader, &aResult->stride) &&
           ReadParam(aReader, &aResult->opacity);
  }
};

template <>
struct ParamTraits<mozilla::wr::GeckoDisplayListType::Tag>
    : public ContiguousEnumSerializer<
          mozilla::wr::GeckoDisplayListType::Tag,
          mozilla::wr::GeckoDisplayListType::Tag::None,
          mozilla::wr::GeckoDisplayListType::Tag::Sentinel> {};

template <>
struct ParamTraits<mozilla::wr::GeckoDisplayListType> {
  typedef mozilla::wr::GeckoDisplayListType paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.tag);
    switch (aParam.tag) {
      case mozilla::wr::GeckoDisplayListType::Tag::None:
        break;
      case mozilla::wr::GeckoDisplayListType::Tag::Partial:
        WriteParam(aWriter, aParam.partial._0);
        break;
      case mozilla::wr::GeckoDisplayListType::Tag::Full:
        WriteParam(aWriter, aParam.full._0);
        break;
      default:
        MOZ_RELEASE_ASSERT(false, "bad enum variant");
    }
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    if (!ReadParam(aReader, &aResult->tag)) {
      return false;
    }
    switch (aResult->tag) {
      case mozilla::wr::GeckoDisplayListType::Tag::None:
        return true;
      case mozilla::wr::GeckoDisplayListType::Tag::Partial:
        return ReadParam(aReader, &aResult->partial._0);
      case mozilla::wr::GeckoDisplayListType::Tag::Full:
        return ReadParam(aReader, &aResult->full._0);
      default:
        MOZ_RELEASE_ASSERT(false, "bad enum variant");
    }
  }
};

template <>
struct ParamTraits<mozilla::wr::BuiltDisplayListDescriptor> {
  typedef mozilla::wr::BuiltDisplayListDescriptor paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.gecko_display_list_type);
    WriteParam(aWriter, aParam.builder_start_time);
    WriteParam(aWriter, aParam.builder_finish_time);
    WriteParam(aWriter, aParam.send_start_time);
    WriteParam(aWriter, aParam.total_clip_nodes);
    WriteParam(aWriter, aParam.total_spatial_nodes);
    WriteParam(aWriter, aParam.cache_size);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->gecko_display_list_type) &&
           ReadParam(aReader, &aResult->builder_start_time) &&
           ReadParam(aReader, &aResult->builder_finish_time) &&
           ReadParam(aReader, &aResult->send_start_time) &&
           ReadParam(aReader, &aResult->total_clip_nodes) &&
           ReadParam(aReader, &aResult->total_spatial_nodes) &&
           ReadParam(aReader, &aResult->cache_size);
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

template <>
struct ParamTraits<mozilla::wr::RenderReasons>
    : public PlainOldDataSerializer<mozilla::wr::RenderReasons> {};

}  // namespace IPC

#endif  // GFX_WEBRENDERMESSAGEUTILS_H

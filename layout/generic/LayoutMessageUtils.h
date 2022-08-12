/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_
#define LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"
#include "nsIFrame.h"
#include "mozilla/AspectRatio.h"
#include "mozilla/webrender/WebRenderTypes.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::IntrinsicSize> {
  using paramType = mozilla::IntrinsicSize;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.width);
    WriteParam(aWriter, aParam.height);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->width) &&
           ReadParam(aReader, &aResult->height);
  }
};

template <>
struct ParamTraits<mozilla::AspectRatio> {
  using paramType = mozilla::AspectRatio;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mRatio);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mRatio);
  }
};

template <>
struct ParamTraits<mozilla::StyleImageRendering>
    : public ContiguousEnumSerializerInclusive<
          mozilla::StyleImageRendering, mozilla::StyleImageRendering::Auto,
          mozilla::StyleImageRendering::Optimizequality> {};

}  // namespace IPC

#endif  // LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_

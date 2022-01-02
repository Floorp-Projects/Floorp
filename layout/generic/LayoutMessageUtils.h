/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_
#define LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_

#include "ipc/IPCMessageUtils.h"
#include "nsIFrame.h"
#include "mozilla/AspectRatio.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::IntrinsicSize> {
  using paramType = mozilla::IntrinsicSize;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.width);
    WriteParam(aMsg, aParam.height);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->width) &&
           ReadParam(aMsg, aIter, &aResult->height);
  }
};

template <>
struct ParamTraits<mozilla::AspectRatio> {
  using paramType = mozilla::AspectRatio;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mRatio);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mRatio);
  }
};

}  // namespace IPC

#endif  // LAYOUT_GENERIC_LAYOUTMESSAGEUTILS_H_

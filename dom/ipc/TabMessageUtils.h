/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TABMESSAGE_UTILS_H
#define TABMESSAGE_UTILS_H

#include "ipc/EnumSerializer.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/Event.h"
#include "nsIRemoteTab.h"
#include "nsPIDOMWindow.h"
#include "nsCOMPtr.h"
#include "mozilla/dom/EffectsInfo.h"
#include "mozilla/layers/LayersMessageUtils.h"
#include "TabMessageTypes.h"
#include "X11UndefineNone.h"

namespace mozilla::dom {

bool ReadRemoteEvent(const IPC::Message* aMsg, PickleIterator* aIter,
                     mozilla::dom::RemoteDOMEvent* aResult);

}  // namespace mozilla::dom

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::RemoteDOMEvent> {
  typedef mozilla::dom::RemoteDOMEvent paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    aParam.mEvent->Serialize(aMsg, true);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return mozilla::dom::ReadRemoteEvent(aMsg, aIter, aResult);
  }

  static void Log(const paramType& aParam, std::wstring* aLog) {}
};

template <>
struct ParamTraits<nsSizeMode>
    : public ContiguousEnumSerializer<nsSizeMode, nsSizeMode_Normal,
                                      nsSizeMode_Invalid> {};

template <>
struct ParamTraits<UIStateChangeType>
    : public ContiguousEnumSerializer<UIStateChangeType,
                                      UIStateChangeType_NoChange,
                                      UIStateChangeType_Invalid> {};

template <>
struct ParamTraits<nsIRemoteTab::NavigationType>
    : public ContiguousEnumSerializerInclusive<
          nsIRemoteTab::NavigationType,
          nsIRemoteTab::NavigationType::NAVIGATE_BACK,
          nsIRemoteTab::NavigationType::NAVIGATE_URL> {};

template <>
struct ParamTraits<mozilla::dom::EffectsInfo> {
  typedef mozilla::dom::EffectsInfo paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mVisibleRect);
    WriteParam(aMsg, aParam.mScaleX);
    WriteParam(aMsg, aParam.mScaleY);
    WriteParam(aMsg, aParam.mTransformToAncestorScale);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mVisibleRect) &&
           ReadParam(aMsg, aIter, &aResult->mScaleX) &&
           ReadParam(aMsg, aIter, &aResult->mScaleY) &&
           ReadParam(aMsg, aIter, &aResult->mTransformToAncestorScale);
  }
};

template <>
struct ParamTraits<mozilla::WhenToScroll>
    : public ContiguousEnumSerializerInclusive<
          mozilla::WhenToScroll, mozilla::WhenToScroll::Always,
          mozilla::WhenToScroll::IfNotFullyVisible> {};

template <>
struct ParamTraits<mozilla::ScrollFlags>
    : public BitFlagsEnumSerializer<mozilla::ScrollFlags,
                                    mozilla::ScrollFlags::ALL_BITS> {};

template <>
struct ParamTraits<mozilla::ScrollAxis> {
  typedef mozilla::ScrollAxis paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mWhereToScroll);
    WriteParam(aMsg, aParam.mWhenToScroll);
    WriteParam(aMsg, aParam.mOnlyIfPerceivedScrollableDirection);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    if (!ReadParam(aMsg, aIter, &aResult->mWhereToScroll)) {
      return false;
    }
    if (!ReadParam(aMsg, aIter, &aResult->mWhenToScroll)) {
      return false;
    }

    // We can't set mOnlyIfPerceivedScrollableDirection directly since it's
    // a bitfield.
    bool value;
    if (!ReadParam(aMsg, aIter, &value)) {
      return false;
    }
    aResult->mOnlyIfPerceivedScrollableDirection = value;

    return true;
  }
};

template <>
struct ParamTraits<mozilla::dom::EmbedderElementEventType>
    : public ContiguousEnumSerializer<
          mozilla::dom::EmbedderElementEventType,
          mozilla::dom::EmbedderElementEventType::NoEvent,
          mozilla::dom::EmbedderElementEventType::EndGuard_> {};

}  // namespace IPC

#endif  // TABMESSAGE_UTILS_H

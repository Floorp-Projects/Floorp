/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EMEUtils.h"

#include "jsfriendapi.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/dom/KeySystemNames.h"
#include "mozilla/dom/UnionTypes.h"

namespace mozilla {

LogModule* GetEMELog() {
  static LazyLogModule log("EME");
  return log;
}

LogModule* GetEMEVerboseLog() {
  static LazyLogModule log("EMEV");
  return log;
}

ArrayData GetArrayBufferViewOrArrayBufferData(
    const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView) {
  MOZ_ASSERT(aBufferOrView.IsArrayBuffer() ||
             aBufferOrView.IsArrayBufferView());
  JS::AutoCheckCannotGC nogc;
  if (aBufferOrView.IsArrayBuffer()) {
    const dom::ArrayBuffer& buffer = aBufferOrView.GetAsArrayBuffer();
    buffer.ComputeState();
    return ArrayData(buffer.Data(), buffer.Length());
  } else if (aBufferOrView.IsArrayBufferView()) {
    const dom::ArrayBufferView& bufferview =
        aBufferOrView.GetAsArrayBufferView();
    bufferview.ComputeState();
    return ArrayData(bufferview.Data(), bufferview.Length());
  }
  return ArrayData(nullptr, 0);
}

void CopyArrayBufferViewOrArrayBufferData(
    const dom::ArrayBufferViewOrArrayBuffer& aBufferOrView,
    nsTArray<uint8_t>& aOutData) {
  JS::AutoCheckCannotGC nogc;
  ArrayData data = GetArrayBufferViewOrArrayBufferData(aBufferOrView);
  aOutData.Clear();
  if (!data.IsValid()) {
    return;
  }
  aOutData.AppendElements(data.mData, data.mLength);
}

bool IsClearkeyKeySystem(const nsAString& aKeySystem) {
  if (StaticPrefs::media_clearkey_test_key_systems_enabled()) {
    return aKeySystem.EqualsLiteral(kClearKeyKeySystemName) ||
           aKeySystem.EqualsLiteral(kClearKeyWithProtectionQueryKeySystemName);
  }
  return aKeySystem.EqualsLiteral(kClearKeyKeySystemName);
}

bool IsWidevineKeySystem(const nsAString& aKeySystem) {
  return aKeySystem.EqualsLiteral(kWidevineKeySystemName);
}

nsString KeySystemToGMPName(const nsAString& aKeySystem) {
  if (IsClearkeyKeySystem(aKeySystem)) {
    return u"gmp-clearkey"_ns;
  }
  if (IsWidevineKeySystem(aKeySystem)) {
    return u"gmp-widevinecdm"_ns;
  }
  MOZ_ASSERT(false, "We should only call this for known GMPs");
  return u""_ns;
}

}  // namespace mozilla

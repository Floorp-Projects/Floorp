/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_OriginTrialsIPCUtils_h
#define mozilla_OriginTrialsIPCUtils_h

#include "mozilla/OriginTrials.h"
#include "mozilla/EnumTypeTraits.h"
#include "ipc/EnumSerializer.h"

namespace mozilla {
template <>
struct MaxEnumValue<OriginTrial> {
  static constexpr unsigned int value =
      static_cast<unsigned int>(OriginTrial::MAX);
};
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::OriginTrials> {
  using paramType = mozilla::OriginTrials;
  using RawType = mozilla::OriginTrials::RawType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.Raw());
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    RawType raw;
    if (!ReadParam(aReader, &raw)) {
      return false;
    }
    *aResult = mozilla::OriginTrials::FromRaw(raw);
    return true;
  }
};

}  // namespace IPC

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_Tainting_h
#define mozilla_ipc_Tainting_h

#include "mozilla/Tainting.h"

#include "base/basictypes.h"
#include "base/process.h"

#include "mozilla/ipc/IPDLParamTraits.h"

namespace mozilla {
namespace ipc {

template <typename T>
struct IPDLParamTraits<mozilla::Tainted<T>> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const mozilla::Tainted<T>& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mValue);
  }

  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    mozilla::Tainted<T>&& aParam) {
    WriteIPDLParam(aWriter, aActor, std::move(aParam.mValue));
  }

  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   mozilla::Tainted<T>* aResult) {
    return ReadIPDLParam(aReader, aActor, &(aResult->mValue));
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // ifndef mozilla_ipc_Tainting_h

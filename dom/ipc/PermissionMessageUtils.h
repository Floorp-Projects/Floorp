/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_permission_message_utils_h__
#define mozilla_dom_permission_message_utils_h__

#include "mozilla/ipc/IPDLParamTraits.h"
#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"

namespace IPC {

/**
 * Legacy IPC::Principal type. Use nsIPrincipal directly in new IPDL code.
 */
class Principal {
  friend struct mozilla::ipc::IPDLParamTraits<Principal>;

 public:
  Principal() = default;

  explicit Principal(nsIPrincipal* aPrincipal) : mPrincipal(aPrincipal) {}
  Principal(Principal&&) = default;

  Principal& operator=(Principal&& aOther) = default;
  Principal& operator=(const Principal& aOther) = delete;

  operator nsIPrincipal*() const { return mPrincipal.get(); }

 private:
  RefPtr<nsIPrincipal> mPrincipal;
};

}  // namespace IPC

namespace mozilla::ipc {

template <>
struct IPDLParamTraits<nsIPrincipal*> {
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    nsIPrincipal* aParam);
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   RefPtr<nsIPrincipal>* aResult);

  // Overload to support deserializing nsCOMPtr<nsIPrincipal> directly.
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   nsCOMPtr<nsIPrincipal>* aResult) {
    RefPtr<nsIPrincipal> result;
    if (!Read(aReader, aActor, &result)) {
      return false;
    }
    *aResult = std::move(result);
    return true;
  }
};

template <>
struct IPDLParamTraits<IPC::Principal> {
  typedef IPC::Principal paramType;
  static void Write(IPC::MessageWriter* aWriter, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aWriter, aActor, aParam.mPrincipal);
  }
  static bool Read(IPC::MessageReader* aReader, IProtocol* aActor,
                   paramType* aResult) {
    return ReadIPDLParam(aReader, aActor, &aResult->mPrincipal);
  }
};

}  // namespace mozilla::ipc

#endif  // mozilla_dom_permission_message_utils_h__

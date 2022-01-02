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

  operator nsIPrincipal*() const { return mPrincipal.get(); }

  Principal& operator=(const Principal& aOther) = delete;

 private:
  RefPtr<nsIPrincipal> mPrincipal;
};

}  // namespace IPC

namespace mozilla {
namespace ipc {

template <>
struct IPDLParamTraits<nsIPrincipal*> {
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    nsIPrincipal* aParam);
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, RefPtr<nsIPrincipal>* aResult);

  // Overload to support deserializing nsCOMPtr<nsIPrincipal> directly.
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, nsCOMPtr<nsIPrincipal>* aResult) {
    RefPtr<nsIPrincipal> result;
    if (!Read(aMsg, aIter, aActor, &result)) {
      return false;
    }
    *aResult = std::move(result);
    return true;
  }
};

template <>
struct IPDLParamTraits<IPC::Principal> {
  typedef IPC::Principal paramType;
  static void Write(IPC::Message* aMsg, IProtocol* aActor,
                    const paramType& aParam) {
    WriteIPDLParam(aMsg, aActor, aParam.mPrincipal);
  }
  static bool Read(const IPC::Message* aMsg, PickleIterator* aIter,
                   IProtocol* aActor, paramType* aResult) {
    return ReadIPDLParam(aMsg, aIter, aActor, &aResult->mPrincipal);
  }
};

}  // namespace ipc
}  // namespace mozilla

#endif  // mozilla_dom_permission_message_utils_h__

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_permission_message_utils_h__
#define mozilla_dom_permission_message_utils_h__

#include "ipc/IPCMessageUtils.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"

namespace IPC {

class Principal {
  friend struct ParamTraits<Principal>;

public:
  Principal() : mPrincipal(nullptr) {}
  Principal(nsIPrincipal* aPrincipal) : mPrincipal(aPrincipal) {}
  operator nsIPrincipal*() const { return mPrincipal.get(); }

private:
  // Unimplemented
  Principal& operator=(Principal&);
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

template <>
struct ParamTraits<Principal>
{
  typedef Principal paramType;
  static void Write(Message* aMsg, const paramType& aParam);
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult);
};

} // namespace IPC

#endif // mozilla_dom_permission_message_utils_h__


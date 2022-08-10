/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestChild_h
#define mozilla__ipdltest_IPDLUnitTestChild_h

#include "mozilla/_ipdltest/PIPDLUnitTestChild.h"
#include "nsISupportsImpl.h"

namespace mozilla::_ipdltest {

class IPDLUnitTestChild : public PIPDLUnitTestChild {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPDLUnitTestChild, override)

 private:
  friend class PIPDLUnitTestChild;

  ipc::IPCResult RecvStart(const nsCString& aName, ipc::ScopedPort aPort,
                           const nsID& aChannelId);

  void ActorDestroy(ActorDestroyReason aReason) override;

  ~IPDLUnitTestChild() = default;
};

}  // namespace mozilla::_ipdltest

#endif  // mozilla__ipdltest_IPDLUnitTestChild_h

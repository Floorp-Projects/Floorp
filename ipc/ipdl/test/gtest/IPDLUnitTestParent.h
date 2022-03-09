/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestParent_h
#define mozilla__ipdltest_IPDLUnitTestParent_h

#include "mozilla/_ipdltest/PIPDLUnitTestParent.h"
#include "mozilla/ipc/GeckoChildProcessHost.h"
#include "nsISupportsImpl.h"

namespace mozilla::_ipdltest {

class IPDLUnitTestParent : public PIPDLUnitTestParent {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(IPDLUnitTestParent, override)

  static already_AddRefed<IPDLUnitTestParent> CreateCrossProcess();
  static already_AddRefed<IPDLUnitTestParent> CreateCrossThread();

  // Try to start a connection with the given name and open `aParentActor` with
  // it. Fails the current test and returns false on failure.
  bool Start(const char* aName, IToplevelProtocol* aParentActor);

  bool ReportedComplete() const { return mComplete; }

 private:
  friend class PIPDLUnitTestParent;

  ipc::IPCResult RecvReport(const TestPartResult& aResult);
  ipc::IPCResult RecvComplete();

  void KillHard();

  ~IPDLUnitTestParent();

  // Only one of these two will be set depending.
  nsCOMPtr<nsIThread> mOtherThread;
  mozilla::ipc::GeckoChildProcessHost* mSubprocess = nullptr;

  // Set to true when the test is complete.
  bool mComplete = false;
  bool mCalledKillHard = false;
};

}  // namespace mozilla::_ipdltest

#endif  // mozilla__ipdltest_IPDLUnitTestParent_h

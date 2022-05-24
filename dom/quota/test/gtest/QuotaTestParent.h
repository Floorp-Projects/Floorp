/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_TEST_GTEST_QUOTATESTPARENT_H_
#define DOM_QUOTA_TEST_GTEST_QUOTATESTPARENT_H_

#include "mozilla/dom/quota/PQuotaTestParent.h"

namespace mozilla::dom::quota {

class QuotaTestParent : public PQuotaTestParent {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(QuotaTestParent, override)

 public:
  mozilla::ipc::IPCResult RecvTry_Success_CustomErr_QmIpcFail(
      bool* aTryDidNotReturn);

  mozilla::ipc::IPCResult RecvTry_Success_CustomErr_IpcFail(
      bool* aTryDidNotReturn);

  mozilla::ipc::IPCResult RecvTryInspect_Success_CustomErr_QmIpcFail(
      bool* aTryDidNotReturn);

  mozilla::ipc::IPCResult RecvTryInspect_Success_CustomErr_IpcFail(
      bool* aTryDidNotReturn);

 private:
  ~QuotaTestParent() = default;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_TEST_GTEST_QUOTATESTPARENT_H_

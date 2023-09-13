/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_
#define DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_

#include "OriginOperationBase.h"
#include "mozilla/Atomics.h"
#include "mozilla/dom/quota/CheckedUnsafePtr.h"

namespace mozilla::dom::quota {

class NormalOriginOperationBase
    : public OriginOperationBase,
      public SupportsCheckedUnsafePtr<CheckIf<DiagnosticAssertEnabled>> {
 protected:
  mozilla::Atomic<bool> mCanceled;

  // If we want to only forward declare DirectoryLock which is referenced by
  // the mDirectoryLock member then the constructor and destructor must be
  // defined in the cpp where DirectoryLock is fully declared (DirectoryLock.h
  // is included). The compiler would complain otherwise because it wouldn't
  // know how to call DirectoryLock::AddRef/Release in the constructor and
  // destructor
  NormalOriginOperationBase(MovingNotNull<RefPtr<QuotaManager>> aQuotaManager,
                            const char* aName);

  ~NormalOriginOperationBase();

  virtual RefPtr<BoolPromise> OpenDirectory() = 0;

  // Used to send results before unblocking open.
  virtual void SendResults() = 0;

  virtual void CloseDirectory() = 0;

 private:
  virtual RefPtr<BoolPromise> Open() override;

  virtual void UnblockOpen() override;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_NORMALORIGINOPERATIONBASE_H_

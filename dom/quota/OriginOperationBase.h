/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_ORIGINOPERATIONBASE_H_
#define DOM_QUOTA_ORIGINOPERATIONBASE_H_

#include "ErrorList.h"
#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/dom/quota/Config.h"
#include "mozilla/dom/quota/ForwardDecls.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom::quota {

class QuotaManager;

class OriginOperationBase : public BackgroundThreadObject {
 protected:
  const NotNull<RefPtr<QuotaManager>> mQuotaManager;
  nsresult mResultCode;

 private:
  bool mActorDestroyed;

#ifdef QM_COLLECTING_OPERATION_TELEMETRY
  const char* mName = nullptr;
#endif

 public:
  NS_INLINE_DECL_REFCOUNTING(OriginOperationBase)

  void NoteActorDestroyed() {
    AssertIsOnOwningThread();

    mActorDestroyed = true;
  }

  bool IsActorDestroyed() const {
    AssertIsOnOwningThread();

    return mActorDestroyed;
  }

#ifdef QM_COLLECTING_OPERATION_TELEMETRY
  const char* Name() const { return mName; }
#endif

  void RunImmediately();

 protected:
  OriginOperationBase(MovingNotNull<RefPtr<QuotaManager>>&& aQuotaManager,
                      const char* aName);

  // Reference counted.
  virtual ~OriginOperationBase();

  virtual nsresult DoInit(QuotaManager& aQuotaManager);

  virtual RefPtr<BoolPromise> Open() = 0;

#ifdef DEBUG
  virtual nsresult DirectoryOpen();
#endif

  virtual nsresult DoDirectoryWork(QuotaManager& aQuotaManager) = 0;

  virtual void UnblockOpen() = 0;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_ORIGINOPERATIONBASE_H_

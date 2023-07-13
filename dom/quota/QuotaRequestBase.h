/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAREQUESTBASE_H_
#define DOM_QUOTA_QUOTAREQUESTBASE_H_

#include "NormalOriginOperationBase.h"
#include "mozilla/dom/quota/PQuotaRequestParent.h"

namespace mozilla::dom::quota {

class RequestResponse;

class QuotaRequestBase : public NormalOriginOperationBase,
                         public PQuotaRequestParent {
 protected:
  explicit QuotaRequestBase(const char* aRunnableName, bool aExclusive)
      : NormalOriginOperationBase(aRunnableName, Nullable<PersistenceType>(),
                                  OriginScope::FromNull(),
                                  Nullable<Client::Type>(), aExclusive) {}

  QuotaRequestBase(const char* aRunnableName,
                   const Nullable<PersistenceType>& aPersistenceType,
                   const OriginScope& aOriginScope,
                   const Nullable<Client::Type>& aClientType, bool aExclusive)
      : NormalOriginOperationBase(aRunnableName, aPersistenceType, aOriginScope,
                                  aClientType, aExclusive) {}

  // Subclasses use this override to set the IPDL response value.
  virtual void GetResponse(RequestResponse& aResponse) = 0;

 private:
  virtual void SendResults() override;

  // IPDL methods.
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAREQUESTBASE_H_

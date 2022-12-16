/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_QUOTA_QUOTAOBJECT_H_
#define DOM_QUOTA_QUOTAOBJECT_H_

#include "nsISupportsImpl.h"

class nsIInterfaceRequestor;

namespace mozilla::dom::quota {

class CanonicalQuotaObject;
class IPCQuotaObject;
class RemoteQuotaObject;

// QuotaObject type is serializable, but only in a restricted manner. The type
// is only safe to serialize in the parent process and only when the type
// hasn't been previously deserialized. So the type can be serialized in the
// parent process and deserialized in a child process or it can be serialized
// in the parent process and deserialized in the parent process as well
// (non-e10s mode). The same type can never be serialized/deserialized more
// than once.
// The deserialized type (remote variant) can only be used on the thread it was
// deserialized on and it will stop working if the thread it was sent from is
// shutdown (consumers should make sure that the originating thread is kept
// alive for the necessary time).
class QuotaObject {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  CanonicalQuotaObject* AsCanonicalQuotaObject();

  RemoteQuotaObject* AsRemoteQuotaObject();

  // Serialize this QuotaObject. This method works only in the parent process
  // and only with objects which haven't been previously deserialized.
  // The serial event target where this method is called should be highly
  // available, as it will be used to process requests from the remote variant.
  IPCQuotaObject Serialize(nsIInterfaceRequestor* aCallbacks);

  // Deserialize a QuotaObject. This method works in both the child and parent.
  // The deserialized QuotaObject can only be used on the calling serial event
  // target.
  static RefPtr<QuotaObject> Deserialize(IPCQuotaObject& aQuotaObject);

  virtual const nsAString& Path() const = 0;

  [[nodiscard]] virtual bool MaybeUpdateSize(int64_t aSize, bool aTruncate) = 0;

  virtual bool IncreaseSize(int64_t aDelta) = 0;

  virtual void DisableQuotaCheck() = 0;

  virtual void EnableQuotaCheck() = 0;

 protected:
  QuotaObject(bool aIsRemote) : mIsRemote(aIsRemote) {}

  virtual ~QuotaObject() = default;

  const bool mIsRemote;
};

}  // namespace mozilla::dom::quota

#endif  // DOM_QUOTA_QUOTAOBJECT_H_

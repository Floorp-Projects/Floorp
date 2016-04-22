/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushSubscriptionOptions.h"

#include "mozilla/dom/PushSubscriptionOptionsBinding.h"

namespace mozilla {
namespace dom {

PushSubscriptionOptions::PushSubscriptionOptions(nsIGlobalObject* aGlobal,
                                                 nsTArray<uint8_t>&& aAppServerKey)
  : mGlobal(aGlobal)
  , mAppServerKey(Move(aAppServerKey))
{
  // There's only one global on a worker, so we don't need to pass a global
  // object to the constructor.
  MOZ_ASSERT_IF(NS_IsMainThread(), mGlobal);
}

PushSubscriptionOptions::~PushSubscriptionOptions() {}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(PushSubscriptionOptions, mGlobal)
NS_IMPL_CYCLE_COLLECTING_ADDREF(PushSubscriptionOptions)
NS_IMPL_CYCLE_COLLECTING_RELEASE(PushSubscriptionOptions)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(PushSubscriptionOptions)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

JSObject*
PushSubscriptionOptions::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto)
{
  return PushSubscriptionOptionsBinding::Wrap(aCx, this, aGivenProto);
}

void
PushSubscriptionOptions::GetApplicationServerKey(JSContext* aCx,
                                                 JS::MutableHandle<JSObject*> aKey,
                                                 ErrorResult& aRv)
{
  PushUtil::CopyArrayToArrayBuffer(aCx, mAppServerKey, aKey, aRv);
}

} // namespace dom
} // namespace mozilla

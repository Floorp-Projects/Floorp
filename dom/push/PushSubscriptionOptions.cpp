/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PushSubscriptionOptions.h"

#include "mozilla/dom/PushSubscriptionOptionsBinding.h"
#include "mozilla/HoldDropJSObjects.h"

namespace mozilla {
namespace dom {

PushSubscriptionOptions::PushSubscriptionOptions(nsIGlobalObject* aGlobal,
                                                 nsTArray<uint8_t>&& aRawAppServerKey)
  : mGlobal(aGlobal)
  , mRawAppServerKey(Move(aRawAppServerKey))
  , mAppServerKey(nullptr)
{
  // There's only one global on a worker, so we don't need to pass a global
  // object to the constructor.
  MOZ_ASSERT_IF(NS_IsMainThread(), mGlobal);
  mozilla::HoldJSObjects(this);
}

PushSubscriptionOptions::~PushSubscriptionOptions()
{
  mAppServerKey = nullptr;
  mozilla::DropJSObjects(this);
}

NS_IMPL_CYCLE_COLLECTION_CLASS(PushSubscriptionOptions)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(PushSubscriptionOptions)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->mAppServerKey = nullptr;
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(PushSubscriptionOptions)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(PushSubscriptionOptions)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAppServerKey)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

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
  if (!mRawAppServerKey.IsEmpty() && !mAppServerKey) {
    JS::Rooted<JSObject*> appServerKey(aCx);
    PushUtil::CopyArrayToArrayBuffer(aCx, mRawAppServerKey, &appServerKey, aRv);
    if (aRv.Failed()) {
      return;
    }
    MOZ_ASSERT(appServerKey);
    mAppServerKey = appServerKey;
  }
  aKey.set(mAppServerKey);
}

} // namespace dom
} // namespace mozilla

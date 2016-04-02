/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PushSubscription_h
#define mozilla_dom_PushSubscription_h

#include "jsapi.h"
#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/RefPtr.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/PushSubscriptionBinding.h"
#include "mozilla/dom/TypedArray.h"

class nsIGlobalObject;

namespace mozilla {
namespace dom {

namespace workers {
class WorkerPrivate;
}

class Promise;

class PushSubscription final : public nsISupports
                             , public nsWrapperCache
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(PushSubscription)

  PushSubscription(nsIGlobalObject* aGlobal,
                   const nsAString& aEndpoint,
                   const nsAString& aScope,
                   nsTArray<uint8_t>&& aP256dhKey,
                   nsTArray<uint8_t>&& aAuthSecret);

  JSObject*
  WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto) override;

  nsIGlobalObject*
  GetParentObject() const
  {
    return mGlobal;
  }

  void
  GetEndpoint(nsAString& aEndpoint) const
  {
    aEndpoint = mEndpoint;
  }

  void
  GetKey(JSContext* cx,
         PushEncryptionKeyName aType,
         JS::MutableHandle<JSObject*> aKey);

  static already_AddRefed<PushSubscription>
  Constructor(GlobalObject& aGlobal,
              const nsAString& aEndpoint,
              const nsAString& aScope,
              const Nullable<ArrayBuffer>& aP256dhKey,
              const Nullable<ArrayBuffer>& aAuthSecret,
              ErrorResult& aRv);

  already_AddRefed<Promise>
  Unsubscribe(ErrorResult& aRv);

  void
  ToJSON(PushSubscriptionJSON& aJSON);

protected:
  ~PushSubscription();

private:
  already_AddRefed<Promise>
  UnsubscribeFromWorker(ErrorResult& aRv);

  nsString mEndpoint;
  nsString mScope;
  nsTArray<uint8_t> mRawP256dhKey;
  nsTArray<uint8_t> mAuthSecret;
  nsCOMPtr<nsIGlobalObject> mGlobal;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PushSubscription_h

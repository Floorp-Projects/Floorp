/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MediaDevices.h"
#include "mozilla/dom/MediaStreamBinding.h"
#include "mozilla/dom/MediaDevicesBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/MediaManager.h"
#include "nsIEventTarget.h"
#include "nsIScriptGlobalObject.h"
#include "nsPIDOMWindow.h"

namespace mozilla {
namespace dom {

class MediaDevices::GumResolver : public nsIDOMGetUserMediaSuccessCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GumResolver(Promise* aPromise) : mPromise(aPromise) {}

  NS_IMETHOD
  OnSuccess(nsISupports* aStream) MOZ_OVERRIDE
  {
    nsRefPtr<DOMLocalMediaStream> stream = do_QueryObject(aStream);
    if (!stream) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeResolve(stream);
    return NS_OK;
  }

private:
  virtual ~GumResolver() {}
  nsRefPtr<Promise> mPromise;
};

class MediaDevices::GumRejecter : public nsIDOMGetUserMediaErrorCallback
{
public:
  NS_DECL_ISUPPORTS

  explicit GumRejecter(Promise* aPromise) : mPromise(aPromise) {}

  NS_IMETHOD
  OnError(nsISupports* aError) MOZ_OVERRIDE
  {
    nsRefPtr<MediaStreamError> error = do_QueryObject(aError);
    if (!error) {
      return NS_ERROR_FAILURE;
    }
    mPromise->MaybeReject(error);
    return NS_OK;
  }

private:
  virtual ~GumRejecter() {}
  nsRefPtr<Promise> mPromise;
};

NS_IMPL_ISUPPORTS(MediaDevices::GumResolver, nsIDOMGetUserMediaSuccessCallback)
NS_IMPL_ISUPPORTS(MediaDevices::GumRejecter, nsIDOMGetUserMediaErrorCallback)

already_AddRefed<Promise>
MediaDevices::GetUserMedia(const MediaStreamConstraints& aConstraints,
                           ErrorResult &aRv)
{
  ErrorResult rv;
  nsPIDOMWindow* window = GetOwner();
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(window);
  nsRefPtr<Promise> p = Promise::Create(go, aRv);
  NS_ENSURE_TRUE(!rv.Failed(), nullptr);

  nsRefPtr<GumResolver> resolver = new GumResolver(p);
  nsRefPtr<GumRejecter> rejecter = new GumRejecter(p);

  aRv = MediaManager::Get()->GetUserMedia(window, aConstraints,
                                          resolver, rejecter);
  return p.forget();
}

NS_IMPL_ADDREF_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(MediaDevices, DOMEventTargetHelper)
NS_INTERFACE_MAP_BEGIN(MediaDevices)
  NS_INTERFACE_MAP_ENTRY(MediaDevices)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

JSObject*
MediaDevices::WrapObject(JSContext* aCx)
{
  return MediaDevicesBinding::Wrap(aCx, this);
}

} // namespace dom
} // namespace mozilla

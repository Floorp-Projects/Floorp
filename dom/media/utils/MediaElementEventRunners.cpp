/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaElementEventRunners.h"

#include "mozilla/dom/HTMLMediaElement.h"

extern mozilla::LazyLogModule gMediaElementEventsLog;
#define LOG_EVENT(type, msg) MOZ_LOG(gMediaElementEventsLog, type, msg)

namespace mozilla::dom {

nsMediaEventRunner::nsMediaEventRunner(const nsAString& aName,
                                       HTMLMediaElement* aElement,
                                       const nsAString& aEventName)
    : mElement(aElement),
      mName(aName),
      mEventName(aEventName),
      mLoadID(mElement->GetCurrentLoadID()) {}

bool nsMediaEventRunner::IsCancelled() {
  return !mElement || mElement->GetCurrentLoadID() != mLoadID;
}

NS_IMPL_CYCLE_COLLECTION(nsMediaEventRunner, mElement)
NS_IMPL_CYCLE_COLLECTING_ADDREF(nsMediaEventRunner)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsMediaEventRunner)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY(nsIRunnable)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIRunnable)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsAsyncEventRunner::Run() {
  // Silently cancel if our load has been cancelled or element has been CCed.
  return IsCancelled() ? NS_OK : mElement->DispatchEvent(mEventName);
}

nsResolveOrRejectPendingPlayPromisesRunner::
    nsResolveOrRejectPendingPlayPromisesRunner(
        HTMLMediaElement* aElement, nsTArray<RefPtr<PlayPromise>>&& aPromises,
        nsresult aError)
    : nsMediaEventRunner(u"nsResolveOrRejectPendingPlayPromisesRunner"_ns,
                         aElement),
      mPromises(std::move(aPromises)),
      mError(aError) {
  mElement->mPendingPlayPromisesRunners.AppendElement(this);
}

void nsResolveOrRejectPendingPlayPromisesRunner::ResolveOrReject() {
  if (NS_SUCCEEDED(mError)) {
    PlayPromise::ResolvePromisesWithUndefined(mPromises);
  } else {
    PlayPromise::RejectPromises(mPromises, mError);
  }
}

NS_IMETHODIMP nsResolveOrRejectPendingPlayPromisesRunner::Run() {
  if (!IsCancelled()) {
    ResolveOrReject();
  }

  mElement->mPendingPlayPromisesRunners.RemoveElement(this);
  return NS_OK;
}

NS_IMETHODIMP nsNotifyAboutPlayingRunner::Run() {
  if (IsCancelled()) {
    mElement->mPendingPlayPromisesRunners.RemoveElement(this);
    return NS_OK;
  }

  mElement->DispatchEvent(u"playing"_ns);
  return nsResolveOrRejectPendingPlayPromisesRunner::Run();
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                                   nsMediaEventRunner, mPromises)
NS_IMPL_ADDREF_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                         nsMediaEventRunner)
NS_IMPL_RELEASE_INHERITED(nsResolveOrRejectPendingPlayPromisesRunner,
                          nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(
    nsResolveOrRejectPendingPlayPromisesRunner)
NS_INTERFACE_MAP_END_INHERITING(nsMediaEventRunner)

NS_IMETHODIMP nsSourceErrorEventRunner::Run() {
  // Silently cancel if our load has been cancelled.
  if (IsCancelled()) {
    return NS_OK;
  }
  LOG_EVENT(LogLevel::Debug,
            ("%p Dispatching simple event source error", mElement.get()));
  return nsContentUtils::DispatchTrustedEvent(mElement->OwnerDoc(), mSource,
                                              u"error"_ns, CanBubble::eNo,
                                              Cancelable::eNo);
}

NS_IMPL_CYCLE_COLLECTION_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner,
                                   mSource)
NS_IMPL_ADDREF_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner)
NS_IMPL_RELEASE_INHERITED(nsSourceErrorEventRunner, nsMediaEventRunner)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsSourceErrorEventRunner)
NS_INTERFACE_MAP_END_INHERITING(nsMediaEventRunner)

NS_IMETHODIMP nsSyncSection::Run() {
  // Silently cancel if our load has been cancelled.
  if (IsCancelled()) {
    return NS_OK;
  }
  mRunnable->Run();
  return NS_OK;
}

#undef LOG_EVENT
}  // namespace mozilla::dom

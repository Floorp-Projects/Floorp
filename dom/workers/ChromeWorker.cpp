/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromeWorker.h"

#include "mozilla/dom/WorkerBinding.h"
#include "nsContentUtils.h"
#include "WorkerPrivate.h"

namespace mozilla {
namespace dom {

/* static */ already_AddRefed<ChromeWorker>
ChromeWorker::Constructor(const GlobalObject& aGlobal,
                          const nsAString& aScriptURL,
                          ErrorResult& aRv)
{
  JSContext* cx = aGlobal.Context();

  RefPtr<WorkerPrivate> workerPrivate =
    WorkerPrivate::Constructor(cx, aScriptURL, true /* aIsChromeWorker */,
                               WorkerTypeDedicated, EmptyString(),
                               VoidCString(), nullptr /*aLoadInfo */, aRv);
  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> globalObject =
    do_QueryInterface(aGlobal.GetAsSupports());

  RefPtr<ChromeWorker> worker =
    new ChromeWorker(globalObject, workerPrivate.forget());
  return worker.forget();
}

/* static */ bool
ChromeWorker::WorkerAvailable(JSContext* aCx, JSObject* /* unused */)
{
  // Chrome is always allowed to use workers, and content is never
  // allowed to use ChromeWorker, so all we have to check is the
  // caller.  However, chrome workers apparently might not have a
  // system principal, so we have to check for them manually.
  if (NS_IsMainThread()) {
    return nsContentUtils::IsSystemCaller(aCx);
  }

  return GetWorkerPrivateFromContext(aCx)->IsChromeWorker();
}

ChromeWorker::ChromeWorker(nsIGlobalObject* aGlobalObject,
                           already_AddRefed<WorkerPrivate> aWorkerPrivate)
  : Worker(aGlobalObject, std::move(aWorkerPrivate))
{}

ChromeWorker::~ChromeWorker() = default;

JSObject*
ChromeWorker::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  JS::Rooted<JSObject*> wrapper(aCx,
    ChromeWorkerBinding::Wrap(aCx, this, aGivenProto));
  if (wrapper) {
    // Most DOM objects don't assume they have a reflector. If they don't have
    // one and need one, they create it. But in workers code, we assume that the
    // reflector is always present.  In order to guarantee that it's always
    // present, we have to preserve it. Otherwise the GC will happily collect it
    // as needed.
    MOZ_ALWAYS_TRUE(TryPreserveWrapper(wrapper));
  }

  return wrapper;
}

} // dom namespace
} // mozilla namespace

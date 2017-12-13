/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletGlobalScope.h"
#include "mozilla/dom/WorkletGlobalScopeBinding.h"
#include "mozilla/dom/Console.h"
#include "nsContentUtils.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkletGlobalScope)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkletGlobalScope)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mConsole)
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkletGlobalScope)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mConsole)
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(WorkletGlobalScope)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletGlobalScope)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletGlobalScope)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletGlobalScope)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(WorkletGlobalScope)
NS_INTERFACE_MAP_END

WorkletGlobalScope::WorkletGlobalScope(nsPIDOMWindowInner* aWindow)
  : mWindow(aWindow)
{
  MOZ_ASSERT(aWindow);
}

WorkletGlobalScope::~WorkletGlobalScope()
{
}

JSObject*
WorkletGlobalScope::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_CRASH("We should never get here!");
  return nullptr;
}

Console*
WorkletGlobalScope::GetConsole(ErrorResult& aRv)
{
  if (!mConsole) {
    mConsole = Console::Create(mWindow, aRv);
    if (NS_WARN_IF(aRv.Failed())) {
      return nullptr;
    }
  }

  return mConsole;
}

void
WorkletGlobalScope::Dump(const Optional<nsAString>& aString) const
{
  if (!nsContentUtils::DOMWindowDumpEnabled()) {
    return;
  }

  if (!aString.WasPassed()) {
    return;
  }

  NS_ConvertUTF16toUTF8 str(aString.Value());

#ifdef ANDROID
  __android_log_print(ANDROID_LOG_INFO, "Gecko", "%s", str.get());
#endif

  fputs(str.get(), stdout);
  fflush(stdout);
}

} // dom namespace
} // mozilla namespace

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WorkletGlobalScope.h"
#include "mozilla/dom/WorkletGlobalScopeBinding.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(WorkletGlobalScope)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(WorkletGlobalScope)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(WorkletGlobalScope)

  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_WRAPPERCACHE(WorkletGlobalScope)

NS_IMPL_CYCLE_COLLECTING_ADDREF(WorkletGlobalScope)
NS_IMPL_CYCLE_COLLECTING_RELEASE(WorkletGlobalScope)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(WorkletGlobalScope)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
NS_INTERFACE_MAP_END

JSObject*
WorkletGlobalScope::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  MOZ_CRASH("We should never get here!");
  return nullptr;
}

bool
WorkletGlobalScope::WrapGlobalObject(JSContext* aCx,
                                     nsIPrincipal* aPrincipal,
                                     JS::MutableHandle<JSObject*> aReflector)
{
  JS::CompartmentOptions options;
  return WorkletGlobalScopeBinding::Wrap(aCx, this, this,
                                         options,
                                         nsJSPrincipals::get(aPrincipal),
                                         true, aReflector);
}
} // dom namespace
} // mozilla namespace

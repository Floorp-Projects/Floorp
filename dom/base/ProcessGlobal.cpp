/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ProcessGlobal.h"

#include "nsContentCID.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/HoldDropJSObjects.h"

using namespace mozilla;
using namespace mozilla::dom;

ProcessGlobal::ProcessGlobal(nsFrameMessageManager* aMessageManager)
 : mInitialized(false),
   mMessageManager(aMessageManager)
{
  SetIsNotDOMBinding();
  mozilla::HoldJSObjects(this);
}

ProcessGlobal::~ProcessGlobal()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

ProcessGlobal*
ProcessGlobal::Get()
{
  nsCOMPtr<nsISyncMessageSender> service = do_GetService(NS_CHILDPROCESSMESSAGEMANAGER_CONTRACTID);
  if (!service) {
    return nullptr;
  }
  return static_cast<ProcessGlobal*>(service.get());
}

/* [notxpcom] boolean markForCC (); */
// This method isn't automatically forwarded safely because it's notxpcom, so
// the IDL binding doesn't know what value to return.
NS_IMETHODIMP_(bool)
ProcessGlobal::MarkForCC()
{
  MarkScopesForCC();
  return mMessageManager ? mMessageManager->MarkForCC() : false;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ProcessGlobal)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ProcessGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mGlobal)
  tmp->TraverseHostObjectURIs(cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ProcessGlobal)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  for (uint32_t i = 0; i < tmp->mAnonymousGlobalScopes.Length(); ++i) {
    NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mAnonymousGlobalScopes[i])
  }
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ProcessGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mGlobal)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mAnonymousGlobalScopes)
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ProcessGlobal)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentProcessMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageListenerManager)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsISyncMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIContentProcessMessageManager)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(ContentProcessMessageManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ProcessGlobal)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ProcessGlobal)

bool
ProcessGlobal::Init()
{
  if (mInitialized) {
    return true;
  }
  mInitialized = true;

  nsISupports* scopeSupports = NS_ISUPPORTS_CAST(nsIContentProcessMessageManager*, this);
  return InitChildGlobalInternal(scopeSupports, NS_LITERAL_CSTRING("processChildGlobal"));
}

void
ProcessGlobal::LoadScript(const nsAString& aURL)
{
  Init();
  LoadScriptInternal(aURL, false);
}

void
ProcessGlobal::SetInitialProcessData(JS::HandleValue aInitialData)
{
  mMessageManager->SetInitialProcessData(aInitialData);
}

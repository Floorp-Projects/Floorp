/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentProcessMessageManager.h"

#include "nsContentCID.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/MessageManagerBinding.h"
#include "mozilla/dom/ParentProcessMessageManager.h"
#include "mozilla/dom/ResolveSystemBinding.h"
#include "mozilla/dom/ipc/SharedMap.h"

using namespace mozilla;
using namespace mozilla::dom;

bool ContentProcessMessageManager::sWasCreated = false;

ContentProcessMessageManager::ContentProcessMessageManager(nsFrameMessageManager* aMessageManager)
 : MessageManagerGlobal(aMessageManager),
   mInitialized(false)
{
  mozilla::HoldJSObjects(this);
}

ContentProcessMessageManager::~ContentProcessMessageManager()
{
  mAnonymousGlobalScopes.Clear();
  mozilla::DropJSObjects(this);
}

bool
ContentProcessMessageManager::DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                        JS::Handle<jsid> aId,
                                        JS::MutableHandle<JS::PropertyDescriptor> aDesc)
{
    bool found;
    if (!SystemGlobalResolve(aCx, aObj, aId, &found)) {
      return false;
    }
    if (found) {
      FillPropertyDescriptor(aDesc, aObj, JS::UndefinedValue(), false);
    }
    return true;
}

/* static */
bool
ContentProcessMessageManager::MayResolve(jsid aId)
{
  return MayResolveAsSystemBindingName(aId);
}

void
ContentProcessMessageManager::GetOwnPropertyNames(JSContext* aCx, JS::AutoIdVector& aNames,
                                                  bool aEnumerableOnly, ErrorResult& aRv)
{
  JS::Rooted<JSObject*> thisObj(aCx, GetWrapper());
  GetSystemBindingNames(aCx, thisObj, aNames, aEnumerableOnly, aRv);
}

ContentProcessMessageManager*
ContentProcessMessageManager::Get()
{
  nsCOMPtr<nsIGlobalObject> service = do_GetService(NS_CHILDPROCESSMESSAGEMANAGER_CONTRACTID);
  if (!service) {
    return nullptr;
  }
  ContentProcessMessageManager* global = static_cast<ContentProcessMessageManager*>(service.get());
  if (global) {
    sWasCreated = true;
  }
  return global;
}

already_AddRefed<mozilla::dom::ipc::SharedMap>
ContentProcessMessageManager::SharedData()
{
  if (ContentChild* child = ContentChild::GetSingleton()) {
    return do_AddRef(child->SharedData());
  }
  auto* ppmm = nsFrameMessageManager::sParentProcessManager;
  return do_AddRef(ppmm->SharedData()->GetReadOnly());
}

bool
ContentProcessMessageManager::WasCreated()
{
  return sWasCreated;
}

void
ContentProcessMessageManager::MarkForCC()
{
  MarkScopesForCC();
  MessageManagerGlobal::MarkForCC();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(ContentProcessMessageManager)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ContentProcessMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mMessageManager)
  tmp->TraverseHostObjectURIs(cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ContentProcessMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ContentProcessMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
  tmp->UnlinkHostObjectURIs();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ContentProcessMessageManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIScriptObjectPrincipal)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalObject)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ContentProcessMessageManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ContentProcessMessageManager)

bool
ContentProcessMessageManager::Init()
{
  if (mInitialized) {
    return true;
  }
  mInitialized = true;

  return InitChildGlobalInternal(NS_LITERAL_CSTRING("processChildGlobal"));
}

bool
ContentProcessMessageManager::WrapGlobalObject(JSContext* aCx,
                                               JS::RealmOptions& aOptions,
                                               JS::MutableHandle<JSObject*> aReflector)
{
  bool ok = ContentProcessMessageManager_Binding::Wrap(aCx, this, this, aOptions,
                                                       nsJSPrincipals::get(mPrincipal),
                                                       true, aReflector);
  if (ok) {
    // Since we can't rewrap we have to preserve the global's wrapper here.
    PreserveWrapper(ToSupports(this));
  }
  return ok;
}

void
ContentProcessMessageManager::LoadScript(const nsAString& aURL)
{
  Init();
  JS::Rooted<JSObject*> global(mozilla::dom::RootingCx(), GetWrapper());
  LoadScriptInternal(global, aURL, false);
}

void
ContentProcessMessageManager::SetInitialProcessData(JS::HandleValue aInitialData)
{
  mMessageManager->SetInitialProcessData(aInitialData);
}

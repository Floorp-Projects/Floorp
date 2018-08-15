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
#include "mozilla/dom/ScriptSettings.h"
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

ContentProcessMessageManager*
ContentProcessMessageManager::Get()
{
  nsCOMPtr<nsIMessageSender> service = do_GetService(NS_CHILDPROCESSMESSAGEMANAGER_CONTRACTID);
  if (!service) {
    return nullptr;
  }
  sWasCreated = true;
  return static_cast<ContentProcessMessageManager*>(service.get());
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
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ContentProcessMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
  tmp->nsMessageManagerScriptExecutor::Trace(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ContentProcessMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mMessageManager)
  tmp->nsMessageManagerScriptExecutor::Unlink();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(ContentProcessMessageManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMessageSender)
  NS_INTERFACE_MAP_ENTRY(nsIMessageSender)
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

  return nsMessageManagerScriptExecutor::Init();
}

JSObject*
ContentProcessMessageManager::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto)
{
  return ContentProcessMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

JSObject*
ContentProcessMessageManager::GetOrCreateWrapper()
{
  AutoJSAPI jsapi;
  jsapi.Init();

  JS::RootedValue val(jsapi.cx());
  if (!GetOrCreateDOMReflectorNoWrap(jsapi.cx(), this, &val)) {
    return nullptr;
  }
  return &val.toObject();
}

void
ContentProcessMessageManager::LoadScript(const nsAString& aURL)
{
  Init();
  JS::Rooted<JSObject*> messageManager(mozilla::dom::RootingCx(), GetOrCreateWrapper());
  LoadScriptInternal(messageManager, aURL, true);
}

void
ContentProcessMessageManager::SetInitialProcessData(JS::HandleValue aInitialData)
{
  mMessageManager->SetInitialProcessData(aInitialData);
}

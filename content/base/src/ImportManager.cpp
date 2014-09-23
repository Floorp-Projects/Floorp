/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImportManager.h"

#include "mozilla/EventListenerManager.h"
#include "HTMLLinkElement.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsCrossSiteListenerProxy.h"
#include "nsIChannel.h"
#include "nsIChannelPolicy.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsScriptLoader.h"
#include "nsNetUtil.h"

class AutoError {
public:
  explicit AutoError(mozilla::dom::ImportLoader* loader, bool scriptsBlocked = true)
    : mLoader(loader)
    , mPassed(false)
    , mScriptsBlocked(scriptsBlocked)
  {}

  ~AutoError()
  {
    if (!mPassed) {
      mLoader->Error(mScriptsBlocked);
    }
  }

  void Pass() { mPassed = true; }

private:
  mozilla::dom::ImportLoader* mLoader;
  bool mPassed;
  bool mScriptsBlocked;
};

namespace mozilla {
namespace dom {

NS_INTERFACE_MAP_BEGIN(ImportLoader)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(ImportLoader)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImportLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImportLoader)

NS_IMPL_CYCLE_COLLECTION(ImportLoader,
                         mDocument,
                         mImportParent,
                         mLinks)

ImportLoader::ImportLoader(nsIURI* aURI, nsIDocument* aImportParent)
  : mURI(aURI)
  , mImportParent(aImportParent)
  , mReady(false)
  , mStopped(false)
  , mBlockingScripts(false)
{}

void
ImportLoader::BlockScripts()
{
  MOZ_ASSERT(!mBlockingScripts);
  mImportParent->ScriptLoader()->AddExecuteBlocker();
  mBlockingScripts = true;
}

void
ImportLoader::UnblockScripts()
{
  MOZ_ASSERT(mBlockingScripts);
  mImportParent->ScriptLoader()->RemoveExecuteBlocker();
  mBlockingScripts = false;
}

void
ImportLoader::DispatchEventIfFinished(nsINode* aNode)
{
  MOZ_ASSERT(!(mReady && mStopped));
  if (mReady) {
    DispatchLoadEvent(aNode);
  }
  if (mStopped) {
    DispatchErrorEvent(aNode);
  }
}

void
ImportLoader::AddLinkElement(nsINode* aNode)
{
  // If a new link element is added to the import tree that
  // refers to an import that is already finished loading or
  // stopped trying, we need to fire the corresponding event
  // on it.
  mLinks.AppendObject(aNode);
  DispatchEventIfFinished(aNode);
}

void
ImportLoader::RemoveLinkElement(nsINode* aNode)
{
  mLinks.RemoveObject(aNode);
}

// Events has to be fired with a script runner, so mImport can
// be set on the link element before the load event is fired even
// if ImportLoader::Get returns an already loaded import and we
// fire the load event immediately on the new referring link element.
class AsyncEvent : public nsRunnable {
public:
  AsyncEvent(nsINode* aNode, bool aSuccess)
    : mNode(aNode)
    , mSuccess(aSuccess)
  {
    MOZ_ASSERT(mNode);
  }

  NS_IMETHOD Run() {
    return nsContentUtils::DispatchTrustedEvent(mNode->OwnerDoc(),
                                                mNode,
                                                mSuccess ? NS_LITERAL_STRING("load")
                                                         : NS_LITERAL_STRING("error"),
                                                /* aCanBubble = */ false,
                                                /* aCancelable = */ false);
  }

private:
  nsCOMPtr<nsINode> mNode;
  bool mSuccess;
};

void
ImportLoader::DispatchErrorEvent(nsINode* aNode)
{
  nsContentUtils::AddScriptRunner(new AsyncEvent(aNode, /* aSuccess = */ false));
}

void
ImportLoader::DispatchLoadEvent(nsINode* aNode)
{
  nsContentUtils::AddScriptRunner(new AsyncEvent(aNode, /* aSuccess = */ true));
}

void
ImportLoader::Done()
{
  mReady = true;
  uint32_t count = mLinks.Count();
  for (uint32_t i = 0; i < count; i++) {
    DispatchLoadEvent(mLinks[i]);
  }
  UnblockScripts();
  ReleaseResources();
}

void
ImportLoader::Error(bool aUnblockScripts)
{
  mDocument = nullptr;
  mStopped = true;
  uint32_t count = mLinks.Count();
  for (uint32_t i = 0; i < count; i++) {
    DispatchErrorEvent(mLinks[i]);
  }
  if (aUnblockScripts) {
    UnblockScripts();
  }
  ReleaseResources();
}

// Release all the resources we don't need after there is no more
// data available on the channel, and the parser is done.
void ImportLoader::ReleaseResources()
{
  mParserStreamListener = nullptr;
  mImportParent = nullptr;
}

nsIPrincipal*
ImportLoader::Principal()
{
  MOZ_ASSERT(mImportParent);
  nsCOMPtr<nsIDocument> master = mImportParent->MasterDocument();
  nsCOMPtr<nsIScriptObjectPrincipal> sop = do_QueryInterface(master);
  MOZ_ASSERT(sop);
  return sop->GetPrincipal();
}

void
ImportLoader::Open()
{
  AutoError ae(this, false);
  // Imports should obey to the master documents CSP.
  nsCOMPtr<nsIDocument> master = mImportParent->MasterDocument();
  nsIPrincipal* principal = Principal();

  int16_t shouldLoad = nsIContentPolicy::ACCEPT;
  nsresult rv = NS_CheckContentLoadPolicy(nsIContentPolicy::TYPE_SUBDOCUMENT,
                                          mURI,
                                          principal,
                                          mImportParent,
                                          NS_LITERAL_CSTRING("text/html"),
                                          /* extra = */ nullptr,
                                          &shouldLoad,
                                          nsContentUtils::GetContentPolicy(),
                                          nsContentUtils::GetSecurityManager());
  if (NS_FAILED(rv) || NS_CP_REJECTED(shouldLoad)) {
    NS_WARN_IF_FALSE(NS_CP_ACCEPTED(shouldLoad), "ImportLoader rejected by CSP");
    return;
  }

  nsIScriptSecurityManager* secMan = nsContentUtils::GetSecurityManager();
  rv = secMan->CheckLoadURIWithPrincipal(principal, mURI,
                                         nsIScriptSecurityManager::STANDARD);
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<nsILoadGroup> loadGroup = mImportParent->GetDocumentLoadGroup();
  nsCOMPtr<nsIChannelPolicy> channelPolicy;
  nsCOMPtr<nsIContentSecurityPolicy> csp;
  rv = principal->GetCsp(getter_AddRefs(csp));
  NS_ENSURE_SUCCESS_VOID(rv);

  if (csp) {
    channelPolicy = do_CreateInstance("@mozilla.org/nschannelpolicy;1");
    channelPolicy->SetContentSecurityPolicy(csp);
    channelPolicy->SetLoadType(nsIContentPolicy::TYPE_SUBDOCUMENT);
  }
  nsCOMPtr<nsIChannel> channel;
  rv = NS_NewChannel(getter_AddRefs(channel),
                     mURI,
                     mImportParent,
                     nsILoadInfo::SEC_NORMAL,
                     nsIContentPolicy::TYPE_SUBDOCUMENT,
                     channelPolicy,
                     loadGroup,
                     nullptr,  // aCallbacks
                     nsIRequest::LOAD_BACKGROUND);

  NS_ENSURE_SUCCESS_VOID(rv);

  // Init CORSListenerProxy and omit credentials.
  nsRefPtr<nsCORSListenerProxy> corsListener =
    new nsCORSListenerProxy(this, principal,
                            /* aWithCredentials */ false);
  rv = corsListener->Init(channel, true);
  NS_ENSURE_SUCCESS_VOID(rv);

  rv = channel->AsyncOpen(corsListener, nullptr);
  NS_ENSURE_SUCCESS_VOID(rv);

  BlockScripts();
  ae.Pass();
}

NS_IMETHODIMP
ImportLoader::OnDataAvailable(nsIRequest* aRequest,
                              nsISupports* aContext,
                              nsIInputStream* aStream,
                              uint64_t aOffset,
                              uint32_t aCount)
{
  MOZ_ASSERT(mParserStreamListener);

  AutoError ae(this);
  nsresult rv;
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mParserStreamListener->OnDataAvailable(channel, aContext,
                                              aStream, aOffset,
                                              aCount);
  NS_ENSURE_SUCCESS(rv, rv);
  ae.Pass();
  return rv;
}

NS_IMETHODIMP
ImportLoader::HandleEvent(nsIDOMEvent *aEvent)
{
#ifdef DEBUG
  nsAutoString type;
  aEvent->GetType(type);
  MOZ_ASSERT(type.EqualsLiteral("DOMContentLoaded"));
#endif
  Done();
  return NS_OK;
}

NS_IMETHODIMP
ImportLoader::OnStopRequest(nsIRequest* aRequest,
                            nsISupports* aContext,
                            nsresult aStatus)
{
  // OnStartRequest throws a special error code to let us know that we
  // shouldn't do anything else.
  if (aStatus == NS_ERROR_DOM_ABORT_ERR) {
    // We failed in OnStartRequest, nothing more to do (we've already
    // dispatched an error event) just return here.
    MOZ_ASSERT(mStopped);
    return NS_OK;
  }

  if (mParserStreamListener) {
    mParserStreamListener->OnStopRequest(aRequest, aContext, aStatus);
  }

  if (!mDocument) {
    // If at this point we don't have a document, then the error was
    // already reported.
    return NS_ERROR_DOM_ABORT_ERR;
  }

  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(mDocument);
  EventListenerManager* manager = eventTarget->GetOrCreateListenerManager();
  manager->AddEventListenerByType(this,
                                  NS_LITERAL_STRING("DOMContentLoaded"),
                                  TrustedEventsAtSystemGroupBubble());
  return NS_OK;
}

NS_IMETHODIMP
ImportLoader::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  AutoError ae(this);
  nsIPrincipal* principal = Principal();

  nsCOMPtr<nsIChannel> channel = do_QueryInterface(aRequest);
  if (!channel) {
    return NS_ERROR_DOM_ABORT_ERR;
  }

  if (nsContentUtils::IsSystemPrincipal(principal)) {
    // We should never import non-system documents and run their scripts with system principal!
    nsCOMPtr<nsIPrincipal> channelPrincipal;
    nsContentUtils::GetSecurityManager()->GetChannelResultPrincipal(channel,
                                                                    getter_AddRefs(channelPrincipal));
    if (!nsContentUtils::IsSystemPrincipal(channelPrincipal)) {
      return NS_ERROR_FAILURE;
    }
  }
  channel->SetOwner(principal);

  nsAutoCString type;
  channel->GetContentType(type);
  if (!type.EqualsLiteral("text/html")) {
    NS_WARNING("ImportLoader wrong content type");
    return NS_ERROR_DOM_ABORT_ERR;
  }

  // The scope object is same for all the imports in an import tree,
  // let's get it form the import parent.
  nsCOMPtr<nsIGlobalObject> global = mImportParent->GetScopeObject();
  nsCOMPtr<nsIDOMDocument> importDoc;
  nsCOMPtr<nsIURI> baseURI = mImportParent->GetBaseURI();
  const nsAString& emptyStr = EmptyString();
  nsresult rv = NS_NewDOMDocument(getter_AddRefs(importDoc),
                                  emptyStr, emptyStr, nullptr, mURI,
                                  baseURI, principal, false, global,
                                  DocumentFlavorHTML);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  // The imported document must know which master document it belongs to.
  mDocument = do_QueryInterface(importDoc);
  nsCOMPtr<nsIDocument> master = mImportParent->MasterDocument();
  mDocument->SetMasterDocument(master);

  // We have to connect the blank document we created with the channel we opened,
  // and create its own LoadGroup for it.
  nsCOMPtr<nsIStreamListener> listener;
  nsCOMPtr<nsILoadGroup> loadGroup;
  channel->GetLoadGroup(getter_AddRefs(loadGroup));
  nsCOMPtr<nsILoadGroup> newLoadGroup = do_CreateInstance(NS_LOADGROUP_CONTRACTID);
  NS_ENSURE_TRUE(newLoadGroup, NS_ERROR_OUT_OF_MEMORY);
  newLoadGroup->SetLoadGroup(loadGroup);
  rv = mDocument->StartDocumentLoad("import", channel, newLoadGroup,
                                    nullptr, getter_AddRefs(listener),
                                    true);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  // Let's start parser.
  mParserStreamListener = listener;
  rv = listener->OnStartRequest(aRequest, aContext);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  ae.Pass();
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION(ImportManager,
                         mImports)

NS_INTERFACE_MAP_BEGIN(ImportManager)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(ImportManager)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(ImportManager)
NS_IMPL_CYCLE_COLLECTING_RELEASE(ImportManager)

already_AddRefed<ImportLoader>
ImportManager::Get(nsIURI* aURI, nsINode* aNode, nsIDocument* aOrigDocument)
{
  // Check if we have a loader for that URI, if not create one,
  // and start it up.
  nsRefPtr<ImportLoader> loader;
  mImports.Get(aURI, getter_AddRefs(loader));

  if (!loader) {
    loader = new ImportLoader(aURI, aOrigDocument);
    mImports.Put(aURI, loader);
    loader->Open();
  }
  loader->AddLinkElement(aNode);
  MOZ_ASSERT(loader);
  return loader.forget();
}

} // namespace dom
} // namespace mozilla

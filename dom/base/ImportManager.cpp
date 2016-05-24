/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImportManager.h"

#include "mozilla/EventListenerManager.h"
#include "HTMLLinkElement.h"
#include "nsContentPolicyUtils.h"
#include "nsContentUtils.h"
#include "nsIChannel.h"
#include "nsIContentPolicy.h"
#include "nsIContentSecurityPolicy.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMEvent.h"
#include "nsIPrincipal.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsScriptLoader.h"
#include "nsNetUtil.h"

//-----------------------------------------------------------------------------
// AutoError
//-----------------------------------------------------------------------------

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

//-----------------------------------------------------------------------------
// ImportLoader::Updater
//-----------------------------------------------------------------------------

void
ImportLoader::Updater::GetReferrerChain(nsINode* aNode,
                                        nsTArray<nsINode*>& aResult)
{
  // We fill up the array backward. First the last link: aNode.
  MOZ_ASSERT(mLoader->mLinks.Contains(aNode));

  aResult.AppendElement(aNode);
  nsINode* node = aNode;
  RefPtr<ImportManager> manager = mLoader->Manager();
  for (ImportLoader* referrersLoader = manager->Find(node->OwnerDoc());
       referrersLoader;
       referrersLoader = manager->Find(node->OwnerDoc()))
  {
    // Then walking up the main referrer chain and append each link
    // to the array.
    node = referrersLoader->GetMainReferrer();
    MOZ_ASSERT(node);
    aResult.AppendElement(node);
  }

  // The reversed order is more useful for consumers.
  // XXX: This should probably go to nsTArray or some generic utility
  // lib for our containers that we don't have... I would really like to
  // get rid of this part...
  uint32_t l = aResult.Length();
  for (uint32_t i = 0; i < l / 2; i++) {
    Swap(aResult[i], aResult[l - i - 1]);
  }
}

bool
ImportLoader::Updater::ShouldUpdate(nsTArray<nsINode*>& aNewPath)
{
  if (mLoader->Manager()->GetNearestPredecessor(mLoader->GetMainReferrer()) !=
      mLoader->mBlockingPredecessor) {
    return true;
  }
  // Let's walk down on the main referrer chains of both the current main and
  // the new link, and find the last pair of links that are from the same
  // document. This is the junction point between the two referrer chain. Their
  // order in the subimport list of that document will determine if we have to
  // update the spanning tree or this new edge changes nothing in the script
  // execution order.
  nsTArray<nsINode*> oldPath;
  GetReferrerChain(mLoader->mLinks[mLoader->mMainReferrer], oldPath);
  uint32_t max = std::min(oldPath.Length(), aNewPath.Length());
  MOZ_ASSERT(max > 0);
  uint32_t lastCommonImportAncestor = 0;

  for (uint32_t i = 0;
       i < max && oldPath[i]->OwnerDoc() == aNewPath[i]->OwnerDoc();
       i++)
  {
    lastCommonImportAncestor = i;
  }

  MOZ_ASSERT(lastCommonImportAncestor < max);
  nsINode* oldLink = oldPath[lastCommonImportAncestor];
  nsINode* newLink = aNewPath[lastCommonImportAncestor];

  if ((lastCommonImportAncestor == max - 1) &&
      newLink == oldLink ) {
    // If one chain contains the other entirely, then this is a simple cycle,
    // nothing to be done here.
    MOZ_ASSERT(oldPath.Length() != aNewPath.Length(),
               "This would mean that new link == main referrer link");
    return false;
  }

  MOZ_ASSERT(aNewPath != oldPath,
             "How could this happen?");
  nsIDocument* doc = oldLink->OwnerDoc();
  MOZ_ASSERT(doc->HasSubImportLink(newLink));
  MOZ_ASSERT(doc->HasSubImportLink(oldLink));

  return doc->IndexOfSubImportLink(newLink) < doc->IndexOfSubImportLink(oldLink);
}

void
ImportLoader::Updater::UpdateMainReferrer(uint32_t aNewIdx)
{
  MOZ_ASSERT(aNewIdx < mLoader->mLinks.Length());
  nsINode* newMainReferrer = mLoader->mLinks[aNewIdx];

  // This new link means we have to execute our scripts sooner...
  // Let's make sure that unblocking a loader does not trigger a script execution.
  // So we start with placing the new blockers and only then will we remove any
  // blockers.
  if (mLoader->IsBlocking()) {
    // Our import parent is changed, let's block the new one and later unblock
    // the old one.
    newMainReferrer->OwnerDoc()->
      ScriptLoader()->AddParserBlockingScriptExecutionBlocker();
    newMainReferrer->OwnerDoc()->BlockDOMContentLoaded();
  }

  if (mLoader->mDocument) {
    // Our nearest predecessor has changed. So let's add the ScriptLoader to the
    // new one if there is any. And remove it from the old one.
    RefPtr<ImportManager> manager = mLoader->Manager();
    nsScriptLoader* loader = mLoader->mDocument->ScriptLoader();
    ImportLoader*& pred = mLoader->mBlockingPredecessor;
    ImportLoader* newPred = manager->GetNearestPredecessor(newMainReferrer);
    if (pred) {
      if (newPred) {
        newPred->AddBlockedScriptLoader(loader);
      }
      pred->RemoveBlockedScriptLoader(loader);
    }
  }

  if (mLoader->IsBlocking()) {
    mLoader->mImportParent->
      ScriptLoader()->RemoveParserBlockingScriptExecutionBlocker();
    mLoader->mImportParent->UnblockDOMContentLoaded();
  }

  // Finally update mMainReferrer to point to the newly added link.
  mLoader->mMainReferrer = aNewIdx;
  mLoader->mImportParent = newMainReferrer->OwnerDoc();
}

nsINode*
ImportLoader::Updater::NextDependant(nsINode* aCurrentLink,
                                     nsTArray<nsINode*>& aPath,
                                     NodeTable& aVisitedNodes, bool aSkipChildren)
{
  // Depth first graph traversal.
  if (!aSkipChildren) {
    // "first child"
    ImportLoader* loader = mLoader->Manager()->Find(aCurrentLink);
    if (loader && loader->GetDocument()) {
      nsINode* firstSubImport = loader->GetDocument()->GetSubImportLink(0);
      if (firstSubImport && !aVisitedNodes.Contains(firstSubImport)) {
        aPath.AppendElement(aCurrentLink);
        aVisitedNodes.PutEntry(firstSubImport);
        return firstSubImport;
      }
    }
  }

  aPath.AppendElement(aCurrentLink);
  // "(parent's) next sibling"
  while(aPath.Length() > 1) {
    aCurrentLink = aPath[aPath.Length() - 1];
    aPath.RemoveElementAt(aPath.Length() - 1);

    // Let's find the next "sibling"
    ImportLoader* loader =  mLoader->Manager()->Find(aCurrentLink->OwnerDoc());
    MOZ_ASSERT(loader && loader->GetDocument(), "How can this happend?");
    nsIDocument* doc = loader->GetDocument();
    MOZ_ASSERT(doc->HasSubImportLink(aCurrentLink));
    uint32_t idx = doc->IndexOfSubImportLink(aCurrentLink);
    nsINode* next = doc->GetSubImportLink(idx + 1);
    if (next) {
      // Note: If we found an already visited link that means the parent links has
      // closed the circle it's always the "first child" section that should find
      // the first already visited node. Let's just assert that.
      MOZ_ASSERT(!aVisitedNodes.Contains(next));
      aVisitedNodes.PutEntry(next);
      return next;
    }
  }

  return nullptr;
}

void
ImportLoader::Updater::UpdateDependants(nsINode* aNode,
                                        nsTArray<nsINode*>& aPath)
{
  NodeTable visitedNodes;
  nsINode* current = aNode;
  uint32_t initialLength = aPath.Length();
  bool neededUpdate = true;
  while ((current = NextDependant(current, aPath, visitedNodes, !neededUpdate))) {
    if (!current || aPath.Length() <= initialLength) {
      break;
    }
    ImportLoader* loader = mLoader->Manager()->Find(current);
    if (!loader) {
      continue;
    }
    Updater& updater = loader->mUpdater;
    neededUpdate = updater.ShouldUpdate(aPath);
    if (neededUpdate) {
      updater.UpdateMainReferrer(loader->mLinks.IndexOf(current));
    }
  }
}

void
ImportLoader::Updater::UpdateSpanningTree(nsINode* aNode)
{
  if (mLoader->mReady || mLoader->mStopped) {
    // Scripts already executed, nothing to be done here.
    return;
  }

  if (mLoader->mLinks.Length() == 1) {
    // If this is the first referrer, let's mark it.
    mLoader->mMainReferrer = 0;
    return;
  }

  nsTArray<nsINode*> newReferrerChain;
  GetReferrerChain(aNode, newReferrerChain);
  if (ShouldUpdate(newReferrerChain)) {
    UpdateMainReferrer(mLoader->mLinks.Length() - 1);
    UpdateDependants(aNode, newReferrerChain);
  }
}

//-----------------------------------------------------------------------------
// ImportLoader
//-----------------------------------------------------------------------------

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
  , mBlockingPredecessor(nullptr)
  , mReady(false)
  , mStopped(false)
  , mBlockingScripts(false)
  , mUpdater(this)
{
}

void
ImportLoader::BlockScripts()
{
  MOZ_ASSERT(!mBlockingScripts);
  mImportParent->ScriptLoader()->AddParserBlockingScriptExecutionBlocker();
  mImportParent->BlockDOMContentLoaded();
  mBlockingScripts = true;
}

void
ImportLoader::UnblockScripts()
{
  MOZ_ASSERT(mBlockingScripts);
  mImportParent->ScriptLoader()->RemoveParserBlockingScriptExecutionBlocker();
  mImportParent->UnblockDOMContentLoaded();
  for (uint32_t i = 0; i < mBlockedScriptLoaders.Length(); i++) {
    mBlockedScriptLoaders[i]->RemoveParserBlockingScriptExecutionBlocker();
  }
  mBlockedScriptLoaders.Clear();
  mBlockingScripts = false;
}

void
ImportLoader::SetBlockingPredecessor(ImportLoader* aLoader)
{
  mBlockingPredecessor = aLoader;
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
ImportLoader::AddBlockedScriptLoader(nsScriptLoader* aScriptLoader)
{
  if (mBlockedScriptLoaders.Contains(aScriptLoader)) {
    return;
  }

  aScriptLoader->AddParserBlockingScriptExecutionBlocker();

  // Let's keep track of the pending script loaders.
  mBlockedScriptLoaders.AppendElement(aScriptLoader);
}

bool
ImportLoader::RemoveBlockedScriptLoader(nsScriptLoader* aScriptLoader)
{
  aScriptLoader->RemoveParserBlockingScriptExecutionBlocker();
  return mBlockedScriptLoaders.RemoveElement(aScriptLoader);
}

void
ImportLoader::AddLinkElement(nsINode* aNode)
{
  // If a new link element is added to the import tree that
  // refers to an import that is already finished loading or
  // stopped trying, we need to fire the corresponding event
  // on it.
  mLinks.AppendElement(aNode);
  mUpdater.UpdateSpanningTree(aNode);
  DispatchEventIfFinished(aNode);
}

void
ImportLoader::RemoveLinkElement(nsINode* aNode)
{
  mLinks.RemoveElement(aNode);
}

// Events has to be fired with a script runner, so mImport can
// be set on the link element before the load event is fired even
// if ImportLoader::Get returns an already loaded import and we
// fire the load event immediately on the new referring link element.
class AsyncEvent : public Runnable {
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
  uint32_t l = mLinks.Length();
  for (uint32_t i = 0; i < l; i++) {
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
  uint32_t l = mLinks.Length();
  for (uint32_t i = 0; i < l; i++) {
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

  nsCOMPtr<nsILoadGroup> loadGroup =
    mImportParent->MasterDocument()->GetDocumentLoadGroup();

  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                              mURI,
                              mImportParent,
                              nsILoadInfo::SEC_REQUIRE_CORS_DATA_INHERITS,
                              nsIContentPolicy::TYPE_SUBDOCUMENT,
                              loadGroup,
                              nullptr,  // aCallbacks
                              nsIRequest::LOAD_BACKGROUND);

  NS_ENSURE_SUCCESS_VOID(rv);
  rv = channel->AsyncOpen2(this);
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

  // We want to inherit the sandbox flags and fullscreen enabled flag
  // from the master document.
  mDocument->SetSandboxFlags(master->GetSandboxFlags());
  mDocument->SetFullscreenEnabled(master->FullscreenEnabledInternal());

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

  nsCOMPtr<nsIURI> originalURI;
  rv = channel->GetOriginalURI(getter_AddRefs(originalURI));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  nsCOMPtr<nsIURI> URI;
  rv = channel->GetURI(getter_AddRefs(URI));
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);
  MOZ_ASSERT(URI, "URI of a channel should never be null");

  bool equals;
  rv = URI->Equals(originalURI, &equals);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  if (!equals) {
    // In case of a redirection we must add the new URI to the import map.
    Manager()->AddLoaderWithNewURI(this, URI);
  }

  // Let's start the parser.
  mParserStreamListener = listener;
  rv = listener->OnStartRequest(aRequest, aContext);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_DOM_ABORT_ERR);

  ae.Pass();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ImportManager
//-----------------------------------------------------------------------------

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
  RefPtr<ImportLoader> loader;
  mImports.Get(aURI, getter_AddRefs(loader));
  bool needToStart = false;
  if (!loader) {
    loader = new ImportLoader(aURI, aOrigDocument);
    mImports.Put(aURI, loader);
    needToStart = true;
  }

  MOZ_ASSERT(loader);
  // Let's keep track of the sub imports links in each document. It will
  // be used later for scrip execution order calculation. (see UpdateSpanningTree)
  // NOTE: removing and adding back the link to the tree somewhere else will
  // NOT have an effect on script execution order.
  if (!aOrigDocument->HasSubImportLink(aNode)) {
    aOrigDocument->AddSubImportLink(aNode);
  }

  loader->AddLinkElement(aNode);

  if (needToStart) {
    loader->Open();
  }

  return loader.forget();
}

ImportLoader*
ImportManager::Find(nsIDocument* aImport)
{
  return mImports.GetWeak(aImport->GetDocumentURIObject());
}

ImportLoader*
ImportManager::Find(nsINode* aLink)
{
  HTMLLinkElement* linkElement = static_cast<HTMLLinkElement*>(aLink);
  nsCOMPtr<nsIURI> uri = linkElement->GetHrefURI();
  return mImports.GetWeak(uri);
}

void
ImportManager::AddLoaderWithNewURI(ImportLoader* aLoader, nsIURI* aNewURI)
{
  mImports.Put(aNewURI, aLoader);
}

ImportLoader* ImportManager::GetNearestPredecessor(nsINode* aNode)
{
  // Return the previous link if there is any in the same document.
  nsIDocument* doc = aNode->OwnerDoc();
  int32_t idx = doc->IndexOfSubImportLink(aNode);
  MOZ_ASSERT(idx != -1, "aNode must be a sub import link of its owner document");

  for (; idx > 0; idx--) {
    HTMLLinkElement* link =
      static_cast<HTMLLinkElement*>(doc->GetSubImportLink(idx - 1));
    nsCOMPtr<nsIURI> uri = link->GetHrefURI();
    RefPtr<ImportLoader> ret;
    mImports.Get(uri, getter_AddRefs(ret));
    // Only main referrer links are interesting.
    if (ret->GetMainReferrer() == link) {
      return ret;
    }
  }

  if (idx == 0) {
    if (doc->IsMasterDocument()) {
      // If there is no previous one, and it was the master document, then
      // there is no predecessor.
      return nullptr;
    }
    // Else we find the main referrer of the import parent of the link's document.
    // And do a recursion.
    ImportLoader* owner = Find(doc);
    MOZ_ASSERT(owner);
    nsCOMPtr<nsINode> mainReferrer = owner->GetMainReferrer();
    return GetNearestPredecessor(mainReferrer);
  }

  return nullptr;
}

} // namespace dom
} // namespace mozilla

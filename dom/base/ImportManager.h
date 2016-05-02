/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * For each import tree there is one master document (the root) and one
 * import manager. The import manager is a map of URI ImportLoader pairs.
 * An ImportLoader is responsible for loading an import document from a
 * given location, and sending out load or error events to all the link
 * nodes that refer to it when it's done. For loading it opens up a
 * channel, using the same CSP as the master document. It then creates a
 * blank document, and starts parsing the data from the channel. When
 * there is no more data on the channel we wait for the DOMContentLoaded
 * event from the parsed document. For the duration of the loading
 * process the scripts on the parent documents are blocked. When an error
 * occurs, or the DOMContentLoaded event is received, the scripts on the
 * parent document are unblocked and we emit the corresponding event on
 * all the referrer link nodes. If a new link node is added to one of the
 * DOM trees in the import tree that refers to an import that was already
 * loaded, the already existing ImportLoader is being used (without
 * loading the referred import document twice) and if necessary the
 * load/error is emitted on it immediately.
 *
 * Ownership model:
 *
 * ImportDocument ----------------------------
 *  ^                                        |
 *  |                                        v
 *  MasterDocument <- ImportManager <-ImportLoader
 *  ^                                        ^
 *  |                                        |
 *  LinkElement <-----------------------------
 *
 */

#ifndef mozilla_dom_ImportManager_h__
#define mozilla_dom_ImportManager_h__

#include "nsTArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsIStreamListener.h"
#include "nsIWeakReferenceUtils.h"
#include "nsRefPtrHashtable.h"
#include "nsScriptLoader.h"
#include "nsURIHashKey.h"

class nsIDocument;
class nsIPrincipal;
class nsINode;
class AutoError;

namespace mozilla {
namespace dom {

class ImportManager;

typedef nsTHashtable<nsPtrHashKey<nsINode>> NodeTable;

class ImportLoader final : public nsIStreamListener
                         , public nsIDOMEventListener
{

  // A helper inner class to decouple the logic of updating the import graph
  // after a new import link has been found by one of the parsers.
  class Updater {

  public:
    explicit Updater(ImportLoader* aLoader) : mLoader(aLoader)
    {}

    // After a new link is added that refers to this import, we
    // have to update the spanning tree, since given this new link the
    // priority of this import might be higher in the scripts
    // execution order than before. It updates mMainReferrer, mImportParent,
    // the corresponding pending ScriptRunners, etc.
    // It also handles updating additional dependant loaders via the
    // UpdateDependants calls.
    // (NOTE: See GetMainReferrer about spanning tree.)
    void UpdateSpanningTree(nsINode* aNode);

  private:
    // Returns an array of links that forms a referring chain from
    // the master document to this import. Each link in the array
    // is marked as main referrer in the list.
    void GetReferrerChain(nsINode* aNode, nsTArray<nsINode*>& aResult);

    // Once we find a new referrer path to our import, we have to see if
    // it changes the load order hence we have to do an update on the graph.
    bool ShouldUpdate(nsTArray<nsINode*>& aNewPath);
    void UpdateMainReferrer(uint32_t newIdx);

    // It's a depth first graph traversal algorithm, for UpdateDependants. The
    // nodes in the graph are the import link elements, and there is a directed
    // edge from link1 to link2 if link2 is a subimport in the import document
    // of link1.
    // If the ImportLoader that aCurrentLink points to didn't need to be updated
    // the algorithm skips its "children" (subimports). Note, that this graph can
    // also contain cycles, aVisistedLinks is used to track the already visited
    // links to avoid an infinite loop.
    // aPath - (in/out) the referrer link chain of aCurrentLink when called, and
    //                  of the next link when the function returns
    // aVisitedLinks - (in/out) list of links that the traversal already visited
    //                          (to handle cycles in the graph)
    // aSkipChildren - when aCurrentLink points to an import that did not need
    //                 to be updated, we can skip its sub-imports ('children')
    nsINode* NextDependant(nsINode* aCurrentLink,
                           nsTArray<nsINode*>& aPath,
                           NodeTable& aVisitedLinks, bool aSkipChildren);

    // When we find a new link that changes the load order of the known imports,
    // we also have to check all the subimports of it, to see if they need an
    // update too. (see test_imports_nested_2.html)
    void UpdateDependants(nsINode* aNode, nsTArray<nsINode*>& aPath);

    ImportLoader* mLoader;
  };

  friend class ::AutoError;
  friend class ImportManager;
  friend class Updater;

public:
  ImportLoader(nsIURI* aURI, nsIDocument* aOriginDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ImportLoader, nsIStreamListener)
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  // We need to listen to DOMContentLoaded event to know when the document
  // is fully leaded.
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent) override;

  // Validation then opening and starting up the channel.
  void Open();
  void AddLinkElement(nsINode* aNode);
  void RemoveLinkElement(nsINode* aNode);
  bool IsReady() { return mReady; }
  bool IsStopped() { return mStopped; }
  bool IsBlocking() { return mBlockingScripts; }

  ImportManager* Manager() {
    MOZ_ASSERT(mDocument || mImportParent, "One of them should be always set");
    return (mDocument ? mDocument : mImportParent)->ImportManager();
  }

  // Simply getter for the import document. Can return a partially parsed
  // document if called too early.
  nsIDocument* GetDocument()
  {
    return mDocument;
  }

  // Getter for the import document that is used in the spec. Returns
  // nullptr if the import is not yet ready.
  nsIDocument* GetImport()
  {
    if (!mReady) {
      return nullptr;
    }
    return mDocument;
  }

  // There is only one referring link that is marked as primary link per
  // imports. This is the one that has to be taken into account when
  // scrip execution order is determined. Links marked as primary link form
  // a spanning tree in the import graph. (Eliminating the cycles and
  // multiple parents.) This spanning tree is recalculated every time
  // a new import link is added to the manager.
  nsINode* GetMainReferrer()
  {
    if (mLinks.IsEmpty()) {
      return nullptr;
    }
    return mLinks[mMainReferrer];
  }

  // An import is not only blocked by its import children, but also
  // by its predecessors. It's enough to find the closest predecessor
  // and wait for that to run its scripts. We keep track of all the
  // ScriptRunners that are waiting for this import. NOTE: updating
  // the main referrer might change this list.
  void AddBlockedScriptLoader(nsScriptLoader* aScriptLoader);
  bool RemoveBlockedScriptLoader(nsScriptLoader* aScriptLoader);
  void SetBlockingPredecessor(ImportLoader* aLoader);

private:
  ~ImportLoader() {}

  // If a new referrer LinkElement was added, let's
  // see if we are already finished and if so fire
  // the right event.
  void DispatchEventIfFinished(nsINode* aNode);

  // Dispatch event for a single referrer LinkElement.
  void DispatchErrorEvent(nsINode* aNode);
  void DispatchLoadEvent(nsINode* aNode);

  // Must be called when an error has occured during load.
  void Error(bool aUnblockScripts);

  // Must be called when the import document has been loaded successfully.
  void Done();

  // When the reading from the channel and the parsing
  // of the document is done, we can release the resources
  // that we don't need any longer to hold on.
  void ReleaseResources();

  // While the document is being loaded we must block scripts
  // on the import parent document.
  void BlockScripts();
  void UnblockScripts();

  nsIPrincipal* Principal();

  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIStreamListener> mParserStreamListener;
  nsCOMPtr<nsIDocument> mImportParent;
  ImportLoader* mBlockingPredecessor;

  // List of the LinkElements that are referring to this import
  // we need to keep track of them so we can fire event on them.
  nsTArray<nsCOMPtr<nsINode>> mLinks;

  // List of pending ScriptLoaders that are waiting for this import
  // to finish.
  nsTArray<RefPtr<nsScriptLoader>> mBlockedScriptLoaders;

  // There is always exactly one referrer link that is flagged as
  // the main referrer the primary link. This is the one that is
  // used in the script execution order calculation.
  // ("Branch" according to the spec.)
  uint32_t mMainReferrer;
  bool mReady;
  bool mStopped;
  bool mBlockingScripts;
  Updater mUpdater;
};

class ImportManager final : public nsISupports
{
  typedef nsRefPtrHashtable<nsURIHashKey, ImportLoader> ImportMap;

  ~ImportManager() {}

public:
  ImportManager() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ImportManager)

  // Finds the ImportLoader that belongs to aImport in the map.
  ImportLoader* Find(nsIDocument* aImport);

  // Find the ImportLoader aLink refers to.
  ImportLoader* Find(nsINode* aLink);

  void AddLoaderWithNewURI(ImportLoader* aLoader, nsIURI* aNewURI);

  // When a new import link is added, this getter either creates
  // a new ImportLoader for it, or returns an existing one if
  // it was already created and in the import map.
  already_AddRefed<ImportLoader> Get(nsIURI* aURI, nsINode* aNode,
                                     nsIDocument* aOriginDocument);

  // It finds the predecessor for an import link node that runs its
  // scripts the latest among its predecessors.
  ImportLoader* GetNearestPredecessor(nsINode* aNode);

private:
  ImportMap mImports;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImportManager_h__

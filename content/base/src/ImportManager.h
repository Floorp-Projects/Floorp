/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"
#include "nsIDOMEventListener.h"
#include "nsIStreamListener.h"
#include "nsIWeakReferenceUtils.h"
#include "nsRefPtrHashtable.h"
#include "nsURIHashKey.h"

class nsIDocument;
class nsIChannel;
class nsINode;
class AutoError;

namespace mozilla {
namespace dom {

class ImportManager;

class ImportLoader MOZ_FINAL : public nsIStreamListener
                             , public nsIDOMEventListener
{
  friend class ::AutoError;
  friend class ImportManager;

public:
  ImportLoader(nsIURI* aURI, nsIDocument* aOriginDocument);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(ImportLoader, nsIStreamListener)
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUESTOBSERVER

  // We need to listen to DOMContentLoaded event to know when the document
  // is fully leaded.
  NS_IMETHOD HandleEvent(nsIDOMEvent *aEvent) MOZ_OVERRIDE;

  // Validation then opening and starting up the channel.
  void Open();
  void AddLinkElement(nsINode* aNode);
  void RemoveLinkElement(nsINode* aNode);
  bool IsReady() { return mReady; }
  bool IsStopped() { return mStopped; }
  bool IsBlocking() { return mBlockingScripts; }
  already_AddRefed<nsIDocument> GetImport()
  {
    return mReady ? nsCOMPtr<nsIDocument>(mDocument).forget() : nullptr;
  }

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

  nsCOMPtr<nsIDocument> mDocument;
  nsCOMPtr<nsIURI> mURI;
  nsCOMPtr<nsIStreamListener> mParserStreamListener;
  nsCOMPtr<nsIDocument> mImportParent;
  // List of the LinkElements that are referring to this import
  // we need to keep track of them so we can fire event on them.
  nsCOMArray<nsINode> mLinks;
  bool mReady;
  bool mStopped;
  bool mBlockingScripts;
};

class ImportManager MOZ_FINAL : public nsISupports
{
  typedef nsRefPtrHashtable<nsURIHashKey, ImportLoader> ImportMap;

  ~ImportManager() {}

public:
  ImportManager() {}

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(ImportManager)

  already_AddRefed<ImportLoader> Get(nsIURI* aURI, nsINode* aNode,
                                     nsIDocument* aOriginDocument);

private:
  ImportMap mImports;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_ImportManager_h__

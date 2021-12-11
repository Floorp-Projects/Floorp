/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11_DocManager_h_
#define mozilla_a11_DocManager_h_

#include "mozilla/ClearOnShutdown.h"
#include "nsIDOMEventListener.h"
#include "nsRefPtrHashtable.h"
#include "nsIWebProgressListener.h"
#include "nsWeakReference.h"
#include "mozilla/StaticPtr.h"
#include "nsINode.h"

namespace mozilla::dom {
class Document;
}

namespace mozilla {
class PresShell;

namespace a11y {

class LocalAccessible;
class DocAccessible;
class xpcAccessibleDocument;
class DocAccessibleParent;

/**
 * Manage the document accessible life cycle.
 */
class DocManager : public nsIWebProgressListener,
                   public nsIDOMEventListener,
                   public nsSupportsWeakReference {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBPROGRESSLISTENER
  NS_DECL_NSIDOMEVENTLISTENER

  /**
   * Return document accessible for the given DOM node.
   */
  DocAccessible* GetDocAccessible(dom::Document* aDocument);

  /**
   * Return document accessible for the given presshell.
   */
  DocAccessible* GetDocAccessible(const PresShell* aPresShell);

  /**
   * Search through all document accessibles for an accessible with the given
   * unique id.
   */
  LocalAccessible* FindAccessibleInCache(nsINode* aNode) const;

  /**
   * Called by document accessible when it gets shutdown.
   * @param aAllowServiceShutdown true to shut down nsAccessibilityService
   *        if it is no longer required, false to prevent it.
   */
  void NotifyOfDocumentShutdown(DocAccessible* aDocument,
                                dom::Document* aDOMDocument,
                                bool aAllowServiceShutdown = true);

  void RemoveFromXPCDocumentCache(DocAccessible* aDocument,
                                  bool aAllowServiceShutdown = true);

  /**
   * Return XPCOM accessible document.
   */
  xpcAccessibleDocument* GetXPCDocument(DocAccessible* aDocument);
  xpcAccessibleDocument* GetCachedXPCDocument(DocAccessible* aDocument) const {
    return mXPCDocumentCache.GetWeak(aDocument);
  }

  /*
   * Notification that a top level document in a content process has gone away.
   */
  static void RemoteDocShutdown(DocAccessibleParent* aDoc) {
    DebugOnly<bool> result = sRemoteDocuments->RemoveElement(aDoc);
    MOZ_ASSERT(result, "Why didn't we find the document!");
  }

  /*
   * Notify of a new top level document in a content process.
   */
  static void RemoteDocAdded(DocAccessibleParent* aDoc);

  static const nsTArray<DocAccessibleParent*>* TopLevelRemoteDocs() {
    return sRemoteDocuments;
  }

  /**
   * Remove the xpc document for a remote document if there is one.
   */
  static void NotifyOfRemoteDocShutdown(DocAccessibleParent* adoc);

  static void RemoveFromRemoteXPCDocumentCache(DocAccessibleParent* aDoc);

  /**
   * Get a XPC document for a remote document.
   */
  static xpcAccessibleDocument* GetXPCDocument(DocAccessibleParent* aDoc);
  static xpcAccessibleDocument* GetCachedXPCDocument(
      const DocAccessibleParent* aDoc) {
    return sRemoteXPCDocumentCache ? sRemoteXPCDocumentCache->GetWeak(aDoc)
                                   : nullptr;
  }

#ifdef DEBUG
  bool IsProcessingRefreshDriverNotification() const;
#endif

 protected:
  DocManager();
  virtual ~DocManager() = default;

  /**
   * Initialize the manager.
   */
  bool Init();

  /**
   * Shutdown the manager.
   */
  void Shutdown();

  bool HasXPCDocuments() {
    return mXPCDocumentCache.Count() > 0 ||
           (sRemoteXPCDocumentCache && sRemoteXPCDocumentCache->Count() > 0);
  }

 private:
  DocManager(const DocManager&);
  DocManager& operator=(const DocManager&);

 private:
  /**
   * Create an accessible document if it was't created and fire accessibility
   * events if needed.
   *
   * @param  aDocument       [in] loaded DOM document
   * @param  aLoadEventType  [in] specifies the event type to fire load event,
   *                           if 0 then no event is fired
   */
  void HandleDOMDocumentLoad(dom::Document* aDocument, uint32_t aLoadEventType);

  /**
   * Add/remove 'pagehide' and 'DOMContentLoaded' event listeners.
   */
  void AddListeners(dom::Document* aDocument, bool aAddPageShowListener);
  void RemoveListeners(dom::Document* aDocument);

  /**
   * Create document or root accessible.
   */
  DocAccessible* CreateDocOrRootAccessible(dom::Document* aDocument);

  /**
   * Clear the cache and shutdown the document accessibles.
   */
  void ClearDocCache();

  typedef nsRefPtrHashtable<nsPtrHashKey<const dom::Document>, DocAccessible>
      DocAccessibleHashtable;
  DocAccessibleHashtable mDocAccessibleCache;

  typedef nsRefPtrHashtable<nsPtrHashKey<const DocAccessible>,
                            xpcAccessibleDocument>
      XPCDocumentHashtable;
  XPCDocumentHashtable mXPCDocumentCache;
  static StaticAutoPtr<nsRefPtrHashtable<
      nsPtrHashKey<const DocAccessibleParent>, xpcAccessibleDocument>>
      sRemoteXPCDocumentCache;

  /*
   * The list of remote top level documents.
   */
  static StaticAutoPtr<nsTArray<DocAccessibleParent*>> sRemoteDocuments;
};

/**
 * Return the existing document accessible for the document if any.
 * Note this returns the doc accessible for the primary pres shell if there is
 * more than one.
 */
DocAccessible* GetExistingDocAccessible(const dom::Document* aDocument);

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11_DocManager_h_

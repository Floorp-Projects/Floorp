/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IDTracker_h_
#define mozilla_dom_IDTracker_h_

#include "mozilla/Attributes.h"
#include "nsIObserver.h"
#include "nsThreadUtils.h"

class nsAtom;
class nsIContent;
class nsINode;
class nsIURI;
class nsIReferrerInfo;

namespace mozilla {
namespace dom {

class Document;
class DocumentOrShadowRoot;
class Element;

/**
 * Class to track what element is referenced by a given ID.
 *
 * To use it, call one of the Reset methods to set it up to watch a given ID.
 * Call get() anytime to determine the referenced element (which may be null if
 * the element isn't found). When the element changes, ElementChanged
 * will be called, so subclass this class if you want to receive that
 * notification. ElementChanged runs at safe-for-script time, i.e. outside
 * of the content update. Call Unlink() if you want to stop watching
 * for changes (get() will then return null).
 *
 * By default this is a single-shot tracker --- i.e., when ElementChanged
 * fires, we will automatically stop tracking. get() will continue to return
 * the changed-to element.
 * Override IsPersistent to return true if you want to keep tracking after
 * the first change.
 */
class IDTracker {
 public:
  using Element = mozilla::dom::Element;

  IDTracker();

  ~IDTracker();

  /**
   * Find which element, if any, is referenced.
   */
  Element* get() { return mElement; }

  /**
   * Set up the reference. This can be called multiple times to
   * change which reference is being tracked, but these changes
   * do not trigger ElementChanged.
   * @param aFrom the source element for context
   * @param aURI the URI containing a hash-reference to the element
   * @param aReferrerInfo the referrerInfo for loading external resource
   * @param aWatch if false, then we do not set up the notifications to track
   * changes, so ElementChanged won't fire and get() will always return the same
   * value, the current element for the ID.
   * @param aReferenceImage whether the ID references image elements which are
   * subject to the document's mozSetImageElement overriding mechanism.
   */
  void ResetToURIFragmentID(nsIContent* aFrom, nsIURI* aURI,
                            nsIReferrerInfo* aReferrerInfo, bool aWatch = true,
                            bool aReferenceImage = false);

  /**
   * A variation on ResetToURIFragmentID() to set up a reference that consists
   * of the ID of an element in the same document as aFrom.
   * @param aFrom the source element for context
   * @param aID the ID of the element
   * @param aWatch if false, then we do not set up the notifications to track
   * changes, so ElementChanged won't fire and get() will always return the same
   * value, the current element for the ID.
   */
  void ResetWithID(Element& aFrom, nsAtom* aID, bool aWatch = true);

  /**
   * Clears the reference. ElementChanged is not triggered. get() will return
   * null.
   */
  void Unlink();

  void Traverse(nsCycleCollectionTraversalCallback* aCB);

 protected:
  /**
   * Override this to be notified of element changes. Don't forget
   * to call this superclass method to change mElement. This is called
   * at script-runnable time.
   */
  virtual void ElementChanged(Element* aFrom, Element* aTo);

  /**
   * Override this to convert from a single-shot notification to
   * a persistent notification.
   */
  virtual bool IsPersistent() { return false; }

  /**
   * Set ourselves up with our new document.  Note that aDocument might be
   * null.  Either aWatch must be false or aRef must be empty.
   */
  void HaveNewDocumentOrShadowRoot(DocumentOrShadowRoot*, bool aWatch,
                                   const nsString& aRef);

 private:
  static bool Observe(Element* aOldElement, Element* aNewElement, void* aData);

  class Notification : public nsISupports {
   public:
    virtual void SetTo(Element* aTo) = 0;
    virtual void Clear() { mTarget = nullptr; }
    virtual ~Notification() = default;

   protected:
    explicit Notification(IDTracker* aTarget) : mTarget(aTarget) {
      MOZ_ASSERT(aTarget, "Must have a target");
    }
    IDTracker* mTarget;
  };

  class ChangeNotification : public mozilla::Runnable, public Notification {
   public:
    ChangeNotification(IDTracker* aTarget, Element* aFrom, Element* aTo);

    // We need to actually declare all of nsISupports, because
    // Notification inherits from it but doesn't declare it.
    NS_DECL_ISUPPORTS_INHERITED
    NS_IMETHOD Run() override {
      if (mTarget) {
        mTarget->mPendingNotification = nullptr;
        mTarget->ElementChanged(mFrom, mTo);
      }
      return NS_OK;
    }
    void SetTo(Element* aTo) override;
    void Clear() override;

   protected:
    virtual ~ChangeNotification();

    RefPtr<Element> mFrom;
    RefPtr<Element> mTo;
  };
  friend class ChangeNotification;

  class DocumentLoadNotification : public Notification, public nsIObserver {
   public:
    DocumentLoadNotification(IDTracker* aTarget, const nsString& aRef)
        : Notification(aTarget) {
      if (!mTarget->IsPersistent()) {
        mRef = aRef;
      }
    }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
   private:
    virtual ~DocumentLoadNotification() = default;

    virtual void SetTo(Element* aTo) override {}

    nsString mRef;
  };
  friend class DocumentLoadNotification;

  DocumentOrShadowRoot* GetWatchDocOrShadowRoot() const;

  RefPtr<nsAtom> mWatchID;
  nsCOMPtr<nsINode>
      mWatchDocumentOrShadowRoot;  // Always a `DocumentOrShadowRoot`.
  RefPtr<Element> mElement;
  RefPtr<Notification> mPendingNotification;
  bool mReferencingImage = false;
};

inline void ImplCycleCollectionUnlink(IDTracker& aField) { aField.Unlink(); }

inline void ImplCycleCollectionTraverse(
    nsCycleCollectionTraversalCallback& aCallback, IDTracker& aField,
    const char* aName, uint32_t aFlags = 0) {
  aField.Traverse(&aCallback);
}

}  // namespace dom
}  // namespace mozilla

#endif /* mozilla_dom_IDTracker_h_ */

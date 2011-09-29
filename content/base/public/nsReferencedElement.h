/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=78: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan   <robert@ocallahan.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSREFERENCEDELEMENT_H_
#define NSREFERENCEDELEMENT_H_

#include "mozilla/dom/Element.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

class nsIURI;
class nsCycleCollectionCallback;

/**
 * Class to track what element is referenced by a given ID.
 * 
 * To use it, call Reset() to set it up to watch a given URI. Call get()
 * anytime to determine the referenced element (which may be null if
 * the element isn't found). When the element changes, ElementChanged
 * will be called, so subclass this class if you want to receive that
 * notification. ElementChanged runs at safe-for-script time, i.e. outside
 * of the content update. Call Unlink() if you want to stop watching
 * for changes (get() will then return null).
 *
 * By default this is a single-shot tracker --- i.e., when ElementChanged
 * fires, we will automatically stop tracking. get() will continue to return
 * the changed-to element.
 * Override IsPersistent to return PR_TRUE if you want to keep tracking after
 * the first change.
 */
class nsReferencedElement {
public:
  typedef mozilla::dom::Element Element;

  nsReferencedElement() {}
  ~nsReferencedElement() {
    Unlink();
  }

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
   * @param aWatch if false, then we do not set up the notifications to track
   * changes, so ElementChanged won't fire and get() will always return the same
   * value, the current element for the ID.
   * @param aReferenceImage whether the ID references image elements which are
   * subject to the document's mozSetImageElement overriding mechanism.
   */
  void Reset(nsIContent* aFrom, nsIURI* aURI, bool aWatch = true,
             bool aReferenceImage = false);

  /**
   * A variation on Reset() to set up a reference that consists of the ID of
   * an element in the same document as aFrom.
   * @param aFrom the source element for context
   * @param aID the ID of the element
   * @param aWatch if false, then we do not set up the notifications to track
   * changes, so ElementChanged won't fire and get() will always return the same
   * value, the current element for the ID.
   */
  void ResetWithID(nsIContent* aFrom, const nsString& aID,
                   bool aWatch = true);

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
  virtual void ElementChanged(Element* aFrom, Element* aTo) {
    mElement = aTo;
  }

  /**
   * Override this to convert from a single-shot notification to
   * a persistent notification.
   */
  virtual bool IsPersistent() { return false; }

  /**
   * Set ourselves up with our new document.  Note that aDocument might be
   * null.  Either aWatch must be false or aRef must be empty.
   */
  void HaveNewDocument(nsIDocument* aDocument, bool aWatch,
                       const nsString& aRef);
  
private:
  static bool Observe(Element* aOldElement,
                        Element* aNewElement, void* aData);

  class Notification : public nsISupports {
  public:
    virtual void SetTo(Element* aTo) = 0;
    virtual void Clear() { mTarget = nsnull; }
    virtual ~Notification() {}
  protected:
    Notification(nsReferencedElement* aTarget)
      : mTarget(aTarget)
    {
      NS_PRECONDITION(aTarget, "Must have a target");
    }
    nsReferencedElement* mTarget;
  };

  class ChangeNotification : public nsRunnable,
                             public Notification
  {
  public:
    ChangeNotification(nsReferencedElement* aTarget,
                       Element* aFrom, Element* aTo)
      : Notification(aTarget), mFrom(aFrom), mTo(aTo)
    {}
    virtual ~ChangeNotification() {}

    NS_DECL_ISUPPORTS_INHERITED
    NS_IMETHOD Run() {
      if (mTarget) {
        mTarget->mPendingNotification = nsnull;
        mTarget->ElementChanged(mFrom, mTo);
      }
      return NS_OK;
    }
    virtual void SetTo(Element* aTo) { mTo = aTo; }
    virtual void Clear()
    {
      Notification::Clear(); mFrom = nsnull; mTo = nsnull;
    }
  protected:
    nsRefPtr<Element> mFrom;
    nsRefPtr<Element> mTo;
  };
  friend class ChangeNotification;

  class DocumentLoadNotification : public Notification,
                                   public nsIObserver
  {
  public:
    DocumentLoadNotification(nsReferencedElement* aTarget,
                             const nsString& aRef) :
      Notification(aTarget)
    {
      if (!mTarget->IsPersistent()) {
        mRef = aRef;
      }
    }
    virtual ~DocumentLoadNotification() {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIOBSERVER
  private:
    virtual void SetTo(Element* aTo) { }

    nsString mRef;
  };
  friend class DocumentLoadNotification;
  
  nsCOMPtr<nsIAtom>      mWatchID;
  nsCOMPtr<nsIDocument>  mWatchDocument;
  nsRefPtr<Element> mElement;
  nsRefPtr<Notification> mPendingNotification;
  bool                   mReferencingImage;
};

#endif /*NSREFERENCEDELEMENT_H_*/

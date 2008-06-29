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

#include "nsIContent.h"
#include "nsIAtom.h"
#include "nsIDocument.h"
#include "nsThreadUtils.h"
#include "nsAutoPtr.h"

class nsIURI;
class nsCycleCollectionCallback;

/**
 * Class to track what content is referenced by a given ID.
 * 
 * By default this is a single-shot tracker --- i.e., when ContentChanged
 * fires, we will automatically stop tracking. Override IsPersistent
 * to return PR_TRUE if you want to keep tracking after the content for
 * an ID has changed.
 */
class nsReferencedElement {
public:
  nsReferencedElement() {}
  ~nsReferencedElement() {
    Unlink();
    if (mPendingNotification) {
      mPendingNotification->Clear();
    }
  }
  nsIContent* get() { return mContent; }

  void Reset(nsIContent* aFrom, nsIURI* aURI, PRBool aWatch = PR_TRUE);
  void Unlink();

  void Traverse(nsCycleCollectionTraversalCallback* aCB);
  
protected:
  /**
   * Override this to be notified of content changes. Don't forget
   * to call this method to change mContent.
   */
  virtual void ContentChanged(nsIContent* aFrom, nsIContent* aTo) {
    mContent = aTo;
  }

  /**
   * Override this to convert from a single-shot notification to
   * a persistent notification.
   */
  virtual PRBool IsPersistent() { return PR_FALSE; }
  
private:
  static PRBool Observe(nsIContent* aOldContent,
                        nsIContent* aNewContent, void* aData);

  class Notification : public nsRunnable {
  public:
    Notification(nsReferencedElement* aTarget, nsIContent* aFrom, nsIContent* aTo)
      : mTarget(aTarget), mFrom(aFrom), mTo(aTo) {}
    NS_IMETHOD Run() {
      if (mTarget) {
        mTarget->mPendingNotification = nsnull;
        mTarget->ContentChanged(mFrom, mTo);
      }
      return NS_OK;
    }
    void SetTo(nsIContent* aTo) { mTo = aTo; }
    void Clear() { mTarget = nsnull; mFrom = nsnull; mTo = nsnull; }
  private:
    nsReferencedElement* mTarget;
    nsCOMPtr<nsIContent> mFrom;
    nsCOMPtr<nsIContent> mTo;
  };
  friend class Notification;

  nsCOMPtr<nsIAtom>      mWatchID;
  nsCOMPtr<nsIDocument>  mWatchDocument;
  nsCOMPtr<nsIContent>   mContent;
  nsRefPtr<Notification> mPendingNotification;
};

#endif /*NSREFERENCEDELEMENT_H_*/

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim: ft=cpp tw=78 sw=2 et ts=2
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Boris Zbarsky <bzbarsky@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsImageLoadingContent_h__
#define nsImageLoadingContent_h__

#include "nsIImageLoadingContent.h"
#include "imgIRequest.h"
#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsString.h"

/**
 * This is an implementation of nsIImageLoadingContent.  This class
 * can be subclassed by various content nodes that want to provide
 * image-loading functionality (eg <img>, <object>, etc).
 */

class nsIURI;
class nsIDocument;
class imgILoader;
class nsIIOService;

class nsImageLoadingContent : public nsIImageLoadingContent
{
  /* METHODS */
public:
  nsImageLoadingContent();
  virtual ~nsImageLoadingContent();

  /** Called on layout startup to initialize statics */
  static void Initialize();
  /** Called on shutdown to free statics */
  static void Shutdown();

  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_NSIIMAGELOADINGCONTENT

protected:
  /**
   * ImageURIChanged is called by subclasses when the appropriate
   * attributes (eg 'src' for <img> tags) change.  The string passed
   * in is the new uri string; this consolidates the code for getting
   * the charset and any other incidentals into this superclass.
   *
   * Note that this is different from the ImageURIChanged(AString)
   * declared in nsIImageLoadingContent.idl -- that takes an
   * nsAString, this takes an nsACString.
   *
   * @param aNewURI the URI spec to be loaded (may be a relative URI)
   */
  nsresult ImageURIChanged(const nsACString& aNewURI);

private:
  /**
   * Struct used to manage the image observers.
   */
  struct ImageObserver {
    MOZ_DECL_CTOR_COUNTER(ImageObserver)
    
    ImageObserver(imgIDecoderObserver* aObserver) :
      mObserver(aObserver),
      mNext(nsnull)
    {
      MOZ_COUNT_CTOR(ImageObserver);
    }
    ~ImageObserver()
    {
      MOZ_COUNT_DTOR(ImageObserver);
      delete mNext;
    }

    nsCOMPtr<imgIDecoderObserver> mObserver;
    ImageObserver* mNext;
  };

  /**
   * CancelImageRequests can be called when we want to cancel the
   * image requests.  The "current" request will be canceled only if
   * it has not progressed far enough to know the image size yet unless
   * aEvenIfSizeAvailable is true.
   *
   * @param aReason the reason the requests are being canceled
   * @param aEvenIfSizeAvailable cancels the current load even if its size is
   *                             available
   */
  void CancelImageRequests(nsresult aReason, PRBool aEvenIfSizeAvailable);

  /**
   * helper to get the document for this content (from the nodeinfo
   * and such).  Not named GetDocument to prevent ambiguous method
   * names in subclasses (though why this private method leads to
   * ambiguity is not clear to me....).
   *
   * @return the document we belong to
   */
  nsIDocument* GetOurDocument();

  /**
   * Method to create an nsIURI object from the given string (will
   * handle getting the right charset, base, etc).  You MUST pass in a
   * non-null document to this function.
   *
   * @param aSpec the string spec (from an HTML attribute, eg)
   * @param aDocument the document we belong to
   * @return the URI we want to be loading
   */
  nsresult StringToURI(const nsACString& aSpec, nsIDocument* aDocument,
                       nsIURI** aURI);

  /**
   * Method to fire an event once we know what's going on with the image load.
   *
   * @param aEventType "load" or "error" depending on how things went
   */
  nsresult FireEvent(const nsAString& aEventType);

  /* MEMBERS */
protected:
  nsCOMPtr<imgIRequest> mCurrentRequest;
  nsCOMPtr<imgIRequest> mPendingRequest;
  nsCOMPtr<nsIURI>      mCurrentURI;

  // Cache for the io service and image loader
  static imgILoader* sImgLoader;
  static nsIIOService* sIOService;
private:
  /**
   * Typically we will have only one observer (our frame in the screen
   * prescontext), so we want to only make space for one and to
   * heap-allocate anything past that (saves memory and malloc churn
   * in the common case).  The storage is a linked list, we just
   * happen to actually hold the first observer instead of a pointer
   * to it.
   */
  ImageObserver mObserverList;

  PRPackedBool mLoadingEnabled;
  PRPackedBool mImageIsBlocked;
  PRPackedBool mHaveHadObserver;
};

#endif // nsImageLoadingContent_h__

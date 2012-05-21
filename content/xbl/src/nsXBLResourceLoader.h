/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsCOMArray.h"
#include "nsContentUtils.h"
#include "nsCycleCollectionParticipant.h"

class nsIContent;
class nsIAtom;
class nsIDocument;
class nsIScriptContext;
class nsSupportsHashtable;
class nsXBLPrototypeResources;
class nsXBLPrototypeBinding;

// *********************************************************************/
// The XBLResourceLoader class

struct nsXBLResource {
  nsXBLResource* mNext;
  nsIAtom* mType;
  nsString mSrc;

  nsXBLResource(nsIAtom* aType, const nsAString& aSrc) {
    MOZ_COUNT_CTOR(nsXBLResource);
    mNext = nsnull;
    mType = aType;
    mSrc = aSrc;
  }

  ~nsXBLResource() { 
    MOZ_COUNT_DTOR(nsXBLResource);  
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLResource, this, mNext);
  }
};

class nsXBLResourceLoader : public nsICSSLoaderObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsXBLResourceLoader)

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsCSSStyleSheet* aSheet, bool aWasAlternate,
                              nsresult aStatus);

  void LoadResources(bool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);

  nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                      nsXBLPrototypeResources* aResources);
  virtual ~nsXBLResourceLoader();

  void NotifyBoundElements();

  nsresult Write(nsIObjectOutputStream* aStream);

// MEMBER VARIABLES
  nsXBLPrototypeBinding* mBinding; // A pointer back to our binding.
  nsXBLPrototypeResources* mResources; // A pointer back to our resources
                                       // information.  May be null if the
                                       // resources have already been
                                       // destroyed.
  
  nsXBLResource* mResourceList; // The list of resources we need to load.
  nsXBLResource* mLastResource;

  bool mLoadingResources;
  // We need mInLoadResourcesFunc because we do a mixture of sync and
  // async loads.
  bool mInLoadResourcesFunc;
  PRInt16 mPendingSheets; // The number of stylesheets that have yet to load.

  // Bound elements that are waiting on the stylesheets and scripts.
  nsCOMArray<nsIContent> mBoundElements;
};


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXBLResourceLoader_h
#define nsXBLResourceLoader_h

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsCOMArray.h"
#include "nsCycleCollectionParticipant.h"

class nsIContent;
class nsIAtom;
class nsXBLPrototypeResources;
class nsXBLPrototypeBinding;
struct nsXBLResource;
class nsIObjectOutputStream;

// *********************************************************************/
// The XBLResourceLoader class

class nsXBLResourceLoader : public nsICSSLoaderObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsXBLResourceLoader)

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(mozilla::CSSStyleSheet* aSheet,
                              bool aWasAlternate,
                              nsresult aStatus) MOZ_OVERRIDE;

  void LoadResources(bool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);

  nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                      nsXBLPrototypeResources* aResources);

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
  int16_t mPendingSheets; // The number of stylesheets that have yet to load.

  // Bound elements that are waiting on the stylesheets and scripts.
  nsCOMArray<nsIContent> mBoundElements;

protected:
  virtual ~nsXBLResourceLoader();
};

#endif

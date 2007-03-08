/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Original Author: David W. Hyatt (hyatt@netscape.com)
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

#include "nsCOMPtr.h"
#include "nsICSSLoaderObserver.h"
#include "nsISupportsArray.h"
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
    delete mNext; 
  }
};

class nsXBLResourceLoader : public nsICSSLoaderObserver
{
public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsXBLResourceLoader)

  // nsICSSLoaderObserver
  NS_IMETHOD StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aWasAlternate,
                              nsresult aStatus);

  void LoadResources(PRBool* aResult);
  void AddResource(nsIAtom* aResourceType, const nsAString& aSrc);
  void AddResourceListener(nsIContent* aElement);

  nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                      nsXBLPrototypeResources* aResources);
  virtual ~nsXBLResourceLoader();

  void NotifyBoundElements();

// MEMBER VARIABLES
  nsXBLPrototypeBinding* mBinding; // A pointer back to our binding.
  nsXBLPrototypeResources* mResources; // A pointer back to our resources
                                       // information.  May be null if the
                                       // resources have already been
                                       // destroyed.
  
  nsXBLResource* mResourceList; // The list of resources we need to load.
  nsXBLResource* mLastResource;

  PRPackedBool mLoadingResources;
  // We need mInLoadResourcesFunc because we do a mixture of sync and
  // async loads.
  PRPackedBool mInLoadResourcesFunc;
  PRInt16 mPendingSheets; // The number of stylesheets that have yet to load.
  
  nsCOMPtr<nsISupportsArray> mBoundElements; // Bound elements that are waiting on the stylesheets and scripts.
};


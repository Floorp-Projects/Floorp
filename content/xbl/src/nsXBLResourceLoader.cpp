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

#include "nsICSSStyleSheet.h"
#include "nsIStyleRuleProcessor.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsIXBLService.h"
#include "nsIServiceManager.h"
#include "nsXBLResourceLoader.h"
#include "nsXBLPrototypeResources.h"
#include "nsIDocumentObserver.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "nsICSSLoader.h"
#include "nsIXBLDocumentInfo.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsXBLAtoms.h"
#include "nsFrameManager.h"
#include "nsStyleContext.h"
#include "nsXBLPrototypeBinding.h"

NS_IMPL_ISUPPORTS1(nsXBLResourceLoader, nsICSSLoaderObserver)

nsXBLResourceLoader::nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                                         nsXBLPrototypeResources* aResources)
:mBinding(aBinding),
 mResources(aResources),
 mResourceList(nsnull),
 mLastResource(nsnull),
 mLoadingResources(PR_FALSE),
 mInLoadResourcesFunc(PR_FALSE),
 mPendingSheets(0)
{
}

nsXBLResourceLoader::~nsXBLResourceLoader()
{
  delete mResourceList;
}

void
nsXBLResourceLoader::LoadResources(PRBool* aResult)
{
  mInLoadResourcesFunc = PR_TRUE;

  if (mLoadingResources) {
    *aResult = (mPendingSheets == 0);
    mInLoadResourcesFunc = PR_FALSE;
    return;
  }

  mLoadingResources = PR_TRUE;
  *aResult = PR_TRUE;

  // Declare our loaders.
  nsCOMPtr<imgILoader> il;
  nsCOMPtr<nsICSSLoader> cssLoader;

  nsCOMPtr<nsIDocument> doc;
  mBinding->XBLDocumentInfo()->GetDocument(getter_AddRefs(doc));

  nsIURI *docURL = doc->GetDocumentURI();

  nsCOMPtr<nsIURI> url;

  for (nsXBLResource* curr = mResourceList; curr; curr = curr->mNext) {
    if (curr->mSrc.IsEmpty())
      continue;

    if (NS_FAILED(NS_NewURI(getter_AddRefs(url), curr->mSrc, nsnull, docURL)))
      continue;

    if (curr->mType == nsXBLAtoms::image) {
      // Obtain our src attribute.  
      // Construct a URI out of our src attribute.
      // We need to ensure the image loader is constructed.
      if (!il) {
        il = do_GetService("@mozilla.org/image/loader;1");
        if (!il) continue;
      }

      // Now kick off the image load...
      // Passing NULL for pretty much everything -- cause we don't care!
      // XXX: initialDocumentURI is NULL! 
      nsCOMPtr<imgIRequest> req;
      il->LoadImage(url, nsnull, nsnull, nsnull, nsnull, nsnull, nsIRequest::LOAD_BACKGROUND, nsnull, nsnull, getter_AddRefs(req));
    }
    else if (curr->mType == nsXBLAtoms::stylesheet) {
      if (!cssLoader) {
        cssLoader = doc->GetCSSLoader();
      }

      if (!cssLoader)
        continue;

      // Kick off the load of the stylesheet.

      // Always load chrome synchronously
      PRBool chrome;
      nsresult rv;
      if (NS_SUCCEEDED(url->SchemeIs("chrome", &chrome)) && chrome)
      {
        nsCOMPtr<nsICSSStyleSheet> sheet;
        rv = cssLoader->LoadAgentSheet(url, getter_AddRefs(sheet));
        NS_ASSERTION(NS_SUCCEEDED(rv), "Load failed!!!");
        if (NS_SUCCEEDED(rv))
        {
          rv = StyleSheetLoaded(sheet, PR_TRUE);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Processing the style sheet failed!!!");
        }
      }
      else
      {
        PRBool doneLoading;
        NS_NAMED_LITERAL_STRING(empty, "");
        rv = cssLoader->LoadStyleLink(nsnull, url, empty, empty,
                                      nsnull, doneLoading, this);
        NS_ASSERTION(NS_SUCCEEDED(rv), "Load failed!!!");

        if (!doneLoading)
          mPendingSheets++;
      }
    }
  }

  *aResult = (mPendingSheets == 0);
  mInLoadResourcesFunc = PR_FALSE;
  
  // Destroy our resource list.
  delete mResourceList;
  mResourceList = nsnull;
}

// nsICSSLoaderObserver
NS_IMETHODIMP
nsXBLResourceLoader::StyleSheetLoaded(nsICSSStyleSheet* aSheet, PRBool aNotify)
{
  if (!mResources) {
    // Our resources got destroyed -- just bail out
    return NS_OK;
  }
   
  mResources->mStyleSheetList.AppendObject(aSheet);

  if (!mInLoadResourcesFunc)
    mPendingSheets--;
  
  if (mPendingSheets == 0) {
    // All stylesheets are loaded.  
    nsCOMPtr<nsIStyleRuleProcessor> prevProcessor;
    mResources->mRuleProcessors.Clear();
    PRInt32 count = mResources->mStyleSheetList.Count();
    for (PRInt32 i = 0; i < count; i++) {
      nsICSSStyleSheet* sheet = mResources->mStyleSheetList[i];

      nsCOMPtr<nsIStyleRuleProcessor> processor;
      sheet->GetStyleRuleProcessor(*getter_AddRefs(processor), prevProcessor);
      if (processor != prevProcessor) {
        mResources->mRuleProcessors.AppendObject(processor);
        prevProcessor = processor;
      }
    }

    // XXX Check for mPendingScripts when scripts also come online.
    if (!mInLoadResourcesFunc)
      NotifyBoundElements();
  }
  return NS_OK;
}

void 
nsXBLResourceLoader::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  nsXBLResource* res = new nsXBLResource(aResourceType, aSrc);
  if (!res)
    return;

  if (!mResourceList)
    mResourceList = res;
  else
    mLastResource->mNext = res;

  mLastResource = res;
}

void
nsXBLResourceLoader::AddResourceListener(nsIContent* aBoundElement) 
{
  if (!mBoundElements) {
    NS_NewISupportsArray(getter_AddRefs(mBoundElements));
    if (!mBoundElements)
      return;
  }

  mBoundElements->AppendElement(aBoundElement);
}

void
nsXBLResourceLoader::NotifyBoundElements()
{
  nsCOMPtr<nsIXBLService> xblService(do_GetService("@mozilla.org/xbl;1"));
  nsIURI* bindingURI = mBinding->BindingURI();

  PRUint32 eltCount;
  mBoundElements->Count(&eltCount);
  for (PRUint32 j = 0; j < eltCount; j++) {
    nsCOMPtr<nsIContent> content(do_QueryElementAt(mBoundElements, j));
    
    PRBool ready = PR_FALSE;
    xblService->BindingReady(content, bindingURI, &ready);

    if (ready) {
      nsCOMPtr<nsIDocument> doc = content->GetDocument();
    
      if (doc) {
        // Flush first to make sure we can get the frame for content
        doc->FlushPendingNotifications(Flush_Frames);

        // Notify
        nsCOMPtr<nsIContent> parent = content->GetParent();
        PRInt32 index = 0;
        if (parent)
          index = parent->IndexOf(content);
        
        // If |content| is (in addition to having binding |mBinding|)
        // also a descendant of another element with binding |mBinding|,
        // then we might have just constructed it due to the
        // notification of its parent.  (We can know about both if the
        // binding loads were triggered from the DOM rather than frame
        // construction.)  So we have to check both whether the element
        // has a primary frame and whether it's in the undisplayed map
        // before sending a ContentInserted notification, or bad things
        // will happen.
        nsIPresShell *shell = doc->GetShellAt(0);
        if (shell) {
          nsIFrame* childFrame;
          shell->GetPrimaryFrameFor(content, &childFrame);
          if (!childFrame) {
            // Check to see if it's in the undisplayed content map.
            nsStyleContext* sc =
              shell->FrameManager()->GetUndisplayedContent(content);

            if (!sc) {
              nsCOMPtr<nsIDocumentObserver> obs(do_QueryInterface(shell));
              obs->ContentInserted(doc, parent, content, index);
            }
          }
        }

        // Flush again
        // XXXbz why is this needed?
        doc->FlushPendingNotifications(Flush_ContentAndNotify);
      }
    }
  }

  // Clear out the whole array.
  mBoundElements = nsnull;

  // Delete ourselves.
  NS_RELEASE(mResources->mLoader);
}

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
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
#include "nsIHTMLContentContainer.h"
#include "nsNetUtil.h"
#include "nsXBLAtoms.h"
#include "nsINameSpaceManager.h"
#include "nsIFrameManager.h"
#include "nsIStyleContext.h"

NS_IMPL_ISUPPORTS1(nsXBLResourceLoader, nsICSSLoaderObserver)

nsXBLResourceLoader::nsXBLResourceLoader(nsIXBLPrototypeBinding* aBinding, nsXBLPrototypeResources* aResources)
:mBinding(aBinding),
 mResources(aResources),
 mResourceList(nsnull),
 mLastResource(nsnull),
 mLoadingResources(PR_FALSE),
 mInLoadResourcesFunc(PR_FALSE),
 mPendingSheets(0)
{
  NS_INIT_ISUPPORTS();
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

  nsCOMPtr<nsIXBLDocumentInfo> info;
  mBinding->GetXBLDocumentInfo(nsnull, getter_AddRefs(info));
  if (!info) {
    mInLoadResourcesFunc = PR_FALSE;
    return;
  }

  nsCOMPtr<nsIDocument> doc;
  info->GetDocument(getter_AddRefs(doc));

  nsCOMPtr<nsIURI> docURL;
  doc->GetDocumentURL(getter_AddRefs(docURL));

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
        nsCOMPtr<nsIHTMLContentContainer> htmlContent(do_QueryInterface(doc));
        htmlContent->GetCSSLoader(*getter_AddRefs(cssLoader));
      }

      if (!cssLoader)
        continue;

      // Kick off the load of the stylesheet.
      PRBool doneLoading;
      NS_NAMED_LITERAL_STRING(empty, "");

      nsresult rv = cssLoader->LoadStyleLink(nsnull, url, empty, empty, kNameSpaceID_Unknown,
                               nsnull,
                               doneLoading, this);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Load failed!!!");

      if (!doneLoading)
        mPendingSheets++;
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
  if (!mResources->mStyleSheetList) {
    NS_NewISupportsArray(getter_AddRefs(mResources->mStyleSheetList));
    if (!mResources->mStyleSheetList)
      return NS_ERROR_OUT_OF_MEMORY;
  }

  mResources->mStyleSheetList->AppendElement(aSheet);

  if (!mInLoadResourcesFunc)
    mPendingSheets--;
  
  if (mPendingSheets == 0) {
    // All stylesheets are loaded.  
    nsCOMPtr<nsIStyleRuleProcessor> prevProcessor;
    NS_NewISupportsArray(getter_AddRefs(mResources->mRuleProcessors));
    PRUint32 count;
    mResources->mStyleSheetList->Count(&count);
    for (PRUint32 i = 0; i < count; i++) {
      nsCOMPtr<nsISupports> supp = getter_AddRefs(mResources->mStyleSheetList->ElementAt(i));
      nsCOMPtr<nsICSSStyleSheet> sheet(do_QueryInterface(supp));

      nsCOMPtr<nsIStyleRuleProcessor> processor;
      sheet->GetStyleRuleProcessor(*getter_AddRefs(processor), prevProcessor);
      if (processor != prevProcessor) {
        mResources->mRuleProcessors->AppendElement(processor);
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
  nsCAutoString bindingURI;
  mBinding->GetBindingURI(bindingURI);

  PRUint32 eltCount;
  mBoundElements->Count(&eltCount);
  for (PRUint32 j = 0; j < eltCount; j++) {
    nsCOMPtr<nsISupports> supp = getter_AddRefs(mBoundElements->ElementAt(j));
    nsCOMPtr<nsIContent> content(do_QueryInterface(supp));
    
    PRBool ready = PR_FALSE;
    xblService->BindingReady(content, bindingURI, &ready);

    if (ready) {
      nsCOMPtr<nsIDocument> doc;
      content->GetDocument(*getter_AddRefs(doc));
    
      if (doc) {
        // Flush first
        doc->FlushPendingNotifications();

        // Notify
        nsCOMPtr<nsIContent> parent;
        content->GetParent(*getter_AddRefs(parent));
        PRInt32 index = 0;
        if (parent)
          parent->IndexOf(content, index);
        
        // If |content| is (in addition to having binding |mBinding|)
        // also a descendant of another element with binding |mBinding|,
        // then we might have just constructed it due to the
        // notification of its parent.  (We can know about both if the
        // binding loads were triggered from the DOM rather than frame
        // construction.)  So we have to check both whether the element
        // has a primary frame and whether it's in the undisplayed map
        // before sending a ContentInserted notification, or bad things
        // will happen.
        nsCOMPtr<nsIPresShell> shell;
        doc->GetShellAt(0, getter_AddRefs(shell));
        if (shell) {
          nsIFrame* childFrame;
          shell->GetPrimaryFrameFor(content, &childFrame);
          if (!childFrame) {
            // Check to see if it's in the undisplayed content map.
            nsCOMPtr<nsIFrameManager> frameManager;
            shell->GetFrameManager(getter_AddRefs(frameManager));
            nsCOMPtr<nsIStyleContext> sc;
            frameManager->GetUndisplayedContent(content, getter_AddRefs(sc));
            if (!sc) {
              nsCOMPtr<nsIDocumentObserver> obs(do_QueryInterface(shell));
              obs->ContentInserted(doc, parent, content, index);
            }
          }
        }

        // Flush again
        doc->FlushPendingNotifications();
      }
    }
  }

  // Clear out the whole array.
  mBoundElements = nsnull;

  // Delete ourselves.
  NS_RELEASE(mResources->mLoader);
}

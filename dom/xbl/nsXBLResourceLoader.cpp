/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCSSFrameConstructor.h"
#include "nsTArray.h"
#include "nsString.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIPresShell.h"
#include "nsXBLService.h"
#include "nsIServiceManager.h"
#include "nsXBLResourceLoader.h"
#include "nsXBLPrototypeResources.h"
#include "nsIDocumentObserver.h"
#include "imgILoader.h"
#include "imgRequestProxy.h"
#include "mozilla/ComputedStyle.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"
#include "mozilla/css/Loader.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsGkAtoms.h"
#include "nsXBLPrototypeBinding.h"
#include "nsContentUtils.h"
#include "nsIScriptSecurityManager.h"

using namespace mozilla;

NS_IMPL_CYCLE_COLLECTION(nsXBLResourceLoader, mBoundElements)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsXBLResourceLoader)
  NS_INTERFACE_MAP_ENTRY(nsICSSLoaderObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsXBLResourceLoader)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsXBLResourceLoader)

struct nsXBLResource
{
  nsXBLResource* mNext;
  nsAtom* mType;
  nsString mSrc;

  nsXBLResource(nsAtom* aType, const nsAString& aSrc)
  {
    MOZ_COUNT_CTOR(nsXBLResource);
    mNext = nullptr;
    mType = aType;
    mSrc = aSrc;
  }

  ~nsXBLResource()
  {
    MOZ_COUNT_DTOR(nsXBLResource);
    NS_CONTENT_DELETE_LIST_MEMBER(nsXBLResource, this, mNext);
  }
};

nsXBLResourceLoader::nsXBLResourceLoader(nsXBLPrototypeBinding* aBinding,
                                         nsXBLPrototypeResources* aResources)
:mBinding(aBinding),
 mResources(aResources),
 mResourceList(nullptr),
 mLastResource(nullptr),
 mLoadingResources(false),
 mInLoadResourcesFunc(false),
 mPendingSheets(0)
{
}

nsXBLResourceLoader::~nsXBLResourceLoader()
{
  delete mResourceList;
}

bool
nsXBLResourceLoader::LoadResources(nsIContent* aBoundElement)
{
  mInLoadResourcesFunc = true;

  if (mLoadingResources) {
    mInLoadResourcesFunc = false;
    return mPendingSheets == 0;
  }

  mLoadingResources = true;

  // Declare our loaders.
  nsCOMPtr<nsIDocument> doc = mBinding->XBLDocumentInfo()->GetDocument();
  mBoundDocument = aBoundElement->OwnerDoc();

  mozilla::css::Loader* cssLoader = doc->CSSLoader();
  MOZ_ASSERT(cssLoader->GetDocument(), "Loader must have document");

  nsIURI *docURL = doc->GetDocumentURI();
  nsIPrincipal* docPrincipal = doc->NodePrincipal();

  nsCOMPtr<nsIURI> url;

  for (nsXBLResource* curr = mResourceList; curr; curr = curr->mNext) {
    if (curr->mSrc.IsEmpty())
      continue;

    if (NS_FAILED(NS_NewURI(getter_AddRefs(url), curr->mSrc,
                            doc->GetDocumentCharacterSet(), docURL)))
      continue;

    if (curr->mType == nsGkAtoms::image) {
      // Now kick off the image load...
      // Passing nullptr for pretty much everything -- cause we don't care!
      // XXX: initialDocumentURI is nullptr!
      RefPtr<imgRequestProxy> req;
      nsContentUtils::LoadImage(url, doc, doc, docPrincipal, 0, docURL,
                                doc->GetReferrerPolicy(), nullptr,
                                nsIRequest::LOAD_BACKGROUND, EmptyString(),
                                getter_AddRefs(req));
    }
    else if (curr->mType == nsGkAtoms::stylesheet) {
      // Kick off the load of the stylesheet.

      // Always load chrome synchronously
      // XXXbz should that still do a content policy check?
      bool chrome;
      nsresult rv;
      if (NS_SUCCEEDED(url->SchemeIs("chrome", &chrome)) && chrome)
      {
        rv = nsContentUtils::GetSecurityManager()->
          CheckLoadURIWithPrincipal(docPrincipal, url,
                                    nsIScriptSecurityManager::ALLOW_CHROME);
        if (NS_SUCCEEDED(rv)) {
          RefPtr<StyleSheet> sheet;
          rv = cssLoader->LoadSheetSync(url, &sheet);
          NS_ASSERTION(NS_SUCCEEDED(rv), "Load failed!!!");
          if (NS_SUCCEEDED(rv))
          {
            rv = StyleSheetLoaded(sheet, false, NS_OK);
            NS_ASSERTION(NS_SUCCEEDED(rv), "Processing the style sheet failed!!!");
          }
        }
      }
      else
      {
        rv = cssLoader->LoadSheet(url, false, docPrincipal, nullptr, this);
        if (NS_SUCCEEDED(rv))
          ++mPendingSheets;
      }
    }
  }

  mInLoadResourcesFunc = false;

  // Destroy our resource list.
  delete mResourceList;
  mResourceList = nullptr;

  return mPendingSheets == 0;
}

// nsICSSLoaderObserver
NS_IMETHODIMP
nsXBLResourceLoader::StyleSheetLoaded(StyleSheet* aSheet,
                                      bool aWasAlternate,
                                      nsresult aStatus)
{
  if (!mResources) {
    // Our resources got destroyed -- just bail out
    return NS_OK;
  }

  mResources->AppendStyleSheet(aSheet);

  if (!mInLoadResourcesFunc)
    mPendingSheets--;

  if (mPendingSheets == 0) {
    // All stylesheets are loaded.
    mResources->ComputeServoStyles(
      *mBoundDocument->GetShell()->StyleSet());

    // XXX Check for mPendingScripts when scripts also come online.
    if (!mInLoadResourcesFunc)
      NotifyBoundElements();
  }
  return NS_OK;
}

void
nsXBLResourceLoader::AddResource(nsAtom* aResourceType, const nsAString& aSrc)
{
  nsXBLResource* res = new nsXBLResource(aResourceType, aSrc);
  if (!mResourceList)
    mResourceList = res;
  else
    mLastResource->mNext = res;

  mLastResource = res;
}

void
nsXBLResourceLoader::AddResourceListener(nsIContent* aBoundElement)
{
  if (aBoundElement) {
    mBoundElements.AppendObject(aBoundElement);
    aBoundElement->OwnerDoc()->BlockOnload();
  }
}

void
nsXBLResourceLoader::NotifyBoundElements()
{
  nsXBLService* xblService = nsXBLService::GetInstance();
  if (!xblService)
    return;

  nsIURI* bindingURI = mBinding->BindingURI();

  uint32_t eltCount = mBoundElements.Count();
  for (uint32_t j = 0; j < eltCount; j++) {
    nsCOMPtr<nsIContent> content = mBoundElements.ObjectAt(j);
    MOZ_ASSERT(content->IsElement());
    content->OwnerDoc()->UnblockOnload(/* aFireSync = */ false);

    bool ready = false;
    xblService->BindingReady(content, bindingURI, &ready);

    if (!ready) {
      continue;
    }

    nsIDocument* doc = content->GetUncomposedDoc();
    if (!doc) {
      continue;
    }

    nsIPresShell* shell = doc->GetShell();
    if (!shell) {
      continue;
    }

    shell->PostRecreateFramesFor(content->AsElement());
  }

  // Clear out the whole array.
  mBoundElements.Clear();

  // Delete ourselves.
  mResources->ClearLoader();
}

nsresult
nsXBLResourceLoader::Write(nsIObjectOutputStream* aStream)
{
  nsresult rv;

  for (nsXBLResource* curr = mResourceList; curr; curr = curr->mNext) {
    if (curr->mType == nsGkAtoms::image)
      rv = aStream->Write8(XBLBinding_Serialize_Image);
    else if (curr->mType == nsGkAtoms::stylesheet)
      rv = aStream->Write8(XBLBinding_Serialize_Stylesheet);
    else
      continue;

    NS_ENSURE_SUCCESS(rv, rv);

    rv = aStream->WriteWStringZ(curr->mSrc.get());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

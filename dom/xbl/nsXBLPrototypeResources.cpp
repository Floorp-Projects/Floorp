/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsIStyleRuleProcessor.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIServiceManager.h"
#include "nsXBLResourceLoader.h"
#include "nsXBLPrototypeResources.h"
#include "nsXBLPrototypeBinding.h"
#include "nsIDocumentObserver.h"
#include "mozilla/css/Loader.h"
#include "nsIURI.h"
#include "nsLayoutCID.h"
#include "nsCSSRuleProcessor.h"
#include "nsStyleSet.h"
#include "mozilla/dom/URL.h"
#include "mozilla/DebugOnly.h"

using namespace mozilla;
using mozilla::dom::IsChromeURI;

nsXBLPrototypeResources::nsXBLPrototypeResources(nsXBLPrototypeBinding* aBinding)
{
  MOZ_COUNT_CTOR(nsXBLPrototypeResources);

  mLoader = new nsXBLResourceLoader(aBinding, this);
}

nsXBLPrototypeResources::~nsXBLPrototypeResources()
{
  MOZ_COUNT_DTOR(nsXBLPrototypeResources);
  if (mLoader) {
    mLoader->mResources = nullptr;
  }
}

void
nsXBLPrototypeResources::AddResource(nsIAtom* aResourceType, const nsAString& aSrc)
{
  if (mLoader)
    mLoader->AddResource(aResourceType, aSrc);
}

void
nsXBLPrototypeResources::LoadResources(bool* aResult)
{
  if (mLoader)
    mLoader->LoadResources(aResult);
  else
    *aResult = true; // All resources loaded.
}

void
nsXBLPrototypeResources::AddResourceListener(nsIContent* aBoundElement)
{
  if (mLoader)
    mLoader->AddResourceListener(aBoundElement);
}

nsresult
nsXBLPrototypeResources::FlushSkinSheets()
{
  if (mStyleSheetList.Length() == 0)
    return NS_OK;

  nsCOMPtr<nsIDocument> doc =
    mLoader->mBinding->XBLDocumentInfo()->GetDocument();

  // If doc is null, we're in the process of tearing things down, so just
  // return without rebuilding anything.
  if (!doc) {
    return NS_OK;
  }

  // We have scoped stylesheets.  Reload any chrome stylesheets we
  // encounter.  (If they aren't skin sheets, it doesn't matter, since
  // they'll still be in the chrome cache.
  mRuleProcessor = nullptr;

  nsTArray<nsRefPtr<CSSStyleSheet>> oldSheets;

  oldSheets.SwapElements(mStyleSheetList);

  mozilla::css::Loader* cssLoader = doc->CSSLoader();

  for (size_t i = 0, count = oldSheets.Length(); i < count; ++i) {
    CSSStyleSheet* oldSheet = oldSheets[i];

    nsIURI* uri = oldSheet->GetSheetURI();

    nsRefPtr<CSSStyleSheet> newSheet;
    if (IsChromeURI(uri)) {
      if (NS_FAILED(cssLoader->LoadSheetSync(uri, getter_AddRefs(newSheet))))
        continue;
    }
    else {
      newSheet = oldSheet;
    }

    mStyleSheetList.AppendElement(newSheet);
  }

  GatherRuleProcessor();

  return NS_OK;
}

nsresult
nsXBLPrototypeResources::Write(nsIObjectOutputStream* aStream)
{
  if (mLoader)
    return mLoader->Write(aStream);
  return NS_OK;
}

void
nsXBLPrototypeResources::Traverse(nsCycleCollectionTraversalCallback &cb)
{
  NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "proto mResources mLoader");
  cb.NoteXPCOMChild(mLoader);

  CycleCollectionNoteChild(cb, mRuleProcessor.get(), "mRuleProcessor");
  ImplCycleCollectionTraverse(cb, mStyleSheetList, "mStyleSheetList");
}

void
nsXBLPrototypeResources::Unlink()
{
  mStyleSheetList.Clear();
  mRuleProcessor = nullptr;
}

void
nsXBLPrototypeResources::ClearLoader()
{
  mLoader = nullptr;
}

void
nsXBLPrototypeResources::GatherRuleProcessor()
{
  mRuleProcessor = new nsCSSRuleProcessor(mStyleSheetList,
                                          nsStyleSet::eDocSheet,
                                          nullptr);
}

void
nsXBLPrototypeResources::AppendStyleSheet(CSSStyleSheet* aSheet)
{
  mStyleSheetList.AppendElement(aSheet);
}

void
nsXBLPrototypeResources::RemoveStyleSheet(CSSStyleSheet* aSheet)
{
  mStyleSheetList.RemoveElement(aSheet);
}

void
nsXBLPrototypeResources::InsertStyleSheetAt(size_t aIndex, CSSStyleSheet* aSheet)
{
  mStyleSheetList.InsertElementAt(aIndex, aSheet);
}

CSSStyleSheet*
nsXBLPrototypeResources::StyleSheetAt(size_t aIndex) const
{
  return mStyleSheetList[aIndex];
}

size_t
nsXBLPrototypeResources::SheetCount() const
{
  return mStyleSheetList.Length();
}

bool
nsXBLPrototypeResources::HasStyleSheets() const
{
  return !mStyleSheetList.IsEmpty();
}

void
nsXBLPrototypeResources::AppendStyleSheetsTo(
                                      nsTArray<CSSStyleSheet*>& aResult) const
{
  aResult.AppendElements(mStyleSheetList);
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "mozilla/StyleSheetHandle.h"
#include "mozilla/StyleSheetHandleInlines.h"

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
  // they'll still be in the chrome cache.  Skip inline sheets, which
  // skin sheets can't be, and which in any case don't have a usable
  // URL to reload.)

  nsTArray<StyleSheetHandle::RefPtr> oldSheets;

  oldSheets.SwapElements(mStyleSheetList);

  mozilla::css::Loader* cssLoader = doc->CSSLoader();

  for (size_t i = 0, count = oldSheets.Length(); i < count; ++i) {
    StyleSheetHandle oldSheet = oldSheets[i];

    nsIURI* uri = oldSheet->GetSheetURI();

    StyleSheetHandle::RefPtr newSheet;
    if (!oldSheet->IsInline() && IsChromeURI(uri)) {
      if (NS_FAILED(cssLoader->LoadSheetSync(uri, &newSheet)))
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
  nsTArray<RefPtr<CSSStyleSheet>> sheets(mStyleSheetList.Length());
  for (StyleSheetHandle sheet : mStyleSheetList) {
    MOZ_ASSERT(sheet->IsGecko(),
               "GatherRuleProcessor must only be called for "
               "nsXBLPrototypeResources objects with Gecko-flavored style "
               "backends");
    sheets.AppendElement(sheet->AsGecko());
  }
  mRuleProcessor = new nsCSSRuleProcessor(Move(sheets),
                                          SheetType::Doc,
                                          nullptr,
                                          mRuleProcessor);
}

void
nsXBLPrototypeResources::AppendStyleSheet(StyleSheetHandle aSheet)
{
  mStyleSheetList.AppendElement(aSheet);
}

void
nsXBLPrototypeResources::RemoveStyleSheet(StyleSheetHandle aSheet)
{
  mStyleSheetList.RemoveElement(aSheet);
}

void
nsXBLPrototypeResources::InsertStyleSheetAt(size_t aIndex, StyleSheetHandle aSheet)
{
  mStyleSheetList.InsertElementAt(aIndex, aSheet);
}

StyleSheetHandle
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
                                      nsTArray<StyleSheetHandle>& aResult) const
{
  aResult.AppendElements(mStyleSheetList);
}

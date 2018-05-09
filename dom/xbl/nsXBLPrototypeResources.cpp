/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include "mozilla/dom/URL.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/ServoStyleRuleMap.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/StyleSheetInlines.h"

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
nsXBLPrototypeResources::AddResource(nsAtom* aResourceType, const nsAString& aSrc)
{
  if (mLoader)
    mLoader->AddResource(aResourceType, aSrc);
}

bool
nsXBLPrototypeResources::LoadResources(nsIContent* aBoundElement)
{
  if (mLoader) {
    return mLoader->LoadResources(aBoundElement);
  }

  return true; // All resources loaded.
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

  nsTArray<RefPtr<StyleSheet>> oldSheets;

  oldSheets.SwapElements(mStyleSheetList);

  mozilla::css::Loader* cssLoader = doc->CSSLoader();

  for (size_t i = 0, count = oldSheets.Length(); i < count; ++i) {
    StyleSheet* oldSheet = oldSheets[i];

    nsIURI* uri = oldSheet->GetSheetURI();

    RefPtr<StyleSheet> newSheet;
    if (!oldSheet->IsInline() && IsChromeURI(uri)) {
      if (NS_FAILED(cssLoader->LoadSheetSync(uri, &newSheet)))
        continue;
    }
    else {
      newSheet = oldSheet;
    }

    mStyleSheetList.AppendElement(newSheet);
  }

  // There may be no shell during unlink.
  if (auto* shell = doc->GetShell()) {
    MOZ_ASSERT(shell->GetPresContext());
    ComputeServoStyles(*shell->StyleSet());
  }

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

  ImplCycleCollectionTraverse(cb, mStyleSheetList, "mStyleSheetList");
}

void
nsXBLPrototypeResources::Unlink()
{
  mStyleSheetList.Clear();
}

void
nsXBLPrototypeResources::ClearLoader()
{
  mLoader = nullptr;
}


void
nsXBLPrototypeResources::SyncServoStyles()
{
  mStyleRuleMap.reset(nullptr);
  mServoStyles.reset(Servo_AuthorStyles_Create());
  for (auto& sheet : mStyleSheetList) {
    Servo_AuthorStyles_AppendStyleSheet(mServoStyles.get(), sheet);
  }
}

void
nsXBLPrototypeResources::ComputeServoStyles(const ServoStyleSet& aMasterStyleSet)
{
  SyncServoStyles();
  Servo_AuthorStyles_Flush(mServoStyles.get(), aMasterStyleSet.RawSet());
}

ServoStyleRuleMap*
nsXBLPrototypeResources::GetServoStyleRuleMap()
{
  if (!HasStyleSheets() || !mServoStyles) {
    return nullptr;
  }

  if (!mStyleRuleMap) {
    mStyleRuleMap = MakeUnique<ServoStyleRuleMap>();
  }

  mStyleRuleMap->EnsureTable(*this);
  return mStyleRuleMap.get();
}

void
nsXBLPrototypeResources::AppendStyleSheet(StyleSheet* aSheet)
{
  mStyleSheetList.AppendElement(aSheet);
}

void
nsXBLPrototypeResources::RemoveStyleSheet(StyleSheet* aSheet)
{
  mStyleSheetList.RemoveElement(aSheet);
}

void
nsXBLPrototypeResources::InsertStyleSheetAt(size_t aIndex, StyleSheet* aSheet)
{
  mStyleSheetList.InsertElementAt(aIndex, aSheet);
}

void
nsXBLPrototypeResources::AppendStyleSheetsTo(
                                      nsTArray<StyleSheet*>& aResult) const
{
  aResult.AppendElements(mStyleSheetList);
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoAuthorStylesMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoAuthorStylesMallocEnclosingSizeOf)

size_t
nsXBLPrototypeResources::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += mStyleSheetList.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (const auto& sheet : mStyleSheetList) {
    n += sheet->SizeOfIncludingThis(aMallocSizeOf);
  }
  n += mServoStyles ? Servo_AuthorStyles_SizeOfIncludingThis(
      ServoAuthorStylesMallocSizeOf,
      ServoAuthorStylesMallocEnclosingSizeOf,
      mServoStyles.get()) : 0;
  n += mStyleRuleMap ? mStyleRuleMap->SizeOfIncludingThis(aMallocSizeOf) : 0;

  // Measurement of the following members may be added later if DMD finds it
  // is worthwhile:
  // - mLoader

  return n;
}

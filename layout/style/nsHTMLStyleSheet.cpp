/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * style sheet and style rule processor representing data from presentational
 * HTML attributes
 */

#include "nsHTMLStyleSheet.h"
#include "nsMappedAttributes.h"
#include "nsGkAtoms.h"
#include "nsPresContext.h"
#include "mozilla/EventStates.h"
#include "mozilla/PresShell.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/DocumentInlines.h"
#include "nsStyleConsts.h"
#include "nsError.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/dom/Element.h"
#include "nsHashKeys.h"
#include "mozilla/OperatorNewExtensions.h"
#include "mozilla/RestyleManager.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoStyleSet.h"

using namespace mozilla;
using namespace mozilla::dom;

// -----------------------------------------------------------

struct MappedAttrTableEntry : public PLDHashEntryHdr {
  nsMappedAttributes* mAttributes;
};

static PLDHashNumber MappedAttrTable_HashKey(const void* key) {
  nsMappedAttributes* attributes =
      static_cast<nsMappedAttributes*>(const_cast<void*>(key));

  return attributes->HashValue();
}

static void MappedAttrTable_ClearEntry(PLDHashTable* table,
                                       PLDHashEntryHdr* hdr) {
  MappedAttrTableEntry* entry = static_cast<MappedAttrTableEntry*>(hdr);

  entry->mAttributes->DropStyleSheetReference();
  memset(entry, 0, sizeof(MappedAttrTableEntry));
}

static bool MappedAttrTable_MatchEntry(const PLDHashEntryHdr* hdr,
                                       const void* key) {
  nsMappedAttributes* attributes =
      static_cast<nsMappedAttributes*>(const_cast<void*>(key));
  const MappedAttrTableEntry* entry =
      static_cast<const MappedAttrTableEntry*>(hdr);

  return attributes->Equals(entry->mAttributes);
}

static const PLDHashTableOps MappedAttrTable_Ops = {
    MappedAttrTable_HashKey, MappedAttrTable_MatchEntry,
    PLDHashTable::MoveEntryStub, MappedAttrTable_ClearEntry, nullptr};

// -----------------------------------------------------------

// -----------------------------------------------------------

nsHTMLStyleSheet::nsHTMLStyleSheet(Document* aDocument)
    : mDocument(aDocument),
      mMappedAttrTable(&MappedAttrTable_Ops, sizeof(MappedAttrTableEntry)),
      mMappedAttrsDirty(false) {
  MOZ_ASSERT(aDocument);
}

void nsHTMLStyleSheet::SetOwningDocument(Document* aDocument) {
  mDocument = aDocument;  // not refcounted
}

void nsHTMLStyleSheet::Reset() {
  mServoUnvisitedLinkDecl = nullptr;
  mServoVisitedLinkDecl = nullptr;
  mServoActiveLinkDecl = nullptr;

  mMappedAttrTable.Clear();
  mMappedAttrsDirty = false;
}

nsresult nsHTMLStyleSheet::ImplLinkColorSetter(
    RefPtr<RawServoDeclarationBlock>& aDecl, nscolor aColor) {
  if (!mDocument || !mDocument->GetPresShell()) {
    return NS_OK;
  }

  MOZ_ASSERT(!ServoStyleSet::IsInServoTraversal());
  aDecl = Servo_DeclarationBlock_CreateEmpty().Consume();
  Servo_DeclarationBlock_SetColorValue(aDecl.get(), eCSSProperty_color, aColor);

  // Now make sure we restyle any links that might need it.  This
  // shouldn't happen often, so just rebuilding everything is ok.
  if (Element* root = mDocument->GetRootElement()) {
    RestyleManager* rm = mDocument->GetPresContext()->RestyleManager();
    rm->PostRestyleEvent(root, RestyleHint::RestyleSubtree(), nsChangeHint(0));
  }
  return NS_OK;
}

nsresult nsHTMLStyleSheet::SetLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoUnvisitedLinkDecl, aColor);
}

nsresult nsHTMLStyleSheet::SetActiveLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoActiveLinkDecl, aColor);
}

nsresult nsHTMLStyleSheet::SetVisitedLinkColor(nscolor aColor) {
  return ImplLinkColorSetter(mServoVisitedLinkDecl, aColor);
}

already_AddRefed<nsMappedAttributes> nsHTMLStyleSheet::UniqueMappedAttributes(
    nsMappedAttributes* aMapped) {
  mMappedAttrsDirty = true;
  auto entry = static_cast<MappedAttrTableEntry*>(
      mMappedAttrTable.Add(aMapped, fallible));
  if (!entry) return nullptr;
  if (!entry->mAttributes) {
    // We added a new entry to the hashtable, so we have a new unique set.
    entry->mAttributes = aMapped;
  }
  RefPtr<nsMappedAttributes> ret = entry->mAttributes;
  return ret.forget();
}

void nsHTMLStyleSheet::DropMappedAttributes(nsMappedAttributes* aMapped) {
  NS_ENSURE_TRUE_VOID(aMapped);
#ifdef DEBUG
  uint32_t entryCount = mMappedAttrTable.EntryCount() - 1;
#endif

  mMappedAttrTable.Remove(aMapped);

  NS_ASSERTION(entryCount == mMappedAttrTable.EntryCount(), "not removed");
}

void nsHTMLStyleSheet::CalculateMappedServoDeclarations() {
  for (auto iter = mMappedAttrTable.Iter(); !iter.Done(); iter.Next()) {
    MappedAttrTableEntry* attr = static_cast<MappedAttrTableEntry*>(iter.Get());
    if (attr->mAttributes->GetServoStyle()) {
      // Only handle cases which haven't been filled in already
      continue;
    }
    attr->mAttributes->LazilyResolveServoDeclaration(mDocument);
  }
}

size_t nsHTMLStyleSheet::DOMSizeOfIncludingThis(
    MallocSizeOf aMallocSizeOf) const {
  size_t n = aMallocSizeOf(this);

  n += mMappedAttrTable.ShallowSizeOfExcludingThis(aMallocSizeOf);
  for (auto iter = mMappedAttrTable.ConstIter(); !iter.Done(); iter.Next()) {
    auto entry = static_cast<MappedAttrTableEntry*>(iter.Get());
    n += entry->mAttributes->SizeOfIncludingThis(aMallocSizeOf);
  }

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURL
  // - mLinkRule
  // - mVisitedRule
  // - mActiveRule
  // - mTableQuirkColorRule
  // - mTableTHRule
  // - mLangRuleTable
  //
  // The following members are not measured:
  // - mDocument, because it's non-owning

  return n;
}

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"

#include "mozilla/css/Rule.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoMediaList.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/MediaList.h"
#include "nsIStyleSheetLinkingElement.h"
#include "Loader.h"


#include "mozAutoDocUpdate.h"
#include "nsIDOMCSSStyleSheet.h"

using namespace mozilla::dom;

namespace mozilla {

// -------------------------------
// CSS Style Sheet Inner Data Container
//

ServoStyleSheetInner::ServoStyleSheetInner(CORSMode aCORSMode,
                                           ReferrerPolicy aReferrerPolicy,
                                           const SRIMetadata& aIntegrity)
  : StyleSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
  MOZ_COUNT_CTOR(ServoStyleSheetInner);
}

ServoStyleSheetInner::ServoStyleSheetInner(ServoStyleSheetInner& aCopy,
                                           ServoStyleSheet* aPrimarySheet)
  : StyleSheetInfo(aCopy, aPrimarySheet)
{
  MOZ_COUNT_CTOR(ServoStyleSheetInner);

  // Actually clone aCopy's mSheet and use that as our mSheet.
  mSheet = Servo_StyleSheet_Clone(aCopy.mSheet).Consume();

  mURLData = aCopy.mURLData;
}

ServoStyleSheetInner::~ServoStyleSheetInner()
{
  MOZ_COUNT_DTOR(ServoStyleSheetInner);
}

StyleSheetInfo*
ServoStyleSheetInner::CloneFor(StyleSheet* aPrimarySheet)
{
  return new ServoStyleSheetInner(*this,
                                  static_cast<ServoStyleSheet*>(aPrimarySheet));
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSheetMallocSizeOf)

size_t
ServoStyleSheetInner::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += Servo_StyleSheet_SizeOfIncludingThis(ServoStyleSheetMallocSizeOf, mSheet);
  return n;
}

ServoStyleSheet::ServoStyleSheet(css::SheetParsingMode aParsingMode,
                                 CORSMode aCORSMode,
                                 net::ReferrerPolicy aReferrerPolicy,
                                 const dom::SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Servo, aParsingMode)
{
  mInner = new ServoStyleSheetInner(aCORSMode, aReferrerPolicy, aIntegrity);
  mInner->AddSheet(this);
}

ServoStyleSheet::ServoStyleSheet(const ServoStyleSheet& aCopy,
                                 ServoStyleSheet* aParentToUse,
                                 dom::CSSImportRule* aOwnerRuleToUse,
                                 nsIDocument* aDocumentToUse,
                                 nsINode* aOwningNodeToUse)
  : StyleSheet(aCopy,
               aParentToUse,
               aOwnerRuleToUse,
               aDocumentToUse,
               aOwningNodeToUse)
{
  if (mDirty) { // CSSOM's been there, force full copy now
    NS_ASSERTION(mInner->mComplete,
                 "Why have rules been accessed on an incomplete sheet?");
    // FIXME: handle failure?
    //
    // NOTE: It's important to call this from the subclass, since this could
    // access uninitialized members otherwise.
    EnsureUniqueInner();
  }
}

ServoStyleSheet::~ServoStyleSheet()
{
  UnparentChildren();

  DropRuleList();
}

// QueryInterface implementation for ServoStyleSheet
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(ServoStyleSheet)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMCSSStyleSheet)
  if (aIID.Equals(NS_GET_IID(ServoStyleSheet)))
    foundInterface = reinterpret_cast<nsISupports*>(this);
  else
NS_INTERFACE_MAP_END_INHERITING(StyleSheet)

NS_IMPL_ADDREF_INHERITED(ServoStyleSheet, StyleSheet)
NS_IMPL_RELEASE_INHERITED(ServoStyleSheet, StyleSheet)

NS_IMPL_CYCLE_COLLECTION_CLASS(ServoStyleSheet)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ServoStyleSheet)
  tmp->DropRuleList();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END_INHERITED(StyleSheet)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(ServoStyleSheet, StyleSheet)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleList)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

bool
ServoStyleSheet::HasRules() const
{
  return Inner()->mSheet && Servo_StyleSheet_HasRules(Inner()->mSheet);
}

nsresult
ServoStyleSheet::ParseSheet(css::Loader* aLoader,
                            const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber,
                            nsCompatibility aCompatMode,
                            css::LoaderReusableStyleSheets* aReusableSheets)
{
  MOZ_ASSERT_IF(mMedia, mMedia->IsServo());
  RefPtr<URLExtraData> extraData =
    new URLExtraData(aBaseURI, aSheetURI, aSheetPrincipal);

  NS_ConvertUTF16toUTF8 input(aInput);
  if (!Inner()->mSheet) {
    auto* mediaList = static_cast<ServoMediaList*>(mMedia.get());
    RawServoMediaList* media = mediaList ?  &mediaList->RawList() : nullptr;

    Inner()->mSheet =
      Servo_StyleSheet_FromUTF8Bytes(
          aLoader, this, &input, mParsingMode, media, extraData,
          aLineNumber, aCompatMode
      ).Consume();
  } else {
    // TODO(emilio): Once we have proper inner cloning (which we don't right
    // now) we should update the mediaList here too, though it's slightly
    // tricky.
    Servo_StyleSheet_ClearAndUpdate(Inner()->mSheet, aLoader,
                                    this, &input, extraData, aLineNumber,
                                    aReusableSheets);
  }

  Inner()->mURLData = extraData.forget();
  return NS_OK;
}

void
ServoStyleSheet::LoadFailed()
{
  Inner()->mSheet = Servo_StyleSheet_Empty(mParsingMode).Consume();
  Inner()->mURLData = URLExtraData::Dummy();
}

nsresult
ServoStyleSheet::ReparseSheet(const nsAString& aInput)
{
  // TODO(kuoe0): Bug 1367996 - Need to call document notification
  // (StyleRuleAdded() and StyleRuleRemoved()) like what we do in
  // CSSStyleSheet::ReparseSheet().

  if (!mInner->mComplete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  } else {
    loader = new css::Loader(StyleBackendType::Servo, nullptr);
  }

  // cache child sheets to reuse
  css::LoaderReusableStyleSheets reusableSheets;
  for (StyleSheet* child = GetFirstChild(); child; child = child->mNext) {
    if (child->GetOriginalURI()) {
      reusableSheets.AddReusableSheet(child);
    }
  }

  // clean up child sheets list
  for (StyleSheet* child = GetFirstChild(); child; ) {
    StyleSheet* next = child->mNext;
    child->mParent = nullptr;
    child->SetAssociatedDocument(nullptr, NotOwnedByDocument);
    child->mNext = nullptr;
    child = next;
  }
  Inner()->mFirstChild = nullptr;

  uint32_t lineNumber = 1;
  if (mOwningNode) {
    nsCOMPtr<nsIStyleSheetLinkingElement> link = do_QueryInterface(mOwningNode);
    if (link) {
      lineNumber = link->GetLineNumber();
    }
  }

  nsresult rv = ParseSheet(loader, aInput, mInner->mSheetURI, mInner->mBaseURI,
                           mInner->mPrincipal, lineNumber,
                           eCompatibility_FullStandards, &reusableSheets);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
ServoStyleSheet::StyleSheetLoaded(StyleSheet* aSheet,
                                  bool aWasAlternate,
                                  nsresult aStatus)
{
  MOZ_ASSERT(aSheet->IsServo(),
             "why we were called back with a CSSStyleSheet?");

  ServoStyleSheet* sheet = aSheet->AsServo();
  if (sheet->GetParentSheet() == nullptr) {
    return NS_OK; // ignore if sheet has been detached already
  }
  NS_ASSERTION(this == sheet->GetParentSheet(),
               "We are being notified of a sheet load for a sheet that is not our child!");

  if (mDocument && NS_SUCCEEDED(aStatus)) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
    NS_WARNING("stylo: Import rule object not implemented");
    mDocument->StyleRuleAdded(this, nullptr);
  }

  return NS_OK;
}

void
ServoStyleSheet::DropRuleList()
{
  if (mRuleList) {
    mRuleList->DropReference();
    mRuleList = nullptr;
  }
}

already_AddRefed<StyleSheet>
ServoStyleSheet::Clone(StyleSheet* aCloneParent,
                       dom::CSSImportRule* aCloneOwnerRule,
                       nsIDocument* aCloneDocument,
                       nsINode* aCloneOwningNode) const
{
  RefPtr<StyleSheet> clone = new ServoStyleSheet(*this,
    static_cast<ServoStyleSheet*>(aCloneParent),
    aCloneOwnerRule,
    aCloneDocument,
    aCloneOwningNode);
  return clone.forget();
}

CSSRuleList*
ServoStyleSheet::GetCssRulesInternal(ErrorResult& aRv)
{
  if (!mRuleList) {
    EnsureUniqueInner();

    RefPtr<ServoCssRules> rawRules =
      Servo_StyleSheet_GetRules(Inner()->mSheet).Consume();
    MOZ_ASSERT(rawRules);
    mRuleList = new ServoCSSRuleList(rawRules.forget(), this);
  }
  return mRuleList;
}

uint32_t
ServoStyleSheet::InsertRuleInternal(const nsAString& aRule,
                                    uint32_t aIndex, ErrorResult& aRv)
{
  // Ensure mRuleList is constructed.
  GetCssRulesInternal(aRv);

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  aRv = mRuleList->InsertRule(aRule, aIndex);
  if (aRv.Failed()) {
    return 0;
  }
  if (mDocument) {
    if (mRuleList->GetRuleType(aIndex) != css::Rule::IMPORT_RULE ||
        !RuleHasPendingChildSheet(mRuleList->GetRule(aIndex))) {
      // XXX We may not want to get the rule when stylesheet change event
      // is not enabled.
      mDocument->StyleRuleAdded(this, mRuleList->GetRule(aIndex));
    }
  }
  return aIndex;
}

void
ServoStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
  // Ensure mRuleList is constructed.
  GetCssRulesInternal(aRv);
  if (aIndex > mRuleList->Length()) {
    aRv.Throw(NS_ERROR_DOM_INDEX_SIZE_ERR);
    return;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  // Hold a strong ref to the rule so it doesn't die when we remove it
  // from the list. XXX We may not want to hold it if stylesheet change
  // event is not enabled.
  RefPtr<css::Rule> rule = mRuleList->GetRule(aIndex);
  aRv = mRuleList->DeleteRule(aIndex);
  MOZ_ASSERT(!aRv.ErrorCodeIs(NS_ERROR_DOM_INDEX_SIZE_ERR),
             "IndexSizeError should have been handled earlier");
  if (!aRv.Failed() && mDocument) {
    mDocument->StyleRuleRemoved(this, rule);
  }
}

nsresult
ServoStyleSheet::InsertRuleIntoGroupInternal(const nsAString& aRule,
                                             css::GroupRule* aGroup,
                                             uint32_t aIndex)
{
  auto rules = static_cast<ServoCSSRuleList*>(aGroup->CssRules());
  MOZ_ASSERT(rules->GetParentRule() == aGroup);
  return rules->InsertRule(aRule, aIndex);
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  size_t n = StyleSheet::SizeOfIncludingThis(aMallocSizeOf);
  const ServoStyleSheet* s = this;
  while (s) {
    // See the comment in CSSStyleSheet::SizeOfIncludingThis() for an
    // explanation of this.
    if (s->Inner()->mSheets.LastElement() == s) {
      n += s->Inner()->SizeOfIncludingThis(aMallocSizeOf);
    }

    // Measurement of the following members may be added later if DMD finds it
    // is worthwhile:
    // - s->mRuleList

    s = s->mNext ? s->mNext->AsServo() : nullptr;
  }
  return n;
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"

#include "mozilla/css/Rule.h"
#include "mozilla/StyleBackendType.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/dom/CSSRuleList.h"

#include "mozAutoDocUpdate.h"

using namespace mozilla::dom;

namespace mozilla {

ServoStyleSheet::ServoStyleSheet(css::SheetParsingMode aParsingMode,
                                 CORSMode aCORSMode,
                                 net::ReferrerPolicy aReferrerPolicy,
                                 const dom::SRIMetadata& aIntegrity)
  : StyleSheet(StyleBackendType::Servo, aParsingMode)
  , mSheetInfo(aCORSMode, aReferrerPolicy, aIntegrity)
{
}

ServoStyleSheet::~ServoStyleSheet()
{
  DropSheet();
  if (mRuleList) {
    mRuleList->DropReference();
  }
}

bool
ServoStyleSheet::HasRules() const
{
  return mSheet && Servo_StyleSheet_HasRules(mSheet);
}

void
ServoStyleSheet::SetOwningDocument(nsIDocument* aDocument)
{
  // XXXheycam: Traverse to child ServoStyleSheets to set this, like
  // CSSStyleSheet::SetOwningDocument does.

  mDocument = aDocument;
}

ServoStyleSheet*
ServoStyleSheet::GetParentSheet() const
{
  // XXXheycam: When we implement support for child sheets, we'll have
  // to fix SetOwningDocument to propagate the owning document down
  // to the children.
  MOZ_CRASH("stylo: not implemented");
}

void
ServoStyleSheet::AppendStyleSheet(ServoStyleSheet* aSheet)
{
  aSheet->mDocument = mDocument;
}

nsresult
ServoStyleSheet::ParseSheet(css::Loader* aLoader,
                            const nsAString& aInput,
                            nsIURI* aSheetURI,
                            nsIURI* aBaseURI,
                            nsIPrincipal* aSheetPrincipal,
                            uint32_t aLineNumber)
{
  RefPtr<ThreadSafeURIHolder> base = new ThreadSafeURIHolder(aBaseURI);
  RefPtr<ThreadSafeURIHolder> referrer = new ThreadSafeURIHolder(aSheetURI);
  RefPtr<ThreadSafePrincipalHolder> principal =
    new ThreadSafePrincipalHolder(aSheetPrincipal);

  nsCString baseString;
  nsresult rv = aBaseURI->GetSpec(baseString);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ConvertUTF16toUTF8 input(aInput);
  if (!mSheet) {
    mSheet =
      Servo_StyleSheet_FromUTF8Bytes(aLoader, this, &input, mParsingMode,
                                     &baseString, base, referrer,
                                     principal).Consume();
  } else {
    Servo_StyleSheet_ClearAndUpdate(mSheet, aLoader, this, &input, base,
                                    referrer, principal);
  }

  return NS_OK;
}

void
ServoStyleSheet::LoadFailed()
{
  mSheet = Servo_StyleSheet_Empty(mParsingMode).Consume();
}

void
ServoStyleSheet::DropSheet()
{
  mSheet = nullptr;
}

size_t
ServoStyleSheet::SizeOfIncludingThis(MallocSizeOf aMallocSizeOf) const
{
  MOZ_CRASH("stylo: not implemented");
}

#ifdef DEBUG
void
ServoStyleSheet::List(FILE* aOut, int32_t aIndex) const
{
  MOZ_CRASH("stylo: not implemented");
}
#endif

nsMediaList*
ServoStyleSheet::Media()
{
  return nullptr;
}

nsIDOMCSSRule*
ServoStyleSheet::GetDOMOwnerRule() const
{
  return nullptr;
}

CSSRuleList*
ServoStyleSheet::GetCssRulesInternal(ErrorResult& aRv)
{
  if (!mRuleList) {
    RefPtr<ServoCssRules> rawRules = Servo_StyleSheet_GetRules(mSheet).Consume();
    mRuleList = new ServoCSSRuleList(this, rawRules.forget());
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
    // XXX When we support @import rules, we should not notify here,
    // but rather when the sheet the rule is importing is loaded.
    // XXX We may not want to get the rule when stylesheet change event
    // is not enabled.
    mDocument->StyleRuleAdded(this, mRuleList->GetRule(aIndex));
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

} // namespace mozilla

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ServoStyleSheet.h"

#include "mozilla/css/Rule.h"
#include "mozilla/ServoBindings.h"
#include "mozilla/ServoCSSRuleList.h"
#include "mozilla/ServoImportRule.h"
#include "mozilla/StaticPrefs.h"
#include "mozilla/css/GroupRule.h"
#include "mozilla/dom/CSSRuleList.h"
#include "mozilla/dom/MediaList.h"
#include "nsIStyleSheetLinkingElement.h"
#include "ErrorReporter.h"
#include "Loader.h"


#include "mozAutoDocUpdate.h"

using namespace mozilla::dom;

namespace mozilla {

// -------------------------------
// CSS Style Sheet Inner Data Container
//

void
ServoStyleSheet::BuildChildListAfterInnerClone()
{
  MOZ_ASSERT(Inner()->mSheets.Length() == 1, "Should've just cloned");
  MOZ_ASSERT(Inner()->mSheets[0] == this);
  MOZ_ASSERT(!Inner()->mFirstChild);

  auto* contents = Inner()->mContents.get();
  RefPtr<ServoCssRules> rules =
    Servo_StyleSheet_GetRules(contents).Consume();

  uint32_t index = 0;
  while (true) {
    uint32_t line, column; // Actually unused.
    RefPtr<RawServoImportRule> import =
      Servo_CssRules_GetImportRuleAt(rules, index, &line, &column).Consume();
    if (!import) {
      // Note that only @charset rules come before @import rules, and @charset
      // rules are parsed but skipped, so we can stop iterating as soon as we
      // find something that isn't an @import rule.
      break;
    }
    auto* sheet =
      const_cast<ServoStyleSheet*>(Servo_ImportRule_GetSheet(import));
    MOZ_ASSERT(sheet);
    PrependStyleSheetSilently(sheet);
    index++;
  }
}

already_AddRefed<ServoStyleSheet>
ServoStyleSheet::CreateEmptyChildSheet(
    already_AddRefed<dom::MediaList> aMediaList) const
{
  RefPtr<ServoStyleSheet> child =
    new ServoStyleSheet(
        ParsingMode(),
        CORSMode::CORS_NONE,
        GetReferrerPolicy(),
        SRIMetadata());

  child->mMedia = aMediaList;
  return child.forget();
}

MOZ_DEFINE_MALLOC_SIZE_OF(ServoStyleSheetMallocSizeOf)
MOZ_DEFINE_MALLOC_ENCLOSING_SIZE_OF(ServoStyleSheetMallocEnclosingSizeOf)

// We disable parallel stylesheet parsing if any of the following three
// conditions hold:
//
// (1) The pref is off.
// (2) The browser is recording CSS errors (which parallel parsing can't handle).
// (3) The stylesheet is a chrome stylesheet, since those can use -moz-bool-pref,
//     which needs to access the pref service, which is not threadsafe.
static bool
AllowParallelParse(css::Loader* aLoader, nsIURI* aSheetURI)
{
  // Check the pref.
  if (!StaticPrefs::layout_css_parsing_parallel()) {
    return false;
  }

  // If the browser is recording CSS errors, we need to use the sequential path
  // because the parallel path doesn't support that.
  nsIDocument* doc = aLoader->GetDocument();
  if (doc && css::ErrorReporter::ShouldReportErrors(*doc)) {
    return false;
  }

  // If this is a chrome stylesheet, it might use -moz-bool-pref, which needs to
  // access the pref service, which is not thread-safe. We could probably expose
  // the relevant booleans as thread-safe var caches if we needed to, but
  // parsing chrome stylesheets in parallel is unlikely to be a win anyway.
  //
  // Note that UA stylesheets can also use -moz-bool-pref, but those are always
  // parsed sync.
  if (dom::IsChromeURI(aSheetURI)) {
    return false;
  }

  return true;
}

RefPtr<StyleSheetParsePromise>
ServoStyleSheet::ParseSheet(css::Loader* aLoader,
                            const nsACString& aBytes,
                            css::SheetLoadData* aLoadData)
{
  MOZ_ASSERT(aLoader);
  MOZ_ASSERT(aLoadData);
  MOZ_ASSERT(mParsePromise.IsEmpty());
  RefPtr<StyleSheetParsePromise> p = mParsePromise.Ensure(__func__);
  Inner()->mURLData =
    new URLExtraData(GetBaseURI(), GetSheetURI(), Principal()); // RefPtr

  if (!AllowParallelParse(aLoader, GetSheetURI())) {
    RefPtr<RawServoStyleSheetContents> contents =
      Servo_StyleSheet_FromUTF8Bytes(aLoader,
                                     this,
                                     aLoadData,
                                     &aBytes,
                                     mParsingMode,
                                     Inner()->mURLData,
                                     aLoadData->mLineNumber,
                                     aLoader->GetCompatibilityMode(),
                                     /* reusable_sheets = */ nullptr)
      .Consume();
    FinishAsyncParse(contents.forget());
  } else {
    RefPtr<css::SheetLoadDataHolder> loadDataHolder =
      new css::SheetLoadDataHolder(__func__, aLoadData);
    Servo_StyleSheet_FromUTF8BytesAsync(loadDataHolder,
                                        Inner()->mURLData,
                                        &aBytes,
                                        mParsingMode,
                                        aLoadData->mLineNumber,
                                        aLoader->GetCompatibilityMode());
  }

  return Move(p);
}

void
ServoStyleSheet::FinishAsyncParse(already_AddRefed<RawServoStyleSheetContents> aSheetContents)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(!mParsePromise.IsEmpty());
  Inner()->mContents = aSheetContents;
  FinishParse();
  mParsePromise.Resolve(true, __func__);
}


void
ServoStyleSheet::ParseSheetSync(css::Loader* aLoader,
                                const nsACString& aBytes,
                                css::SheetLoadData* aLoadData,
                                uint32_t aLineNumber,
                                css::LoaderReusableStyleSheets* aReusableSheets)
{
  nsCompatibility compatMode =
    aLoader ? aLoader->GetCompatibilityMode() : eCompatibility_FullStandards;

  Inner()->mURLData = new URLExtraData(GetBaseURI(), GetSheetURI(), Principal()); // RefPtr
  Inner()->mContents = Servo_StyleSheet_FromUTF8Bytes(aLoader,
                                                      this,
                                                      aLoadData,
                                                      &aBytes,
                                                      mParsingMode,
                                                      Inner()->mURLData,
                                                      aLineNumber,
                                                      compatMode,
                                                      aReusableSheets)
                         .Consume();

  FinishParse();
}

void
ServoStyleSheet::FinishParse()
{
  nsString sourceMapURL;
  Servo_StyleSheet_GetSourceMapURL(Inner()->mContents, &sourceMapURL);
  SetSourceMapURLFromComment(sourceMapURL);

  nsString sourceURL;
  Servo_StyleSheet_GetSourceURL(Inner()->mContents, &sourceURL);
  SetSourceURL(sourceURL);
}

nsresult
ServoStyleSheet::ReparseSheet(const nsAString& aInput)
{
  if (!mInner->mComplete) {
    return NS_ERROR_DOM_INVALID_ACCESS_ERR;
  }

  // Hold strong ref to the CSSLoader in case the document update
  // kills the document
  RefPtr<css::Loader> loader;
  if (mDocument) {
    loader = mDocument->CSSLoader();
    NS_ASSERTION(loader, "Document with no CSS loader!");
  } else {
    loader = new css::Loader;
  }

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);

  WillDirty();

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

  // Notify to the stylesets about the old rules going away.
  {
    ServoCSSRuleList* ruleList = GetCssRulesInternal();
    MOZ_ASSERT(ruleList);

    uint32_t ruleCount = ruleList->Length();
    for (uint32_t i = 0; i < ruleCount; ++i) {
      css::Rule* rule = ruleList->GetRule(i);
      MOZ_ASSERT(rule);
      if (rule->Type() == CSSRuleBinding::IMPORT_RULE &&
          RuleHasPendingChildSheet(rule)) {
        continue; // notify when loaded (see StyleSheetLoaded)
      }
      RuleRemoved(*rule);
    }
  }

  DropRuleList();

  ParseSheetSync(loader,
                 NS_ConvertUTF16toUTF8(aInput),
                 /* aLoadData = */ nullptr,
                 lineNumber,
                 &reusableSheets);

  // Notify the stylesets about the new rules.
  {
    // Get the rule list (which will need to be regenerated after ParseSheet).
    ServoCSSRuleList* ruleList = GetCssRulesInternal();
    MOZ_ASSERT(ruleList);

    uint32_t ruleCount = ruleList->Length();
    for (uint32_t i = 0; i < ruleCount; ++i) {
      css::Rule* rule = ruleList->GetRule(i);
      MOZ_ASSERT(rule);
      if (rule->Type() == CSSRuleBinding::IMPORT_RULE &&
          RuleHasPendingChildSheet(rule)) {
        continue; // notify when loaded (see StyleSheetLoaded)
      }

      RuleAdded(*rule);
    }
  }

  // Our rules are no longer considered modified.
  ClearModifiedRules();

  return NS_OK;
}

// nsICSSLoaderObserver implementation
NS_IMETHODIMP
ServoStyleSheet::StyleSheetLoaded(StyleSheet* aSheet,
                                  bool aWasAlternate,
                                  nsresult aStatus)
{
  if (!aSheet->GetParentSheet()) {
    return NS_OK; // ignore if sheet has been detached already
  }
  NS_ASSERTION(this == aSheet->GetParentSheet(),
               "We are being notified of a sheet load for a sheet that is not our child!");

  if (NS_SUCCEEDED(aStatus)) {
    mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
    RuleAdded(*aSheet->GetOwnerRule());
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

ServoCSSRuleList*
ServoStyleSheet::GetCssRulesInternal()
{
  if (!mRuleList) {
    EnsureUniqueInner();

    RefPtr<ServoCssRules> rawRules =
      Servo_StyleSheet_GetRules(Inner()->mContents).Consume();
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
  GetCssRulesInternal();

  mozAutoDocUpdate updateBatch(mDocument, UPDATE_STYLE, true);
  aRv = mRuleList->InsertRule(aRule, aIndex);
  if (aRv.Failed()) {
    return 0;
  }

  // XXX We may not want to get the rule when stylesheet change event
  // is not enabled.
  css::Rule* rule = mRuleList->GetRule(aIndex);
  if (rule->Type() != CSSRuleBinding::IMPORT_RULE ||
      !RuleHasPendingChildSheet(rule)) {
    RuleAdded(*rule);
  }

  return aIndex;
}

void
ServoStyleSheet::DeleteRuleInternal(uint32_t aIndex, ErrorResult& aRv)
{
  // Ensure mRuleList is constructed.
  GetCssRulesInternal();
  if (aIndex >= mRuleList->Length()) {
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
  if (!aRv.Failed()) {
    RuleRemoved(*rule);
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

OriginFlags
ServoStyleSheet::GetOrigin()
{
  return static_cast<OriginFlags>(
    Servo_StyleSheet_GetOrigin(Inner()->mContents));
}

} // namespace mozilla

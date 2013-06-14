/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* rules in a CSS stylesheet other than style rules (e.g., @import rules) */

#include "mozilla/Attributes.h"

#include "nsCSSRules.h"
#include "nsCSSValue.h"
#include "mozilla/css/ImportRule.h"
#include "mozilla/css/NameSpaceRule.h"

#include "nsString.h"
#include "nsIAtom.h"

#include "nsCSSProps.h"
#include "nsCSSStyleSheet.h"

#include "nsCOMPtr.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIMediaList.h"
#include "nsICSSRuleList.h"
#include "nsIDocument.h"
#include "nsPresContext.h"

#include "nsContentUtils.h"
#include "nsError.h"
#include "nsStyleUtil.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSParser.h"
#include "nsPrintfCString.h"
#include "nsDOMClassInfoID.h"
#include "mozilla/dom/CSSStyleDeclarationBinding.h"
#include "StyleRule.h"
#include "nsFont.h"

using namespace mozilla;

#define IMPL_STYLE_RULE_INHERIT_GET_DOM_RULE_WEAK(class_, super_) \
  /* virtual */ nsIDOMCSSRule* class_::GetDOMRule()               \
  { return this; }                                                \
  /* virtual */ nsIDOMCSSRule* class_::GetExistingDOMRule()       \
  { return this; }
#define IMPL_STYLE_RULE_INHERIT_MAP_RULE_INFO_INTO(class_, super_) \
/* virtual */ void class_::MapRuleInfoInto(nsRuleData* aRuleData) \
  { NS_ABORT_IF_FALSE(false, "should not be called"); }

#define IMPL_STYLE_RULE_INHERIT(class_, super_) \
IMPL_STYLE_RULE_INHERIT_GET_DOM_RULE_WEAK(class_, super_) \
IMPL_STYLE_RULE_INHERIT_MAP_RULE_INFO_INTO(class_, super_)

// base class for all rule types in a CSS style sheet

namespace mozilla {
namespace css {

nsCSSStyleSheet*
Rule::GetStyleSheet() const
{
  if (!(mSheet & 0x1)) {
    return reinterpret_cast<nsCSSStyleSheet*>(mSheet);
  }

  return nullptr;
}

nsHTMLCSSStyleSheet*
Rule::GetHTMLCSSStyleSheet() const
{
  if (mSheet & 0x1) {
    return reinterpret_cast<nsHTMLCSSStyleSheet*>(mSheet & ~uintptr_t(0x1));
  }

  return nullptr;
}

/* virtual */ void
Rule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = reinterpret_cast<uintptr_t>(aSheet);
}

void
Rule::SetHTMLCSSStyleSheet(nsHTMLCSSStyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = reinterpret_cast<uintptr_t>(aSheet);
  mSheet |= 0x1;
}

nsresult
Rule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    NS_IF_ADDREF(*aParentRule = mParentRule->GetDOMRule());
  } else {
    *aParentRule = nullptr;
  }
  return NS_OK;
}

nsresult
Rule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);

  NS_IF_ADDREF(*aSheet = GetStyleSheet());
  return NS_OK;
}

size_t
Rule::SizeOfCOMArrayElementIncludingThis(css::Rule* aElement,
                                         nsMallocSizeOfFun aMallocSizeOf,
                                         void* aData)
{
  return aElement->SizeOfIncludingThis(aMallocSizeOf);
}

// -------------------------------
// Style Rule List for group rules
//

class GroupRuleRuleList MOZ_FINAL : public nsICSSRuleList
{
public:
  GroupRuleRuleList(GroupRule *aGroupRule);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMCSSRULELIST

  virtual nsIDOMCSSRule* GetItemAt(uint32_t aIndex, nsresult* aResult);

  void DropReference() { mGroupRule = nullptr; }

private:
  ~GroupRuleRuleList();

private:
  GroupRule* mGroupRule;
};

GroupRuleRuleList::GroupRuleRuleList(GroupRule *aGroupRule)
{
  // Not reference counted to avoid circular references.
  // The rule will tell us when its going away.
  mGroupRule = aGroupRule;
}

GroupRuleRuleList::~GroupRuleRuleList()
{
}

// QueryInterface implementation for CSSGroupRuleRuleList
NS_INTERFACE_MAP_BEGIN(GroupRuleRuleList)
  NS_INTERFACE_MAP_ENTRY(nsICSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSGroupRuleRuleList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(GroupRuleRuleList)
NS_IMPL_RELEASE(GroupRuleRuleList)

NS_IMETHODIMP
GroupRuleRuleList::GetLength(uint32_t* aLength)
{
  if (mGroupRule) {
    *aLength = (uint32_t)mGroupRule->StyleRuleCount();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

nsIDOMCSSRule*
GroupRuleRuleList::GetItemAt(uint32_t aIndex, nsresult* aResult)
{
  *aResult = NS_OK;

  if (mGroupRule) {
    nsRefPtr<Rule> rule = mGroupRule->GetStyleRuleAt(aIndex);
    if (rule) {
      return rule->GetDOMRule();
    }
  }

  return nullptr;
}

NS_IMETHODIMP
GroupRuleRuleList::Item(uint32_t aIndex, nsIDOMCSSRule** aReturn)
{
  nsresult rv;
  nsIDOMCSSRule* rule = GetItemAt(aIndex, &rv);
  if (!rule) {
    *aReturn = nullptr;
    return rv;
  }

  NS_ADDREF(*aReturn = rule);
  return NS_OK;
}

} // namespace css
} // namespace mozilla

// Must be outside the namespace
DOMCI_DATA(CSSGroupRuleRuleList, css::GroupRuleRuleList)

// -------------------------------------------
// CharsetRule
//

// Must be outside namespace
DOMCI_DATA(CSSCharsetRule, css::CharsetRule)

namespace mozilla {
namespace css {

CharsetRule::CharsetRule(const nsAString& aEncoding)
  : Rule(),
    mEncoding(aEncoding)
{
}

CharsetRule::CharsetRule(const CharsetRule& aCopy)
  : Rule(aCopy),
    mEncoding(aCopy.mEncoding)
{
}

NS_IMPL_ADDREF(CharsetRule)
NS_IMPL_RELEASE(CharsetRule)

// QueryInterface implementation for CharsetRule
NS_INTERFACE_MAP_BEGIN(CharsetRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSCharsetRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSCharsetRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CharsetRule, Rule)

#ifdef DEBUG
/* virtual */ void
CharsetRule::List(FILE* out, int32_t aIndent) const
{
  // Indent
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@charset \"", out);
  fputs(NS_LossyConvertUTF16toASCII(mEncoding).get(), out);
  fputs("\"\n", out);
}
#endif

/* virtual */ int32_t
CharsetRule::GetType() const
{
  return Rule::CHARSET_RULE;
}

/* virtual */ already_AddRefed<Rule>
CharsetRule::Clone() const
{
  nsRefPtr<Rule> clone = new CharsetRule(*this);
  return clone.forget();
}

NS_IMETHODIMP
CharsetRule::GetEncoding(nsAString& aEncoding)
{
  aEncoding = mEncoding;
  return NS_OK;
}

NS_IMETHODIMP
CharsetRule::SetEncoding(const nsAString& aEncoding)
{
  mEncoding = aEncoding;
  return NS_OK;
}

NS_IMETHODIMP
CharsetRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::CHARSET_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CharsetRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@charset \"");
  aCssText.Append(mEncoding);
  aCssText.AppendLiteral("\";");
  return NS_OK;
}

NS_IMETHODIMP
CharsetRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CharsetRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
CharsetRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

/* virtual */ size_t
CharsetRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mEncoding
}

// -------------------------------------------
// ImportRule
//

ImportRule::ImportRule(nsMediaList* aMedia, const nsString& aURLSpec)
  : Rule()
  , mURLSpec(aURLSpec)
  , mMedia(aMedia)
{
  // XXXbz This is really silly.... the mMedia here will be replaced
  // with itself if we manage to load a sheet.  Which should really
  // never fail nowadays, in sane cases.
}

ImportRule::ImportRule(const ImportRule& aCopy)
  : Rule(aCopy),
    mURLSpec(aCopy.mURLSpec)
{
  // Whether or not an @import rule has a null sheet is a permanent
  // property of that @import rule, since it is null only if the target
  // sheet failed security checks.
  if (aCopy.mChildSheet) {
    nsRefPtr<nsCSSStyleSheet> sheet =
      aCopy.mChildSheet->Clone(nullptr, this, nullptr, nullptr);
    SetSheet(sheet);
    // SetSheet sets mMedia appropriately
  }
}

ImportRule::~ImportRule()
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nullptr);
  }
}

NS_IMPL_ADDREF(ImportRule)
NS_IMPL_RELEASE(ImportRule)

// QueryInterface implementation for ImportRule
NS_INTERFACE_MAP_BEGIN(ImportRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSImportRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSImportRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(ImportRule, Rule)

#ifdef DEBUG
/* virtual */ void
ImportRule::List(FILE* out, int32_t aIndent) const
{
  // Indent
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@import \"", out);
  fputs(NS_LossyConvertUTF16toASCII(mURLSpec).get(), out);
  fputs("\" ", out);

  nsAutoString mediaText;
  mMedia->GetText(mediaText);
  fputs(NS_LossyConvertUTF16toASCII(mediaText).get(), out);
  fputs("\n", out);
}
#endif

/* virtual */ int32_t
ImportRule::GetType() const
{
  return Rule::IMPORT_RULE;
}

/* virtual */ already_AddRefed<Rule>
ImportRule::Clone() const
{
  nsRefPtr<Rule> clone = new ImportRule(*this);
  return clone.forget();
}

void
ImportRule::SetSheet(nsCSSStyleSheet* aSheet)
{
  NS_PRECONDITION(aSheet, "null arg");

  // set the new sheet
  mChildSheet = aSheet;
  aSheet->SetOwnerRule(this);

  // set our medialist to be the same as the sheet's medialist
  nsCOMPtr<nsIDOMMediaList> mediaList;
  mChildSheet->GetMedia(getter_AddRefs(mediaList));
  NS_ABORT_IF_FALSE(mediaList, "GetMedia returned null");
  mMedia = static_cast<nsMediaList*>(mediaList.get());
}

NS_IMETHODIMP
ImportRule::GetType(uint16_t* aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = nsIDOMCSSRule::IMPORT_RULE;
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@import url(");
  nsStyleUtil::AppendEscapedCSSString(mURLSpec, aCssText);
  aCssText.Append(NS_LITERAL_STRING(")"));
  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    if (!mediaText.IsEmpty()) {
      aCssText.AppendLiteral(" ");
      aCssText.Append(mediaText);
    }
  }
  aCssText.AppendLiteral(";");
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ImportRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
ImportRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
ImportRule::GetHref(nsAString & aHref)
{
  aHref = mURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::GetMedia(nsIDOMMediaList * *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);

  NS_IF_ADDREF(*aMedia = mMedia);
  return NS_OK;
}

NS_IMETHODIMP
ImportRule::GetStyleSheet(nsIDOMCSSStyleSheet * *aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);

  NS_IF_ADDREF(*aStyleSheet = mChildSheet);
  return NS_OK;
}

/* virtual */ size_t
ImportRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURLSpec
  //
  // The following members are not measured:
  // - mMedia, because it is measured via nsCSSStyleSheet::mMedia
  // - mChildSheet, because it is measured via nsCSSStyleSheetInner::mSheets
}

} // namespace css
} // namespace mozilla

// must be outside the namespace
DOMCI_DATA(CSSImportRule, css::ImportRule)

static bool
CloneRuleInto(css::Rule* aRule, void* aArray)
{
  nsRefPtr<css::Rule> clone = aRule->Clone();
  static_cast<nsCOMArray<css::Rule>*>(aArray)->AppendObject(clone);
  return true;
}

namespace mozilla {
namespace css {

GroupRule::GroupRule()
  : Rule()
{
}

static bool
SetParentRuleReference(Rule* aRule, void* aParentRule)
{
  GroupRule* parentRule = static_cast<GroupRule*>(aParentRule);
  aRule->SetParentRule(parentRule);
  return true;
}

GroupRule::GroupRule(const GroupRule& aCopy)
  : Rule(aCopy)
{
  const_cast<GroupRule&>(aCopy).mRules.EnumerateForwards(CloneRuleInto, &mRules);
  mRules.EnumerateForwards(SetParentRuleReference, this);
}

GroupRule::~GroupRule()
{
  NS_ABORT_IF_FALSE(!mSheet, "SetStyleSheet should have been called");
  mRules.EnumerateForwards(SetParentRuleReference, nullptr);
  if (mRuleCollection) {
    mRuleCollection->DropReference();
  }
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(GroupRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(GroupRule)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GroupRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT_MAP_RULE_INFO_INTO(GroupRule, Rule)

static bool
SetStyleSheetReference(Rule* aRule, void* aSheet)
{
  nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;
  aRule->SetStyleSheet(sheet);
  return true;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(GroupRule)
  tmp->mRules.EnumerateForwards(SetParentRuleReference, nullptr);
  // If tmp does not have a stylesheet, neither do its descendants.  In that
  // case, don't try to null out their stylesheet, to avoid O(N^2) behavior in
  // depth of group rule nesting.  But if tmp _does_ have a stylesheet (which
  // can happen if it gets unlinked earlier than its owning stylesheet), then we
  // need to null out the stylesheet pointer on descendants now, before we clear
  // tmp->mRules.
  if (tmp->GetStyleSheet()) {
    tmp->mRules.EnumerateForwards(SetStyleSheetReference, nullptr);
  }
  tmp->mRules.Clear();
  if (tmp->mRuleCollection) {
    tmp->mRuleCollection->DropReference();
    tmp->mRuleCollection = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(GroupRule)
  const nsCOMArray<Rule>& rules = tmp->mRules;
  for (int32_t i = 0, count = rules.Count(); i < count; ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(cb, "mRules[i]");
    cb.NoteXPCOMChild(rules[i]->GetExistingDOMRule());
  }
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mRuleCollection)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

/* virtual */ void
GroupRule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  // Don't set the sheet on the kids if it's already the same as the sheet we
  // already have.  This is needed to avoid O(N^2) behavior in group nesting
  // depth when seting the sheet to null during unlink, if we happen to unlin in
  // order from most nested rule up to least nested rule.
  if (aSheet != GetStyleSheet()) {
    mRules.EnumerateForwards(SetStyleSheetReference, aSheet);
    Rule::SetStyleSheet(aSheet);
  }
}

#ifdef DEBUG
/* virtual */ void
GroupRule::List(FILE* out, int32_t aIndent) const
{
  fputs(" {\n", out);

  for (int32_t index = 0, count = mRules.Count(); index < count; ++index) {
    mRules.ObjectAt(index)->List(out, aIndent + 1);
  }

  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);
  fputs("}\n", out);
}
#endif

void
GroupRule::AppendStyleRule(Rule* aRule)
{
  mRules.AppendObject(aRule);
  nsCSSStyleSheet* sheet = GetStyleSheet();
  aRule->SetStyleSheet(sheet);
  aRule->SetParentRule(this);
  if (sheet) {
    sheet->SetModifiedByChildRule();
  }
}

Rule*
GroupRule::GetStyleRuleAt(int32_t aIndex) const
{
  return mRules.SafeObjectAt(aIndex);
}

bool
GroupRule::EnumerateRulesForwards(RuleEnumFunc aFunc, void * aData) const
{
  return
    const_cast<GroupRule*>(this)->mRules.EnumerateForwards(aFunc, aData);
}

/*
 * The next two methods (DeleteStyleRuleAt and InsertStyleRuleAt)
 * should never be called unless you have first called WillDirty() on
 * the parents stylesheet.  After they are called, DidDirty() needs to
 * be called on the sheet
 */
nsresult
GroupRule::DeleteStyleRuleAt(uint32_t aIndex)
{
  Rule* rule = mRules.SafeObjectAt(aIndex);
  if (rule) {
    rule->SetStyleSheet(nullptr);
    rule->SetParentRule(nullptr);
  }
  return mRules.RemoveObjectAt(aIndex) ? NS_OK : NS_ERROR_ILLEGAL_VALUE;
}

nsresult
GroupRule::InsertStyleRuleAt(uint32_t aIndex, Rule* aRule)
{
  aRule->SetStyleSheet(GetStyleSheet());
  aRule->SetParentRule(this);
  if (! mRules.InsertObjectAt(aRule, aIndex)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
GroupRule::ReplaceStyleRule(Rule* aOld, Rule* aNew)
{
  int32_t index = mRules.IndexOf(aOld);
  NS_ENSURE_TRUE(index != -1, NS_ERROR_UNEXPECTED);
  mRules.ReplaceObjectAt(aNew, index);
  aNew->SetStyleSheet(GetStyleSheet());
  aNew->SetParentRule(this);
  aOld->SetStyleSheet(nullptr);
  aOld->SetParentRule(nullptr);
  return NS_OK;
}

nsresult
GroupRule::AppendRulesToCssText(nsAString& aCssText)
{
  aCssText.AppendLiteral(" {\n");

  // get all the rules
  for (int32_t index = 0, count = mRules.Count(); index < count; ++index) {
    Rule* rule = mRules.ObjectAt(index);
    nsIDOMCSSRule* domRule = rule->GetDOMRule();
    if (domRule) {
      nsAutoString cssText;
      domRule->GetCssText(cssText);
      aCssText.Append(NS_LITERAL_STRING("  ") +
                      cssText +
                      NS_LITERAL_STRING("\n"));
    }
  }

  aCssText.AppendLiteral("}");

  return NS_OK;
}

// nsIDOMCSSMediaRule or nsIDOMCSSMozDocumentRule methods
nsresult
GroupRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  if (!mRuleCollection) {
    mRuleCollection = new css::GroupRuleRuleList(this);
  }

  NS_ADDREF(*aRuleList = mRuleCollection);
  return NS_OK;
}

nsresult
GroupRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  nsCSSStyleSheet* sheet = GetStyleSheet();
  NS_ENSURE_TRUE(sheet, NS_ERROR_FAILURE);
  
  if (aIndex > uint32_t(mRules.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ASSERTION(uint32_t(mRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  return sheet->InsertRuleIntoGroup(aRule, this, aIndex, _retval);
}

nsresult
GroupRule::DeleteRule(uint32_t aIndex)
{
  nsCSSStyleSheet* sheet = GetStyleSheet();
  NS_ENSURE_TRUE(sheet, NS_ERROR_FAILURE);

  if (aIndex >= uint32_t(mRules.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ASSERTION(uint32_t(mRules.Count()) <= INT32_MAX,
               "Too many style rules!");

  return sheet->DeleteRuleFromGroup(this, aIndex);
}

/* virtual */ size_t
GroupRule::SizeOfExcludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return mRules.SizeOfExcludingThis(Rule::SizeOfCOMArrayElementIncludingThis,
                                    aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mRuleCollection
}


// -------------------------------------------
// nsICSSMediaRule
//
MediaRule::MediaRule()
{
}

MediaRule::MediaRule(const MediaRule& aCopy)
  : GroupRule(aCopy)
{
  if (aCopy.mMedia) {
    aCopy.mMedia->Clone(getter_AddRefs(mMedia));
    if (mMedia) {
      // XXXldb This doesn't really make sense.
      mMedia->SetStyleSheet(aCopy.GetStyleSheet());
    }
  }
}

MediaRule::~MediaRule()
{
  if (mMedia) {
    mMedia->SetStyleSheet(nullptr);
  }
}

NS_IMPL_ADDREF_INHERITED(MediaRule, GroupRule)
NS_IMPL_RELEASE_INHERITED(MediaRule, GroupRule)

// QueryInterface implementation for MediaRule
NS_INTERFACE_MAP_BEGIN(MediaRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMediaRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSMediaRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

/* virtual */ void
MediaRule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  if (mMedia) {
    // Set to null so it knows it's leaving one sheet and joining another.
    mMedia->SetStyleSheet(nullptr);
    mMedia->SetStyleSheet(aSheet);
  }

  GroupRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
/* virtual */ void
MediaRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@media ", out);

  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    fputs(NS_LossyConvertUTF16toASCII(mediaText).get(), out);
  }

  GroupRule::List(out, aIndent);
}
#endif

/* virtual */ int32_t
MediaRule::GetType() const
{
  return Rule::MEDIA_RULE;
}

/* virtual */ already_AddRefed<Rule>
MediaRule::Clone() const
{
  nsRefPtr<Rule> clone = new MediaRule(*this);
  return clone.forget();
}

nsresult
MediaRule::SetMedia(nsMediaList* aMedia)
{
  mMedia = aMedia;
  if (aMedia)
    mMedia->SetStyleSheet(GetStyleSheet());
  return NS_OK;
}

// nsIDOMCSSRule methods
NS_IMETHODIMP
MediaRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
MediaRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@media ");
  AppendConditionText(aCssText);
  return GroupRule::AppendRulesToCssText(aCssText);
}

NS_IMETHODIMP
MediaRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
MediaRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return GroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
MediaRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return GroupRule::GetParentRule(aParentRule);
}

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
MediaRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
MediaRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
MediaRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
MediaRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Truncate(0);
  AppendConditionText(aConditionText);
  return NS_OK;
}

NS_IMETHODIMP
MediaRule::SetConditionText(const nsAString& aConditionText)
{
  if (!mMedia) {
    nsRefPtr<nsMediaList> media = new nsMediaList();
    media->SetStyleSheet(GetStyleSheet());
    nsresult rv = media->SetMediaText(aConditionText);
    if (NS_SUCCEEDED(rv)) {
      mMedia = media;
    }
    return rv;
  }

  return mMedia->SetMediaText(aConditionText);
}

// nsIDOMCSSMediaRule methods
NS_IMETHODIMP
MediaRule::GetMedia(nsIDOMMediaList* *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  NS_IF_ADDREF(*aMedia = mMedia);
  return NS_OK;
}

// GroupRule interface
/* virtual */ bool
MediaRule::UseForPresentation(nsPresContext* aPresContext,
                                   nsMediaQueryResultCacheKey& aKey)
{
  if (mMedia) {
    return mMedia->Matches(aPresContext, &aKey);
  }
  return true;
}

/* virtual */ size_t
MediaRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mMedia

  return n;
}

void
MediaRule::AppendConditionText(nsAString& aOutput)
{
  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    aOutput.Append(mediaText);
  }
}

} // namespace css
} // namespace mozilla

// Must be outside namespace
DOMCI_DATA(CSSMediaRule, css::MediaRule)

namespace mozilla {
namespace css {

DocumentRule::DocumentRule()
{
}

DocumentRule::DocumentRule(const DocumentRule& aCopy)
  : GroupRule(aCopy)
  , mURLs(new URL(*aCopy.mURLs))
{
}

DocumentRule::~DocumentRule()
{
}

NS_IMPL_ADDREF_INHERITED(DocumentRule, GroupRule)
NS_IMPL_RELEASE_INHERITED(DocumentRule, GroupRule)

// QueryInterface implementation for DocumentRule
NS_INTERFACE_MAP_BEGIN(DocumentRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSMozDocumentRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

#ifdef DEBUG
/* virtual */ void
DocumentRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoCString str;
  str.AssignLiteral("@-moz-document ");
  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL:
        str.AppendLiteral("url(\"");
        break;
      case eURLPrefix:
        str.AppendLiteral("url-prefix(\"");
        break;
      case eDomain:
        str.AppendLiteral("domain(\"");
        break;
      case eRegExp:
        str.AppendLiteral("regexp(\"");
        break;
    }
    nsAutoCString escapedURL(url->url);
    escapedURL.ReplaceSubstring("\"", "\\\""); // escape quotes
    str.Append(escapedURL);
    str.AppendLiteral("\"), ");
  }
  str.Cut(str.Length() - 2, 1); // remove last ,
  fputs(str.get(), out);

  GroupRule::List(out, aIndent);
}
#endif

/* virtual */ int32_t
DocumentRule::GetType() const
{
  return Rule::DOCUMENT_RULE;
}

/* virtual */ already_AddRefed<Rule>
DocumentRule::Clone() const
{
  nsRefPtr<Rule> clone = new DocumentRule(*this);
  return clone.forget();
}

// nsIDOMCSSRule methods
NS_IMETHODIMP
DocumentRule::GetType(uint16_t* aType)
{
  // XXX What should really happen here?
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
  return NS_OK;
}

NS_IMETHODIMP
DocumentRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@-moz-document ");
  AppendConditionText(aCssText);
  return GroupRule::AppendRulesToCssText(aCssText);
}

NS_IMETHODIMP
DocumentRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
DocumentRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return GroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
DocumentRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return GroupRule::GetParentRule(aParentRule);
}

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
DocumentRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
DocumentRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
DocumentRule::DeleteRule(uint32_t aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
DocumentRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Truncate(0);
  AppendConditionText(aConditionText);
  return NS_OK;
}

NS_IMETHODIMP
DocumentRule::SetConditionText(const nsAString& aConditionText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// GroupRule interface
/* virtual */ bool
DocumentRule::UseForPresentation(nsPresContext* aPresContext,
                                 nsMediaQueryResultCacheKey& aKey)
{
  nsIDocument *doc = aPresContext->Document();
  nsIURI *docURI = doc->GetDocumentURI();
  nsAutoCString docURISpec;
  if (docURI)
    docURI->GetSpec(docURISpec);

  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL: {
        if (docURISpec == url->url)
          return true;
      } break;
      case eURLPrefix: {
        if (StringBeginsWith(docURISpec, url->url))
          return true;
      } break;
      case eDomain: {
        nsAutoCString host;
        if (docURI)
          docURI->GetHost(host);
        int32_t lenDiff = host.Length() - url->url.Length();
        if (lenDiff == 0) {
          if (host == url->url)
            return true;
        } else {
          if (StringEndsWith(host, url->url) &&
              host.CharAt(lenDiff - 1) == '.')
            return true;
        }
      } break;
      case eRegExp: {
        NS_ConvertUTF8toUTF16 spec(docURISpec);
        NS_ConvertUTF8toUTF16 regex(url->url);
        if (nsContentUtils::IsPatternMatching(spec, regex, doc)) {
          return true;
        }
      } break;
    }
  }

  return false;
}

DocumentRule::URL::~URL()
{
  NS_CSS_DELETE_LIST_MEMBER(DocumentRule::URL, this, next);
}

/* virtual */ size_t
DocumentRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mURLs

  return n;
}

void
DocumentRule::AppendConditionText(nsAString& aCssText)
{
  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL:
        aCssText.AppendLiteral("url(");
        break;
      case eURLPrefix:
        aCssText.AppendLiteral("url-prefix(");
        break;
      case eDomain:
        aCssText.AppendLiteral("domain(");
        break;
      case eRegExp:
        aCssText.AppendLiteral("regexp(");
        break;
    }
    nsStyleUtil::AppendEscapedCSSString(NS_ConvertUTF8toUTF16(url->url),
                                        aCssText);
    aCssText.AppendLiteral("), ");
  }
  aCssText.Truncate(aCssText.Length() - 2); // remove last ", "
}

} // namespace css
} // namespace mozilla

// Must be outside namespace
DOMCI_DATA(CSSMozDocumentRule, css::DocumentRule)

// -------------------------------------------
// NameSpaceRule
//

namespace mozilla {
namespace css {

NameSpaceRule::NameSpaceRule(nsIAtom* aPrefix, const nsString& aURLSpec)
  : Rule(),
    mPrefix(aPrefix),
    mURLSpec(aURLSpec)
{
}

NameSpaceRule::NameSpaceRule(const NameSpaceRule& aCopy)
  : Rule(aCopy),
    mPrefix(aCopy.mPrefix),
    mURLSpec(aCopy.mURLSpec)
{
}

NameSpaceRule::~NameSpaceRule()
{
}

NS_IMPL_ADDREF(NameSpaceRule)
NS_IMPL_RELEASE(NameSpaceRule)

// QueryInterface implementation for NameSpaceRule
NS_INTERFACE_MAP_BEGIN(NameSpaceRule)
  if (aIID.Equals(NS_GET_IID(css::NameSpaceRule))) {
    *aInstancePtr = this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSNameSpaceRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(NameSpaceRule, Rule)

#ifdef DEBUG
/* virtual */ void
NameSpaceRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@namespace ", out);

  if (mPrefix) {
    mPrefix->ToString(buffer);
    fputs(NS_LossyConvertUTF16toASCII(buffer).get(), out);
    fputs(" ", out);
  }

  fputs("url(", out);
  fputs(NS_LossyConvertUTF16toASCII(mURLSpec).get(), out);
  fputs(")\n", out);
}
#endif

/* virtual */ int32_t
NameSpaceRule::GetType() const
{
  return Rule::NAMESPACE_RULE;
}

/* virtual */ already_AddRefed<Rule>
NameSpaceRule::Clone() const
{
  nsRefPtr<Rule> clone = new NameSpaceRule(*this);
  return clone.forget();
}

NS_IMETHODIMP
NameSpaceRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::NAMESPACE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@namespace ");
  if (mPrefix) {
    aCssText.Append(nsDependentAtomString(mPrefix) + NS_LITERAL_STRING(" "));
  }
  aCssText.AppendLiteral("url(");
  nsStyleUtil::AppendEscapedCSSString(mURLSpec, aCssText);
  aCssText.Append(NS_LITERAL_STRING(");"));
  return NS_OK;
}

NS_IMETHODIMP
NameSpaceRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
NameSpaceRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
NameSpaceRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

/* virtual */ size_t
NameSpaceRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mPrefix
  // - mURLSpec
}


} // namespace css
} // namespace mozilla

// Must be outside namespace
DOMCI_DATA(CSSNameSpaceRule, css::NameSpaceRule)

// -------------------------------------------
// nsCSSFontFaceStyleDecl and related routines
//

// A src: descriptor is represented as an array value; each entry in
// the array can be eCSSUnit_URL, eCSSUnit_Local_Font, or
// eCSSUnit_Font_Format.  Blocks of eCSSUnit_Font_Format may appear
// only after one of the first two.  (css3-fonts only contemplates
// annotating URLs with formats, but we handle the general case.)
static void
AppendSerializedFontSrc(const nsCSSValue& src, nsAString & aResult)
{
  NS_PRECONDITION(src.GetUnit() == eCSSUnit_Array,
                  "improper value unit for src:");

  const nsCSSValue::Array& sources = *src.GetArrayValue();
  size_t i = 0;

  while (i < sources.Count()) {
    nsAutoString formats;

    if (sources[i].GetUnit() == eCSSUnit_URL) {
      aResult.AppendLiteral("url(");
      nsDependentString url(sources[i].GetOriginalURLValue());
      nsStyleUtil::AppendEscapedCSSString(url, aResult);
      aResult.AppendLiteral(")");
    } else if (sources[i].GetUnit() == eCSSUnit_Local_Font) {
      aResult.AppendLiteral("local(");
      nsDependentString local(sources[i].GetStringBufferValue());
      nsStyleUtil::AppendEscapedCSSString(local, aResult);
      aResult.AppendLiteral(")");
    } else {
      NS_NOTREACHED("entry in src: descriptor with improper unit");
      i++;
      continue;
    }

    i++;
    formats.Truncate();
    while (i < sources.Count() &&
           sources[i].GetUnit() == eCSSUnit_Font_Format) {
      formats.Append('"');
      formats.Append(sources[i].GetStringBufferValue());
      formats.AppendLiteral("\", ");
      i++;
    }
    if (formats.Length() > 0) {
      formats.Truncate(formats.Length() - 2); // remove the last comma
      aResult.AppendLiteral(" format(");
      aResult.Append(formats);
      aResult.Append(')');
    }
    aResult.AppendLiteral(", ");
  }
  aResult.Truncate(aResult.Length() - 2); // remove the last comma-space
}

// print all characters with at least four hex digits
static void
AppendSerializedUnicodePoint(uint32_t aCode, nsACString &aBuf)
{
  aBuf.Append(nsPrintfCString("%04X", aCode));
}

// A unicode-range: descriptor is represented as an array of integers,
// to be interpreted as a sequence of pairs: min max min max ...
// It is in source order.  (Possibly it should be sorted and overlaps
// consolidated, but right now we don't do that.)
static void
AppendSerializedUnicodeRange(nsCSSValue const & aValue,
                             nsAString & aResult)
{
  NS_PRECONDITION(aValue.GetUnit() == eCSSUnit_Null ||
                  aValue.GetUnit() == eCSSUnit_Array,
                  "improper value unit for unicode-range:");
  aResult.Truncate();
  if (aValue.GetUnit() != eCSSUnit_Array)
    return;

  nsCSSValue::Array const & sources = *aValue.GetArrayValue();
  nsAutoCString buf;

  NS_ABORT_IF_FALSE(sources.Count() % 2 == 0,
                    "odd number of entries in a unicode-range: array");

  for (uint32_t i = 0; i < sources.Count(); i += 2) {
    uint32_t min = sources[i].GetIntValue();
    uint32_t max = sources[i+1].GetIntValue();

    // We don't try to replicate the U+XX?? notation.
    buf.AppendLiteral("U+");
    AppendSerializedUnicodePoint(min, buf);

    if (min != max) {
      buf.Append('-');
      AppendSerializedUnicodePoint(max, buf);
    }
    buf.AppendLiteral(", ");
  }
  buf.Truncate(buf.Length() - 2); // remove the last comma-space
  CopyASCIItoUTF16(buf, aResult);
}

// Mapping from nsCSSFontDesc codes to nsCSSFontFaceStyleDecl fields.
nsCSSValue nsCSSFontFaceStyleDecl::* const
nsCSSFontFaceStyleDecl::Fields[] = {
#define CSS_FONT_DESC(name_, method_) &nsCSSFontFaceStyleDecl::m##method_,
#include "nsCSSFontDescList.h"
#undef CSS_FONT_DESC
};

// QueryInterface implementation for nsCSSFontFaceStyleDecl
NS_INTERFACE_MAP_BEGIN(nsCSSFontFaceStyleDecl)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsICSSDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  // We forward the cycle collection interfaces to ContainingRule(), which is
  // never null (in fact, we're part of that object!)
  if (aIID.Equals(NS_GET_IID(nsCycleCollectionISupports)) ||
      aIID.Equals(NS_GET_IID(nsXPCOMCycleCollectionParticipant))) {
    return ContainingRule()->QueryInterface(aIID, aInstancePtr);
  }
  else
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())

// helper for string GetPropertyValue and RemovePropertyValue
nsresult
nsCSSFontFaceStyleDecl::GetPropertyValue(nsCSSFontDesc aFontDescID,
                                         nsAString & aResult) const
{
  NS_ENSURE_ARG_RANGE(aFontDescID, eCSSFontDesc_UNKNOWN,
                      eCSSFontDesc_COUNT - 1);

  aResult.Truncate();
  if (aFontDescID == eCSSFontDesc_UNKNOWN)
    return NS_OK;

  const nsCSSValue& val = this->*nsCSSFontFaceStyleDecl::Fields[aFontDescID];

  if (val.GetUnit() == eCSSUnit_Null) {
    // Avoid having to check no-value in the Family and Src cases below.
    return NS_OK;
  }

  switch (aFontDescID) {
  case eCSSFontDesc_Family: {
      // we don't use nsCSSValue::AppendToString here because it doesn't
      // canonicalize the way we want, and anyway it's overkill when
      // we know we have eCSSUnit_String
      NS_ASSERTION(val.GetUnit() == eCSSUnit_String, "unexpected unit");
      nsDependentString family(val.GetStringBufferValue());
      nsStyleUtil::AppendEscapedCSSString(family, aResult);
      return NS_OK;
    }

  case eCSSFontDesc_Style:
    val.AppendToString(eCSSProperty_font_style, aResult);
    return NS_OK;

  case eCSSFontDesc_Weight:
    val.AppendToString(eCSSProperty_font_weight, aResult);
    return NS_OK;

  case eCSSFontDesc_Stretch:
    val.AppendToString(eCSSProperty_font_stretch, aResult);
    return NS_OK;

  case eCSSFontDesc_FontFeatureSettings:
    nsStyleUtil::AppendFontFeatureSettings(val, aResult);
    return NS_OK;

  case eCSSFontDesc_FontLanguageOverride:
    val.AppendToString(eCSSProperty_font_language_override, aResult);
    return NS_OK;

  case eCSSFontDesc_Src:
    AppendSerializedFontSrc(val, aResult);
    return NS_OK;

  case eCSSFontDesc_UnicodeRange:
    AppendSerializedUnicodeRange(val, aResult);
    return NS_OK;

  case eCSSFontDesc_UNKNOWN:
  case eCSSFontDesc_COUNT:
    ;
  }
  NS_NOTREACHED("nsCSSFontFaceStyleDecl::GetPropertyValue: "
                "out-of-range value got to the switch");
  return NS_ERROR_INVALID_ARG;
}


// attribute DOMString cssText;
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetCssText(nsAString & aCssText)
{
  nsAutoString descStr;

  aCssText.Truncate();
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1)) {
    if ((this->*nsCSSFontFaceStyleDecl::Fields[id]).GetUnit()
          != eCSSUnit_Null &&
        NS_SUCCEEDED(GetPropertyValue(id, descStr))) {
      NS_ASSERTION(descStr.Length() > 0,
                   "GetCssText: non-null unit, empty property value");
      aCssText.AppendLiteral("  ");
      aCssText.AppendASCII(nsCSSProps::GetStringValue(id).get());
      aCssText.AppendLiteral(": ");
      aCssText.Append(descStr);
      aCssText.AppendLiteral(";\n");
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetCssText(const nsAString & aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

// DOMString getPropertyValue (in DOMString propertyName);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyValue(const nsAString & propertyName,
                                         nsAString & aResult)
{
  return GetPropertyValue(nsCSSProps::LookupFontDesc(propertyName), aResult);
}

// nsIDOMCSSValue getPropertyCSSValue (in DOMString propertyName);
already_AddRefed<dom::CSSValue>
nsCSSFontFaceStyleDecl::GetPropertyCSSValue(const nsAString & propertyName,
                                            ErrorResult& aRv)
{
  // ??? nsDOMCSSDeclaration returns null/NS_OK, but that seems wrong.
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
  return nullptr;
}

// DOMString removeProperty (in DOMString propertyName) raises (DOMException);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::RemoveProperty(const nsAString & propertyName,
                                       nsAString & aResult)
{
  nsCSSFontDesc descID = nsCSSProps::LookupFontDesc(propertyName);
  NS_ASSERTION(descID >= eCSSFontDesc_UNKNOWN &&
               descID < eCSSFontDesc_COUNT,
               "LookupFontDesc returned value out of range");

  if (descID == eCSSFontDesc_UNKNOWN) {
    aResult.Truncate();
  } else {
    nsresult rv = GetPropertyValue(descID, aResult);
    NS_ENSURE_SUCCESS(rv, rv);
    (this->*nsCSSFontFaceStyleDecl::Fields[descID]).Reset();
  }
  return NS_OK;
}

// DOMString getPropertyPriority (in DOMString propertyName);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyPriority(const nsAString & propertyName,
                                            nsAString & aResult)
{
  // font descriptors do not have priorities at present
  aResult.Truncate();
  return NS_OK;
}

// void setProperty (in DOMString propertyName, in DOMString value,
//                   in DOMString priority)  raises (DOMException);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetProperty(const nsAString & propertyName,
                                    const nsAString & value,
                                    const nsAString & priority)
{
  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

// readonly attribute unsigned long length;
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetLength(uint32_t *aLength)
{
  uint32_t len = 0;
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1))
    if ((this->*nsCSSFontFaceStyleDecl::Fields[id]).GetUnit() != eCSSUnit_Null)
      len++;

  *aLength = len;
  return NS_OK;
}

// DOMString item (in unsigned long index);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::Item(uint32_t aIndex, nsAString& aReturn)
{
  bool found;
  IndexedGetter(aIndex, found, aReturn);
  if (!found) {
    aReturn.Truncate();
  }
  return NS_OK;
}

void
nsCSSFontFaceStyleDecl::IndexedGetter(uint32_t index, bool& aFound, nsAString & aResult)
{
  int32_t nset = -1;
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1)) {
    if ((this->*nsCSSFontFaceStyleDecl::Fields[id]).GetUnit()
        != eCSSUnit_Null) {
      nset++;
      if (nset == int32_t(index)) {
        aFound = true;
        aResult.AssignASCII(nsCSSProps::GetStringValue(id).get());
        return;
      }
    }
  }
  aFound = false;
}

// readonly attribute nsIDOMCSSRule parentRule;
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_IF_ADDREF(*aParentRule = ContainingRule()->GetDOMRule());
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyValue(const nsCSSProperty aPropID,
                                         nsAString& aValue)
{
  return
    GetPropertyValue(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aPropID)),
                     aValue);
}

NS_IMETHODIMP
nsCSSFontFaceStyleDecl::SetPropertyValue(const nsCSSProperty aPropID,
                                         const nsAString& aValue)
{
  return SetProperty(NS_ConvertUTF8toUTF16(nsCSSProps::GetStringValue(aPropID)),
                     aValue, EmptyString());
}

nsINode*
nsCSSFontFaceStyleDecl::GetParentObject()
{
  return ContainingRule()->GetDocument();
}

JSObject*
nsCSSFontFaceStyleDecl::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope)
{
  return mozilla::dom::CSSStyleDeclarationBinding::Wrap(cx, scope, this);
}

// -------------------------------------------
// nsCSSFontFaceRule
// 

/* virtual */ already_AddRefed<css::Rule>
nsCSSFontFaceRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new nsCSSFontFaceRule(*this);
  return clone.forget();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSFontFaceRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSFontFaceRule)

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(nsCSSFontFaceRule)
  // Trace the wrapper for our declaration.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  tmp->mDecl.TraceWrapper(aCallbacks, aClosure);
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCSSFontFaceRule)
  // Unlink the wrapper for our declaraton.  This just expands out
  // NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER which we can't use
  // directly because the wrapper is on the declaration, not on us.
  nsContentUtils::ReleaseWrapper(static_cast<nsISupports*>(p), &tmp->mDecl);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCSSFontFaceRule)
  // Just NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS here: that will call
  // into our Trace hook, where we do the right thing with declarations
  // already.
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_SCRIPT_OBJECTS
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(CSSFontFaceRule, nsCSSFontFaceRule)

// QueryInterface implementation for nsCSSFontFaceRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSFontFaceRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFaceRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSFontFaceRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(nsCSSFontFaceRule, Rule)

#ifdef DEBUG
void
nsCSSFontFaceRule::List(FILE* out, int32_t aIndent) const
{
  nsCString baseInd, descInd;
  for (int32_t indent = aIndent; --indent >= 0; ) {
    baseInd.AppendLiteral("  ");
    descInd.AppendLiteral("  ");
  }
  descInd.AppendLiteral("  ");

  nsString descStr;

  fprintf(out, "%s@font-face {\n", baseInd.get());
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1))
    if ((mDecl.*nsCSSFontFaceStyleDecl::Fields[id]).GetUnit()
        != eCSSUnit_Null) {
      if (NS_FAILED(mDecl.GetPropertyValue(id, descStr)))
        descStr.AssignLiteral("#<serialization error>");
      else if (descStr.Length() == 0)
        descStr.AssignLiteral("#<serialization missing>");
      fprintf(out, "%s%s: %s\n",
              descInd.get(), nsCSSProps::GetStringValue(id).get(),
              NS_ConvertUTF16toUTF8(descStr).get());
    }
  fprintf(out, "%s}\n", baseInd.get());
}
#endif

/* virtual */ int32_t
nsCSSFontFaceRule::GetType() const
{
  return Rule::FONT_FACE_RULE;
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::FONT_FACE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetCssText(nsAString& aCssText)
{
  nsAutoString propText;
  mDecl.GetCssText(propText);

  aCssText.AssignLiteral("@font-face {\n");
  aCssText.Append(propText);
  aCssText.Append('}');
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFaceRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED; // bug 443978
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  NS_IF_ADDREF(*aStyle = &mDecl);
  return NS_OK;
}

// Arguably these should forward to nsCSSFontFaceStyleDecl methods.
void
nsCSSFontFaceRule::SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue)
{
  NS_PRECONDITION(aDescID > eCSSFontDesc_UNKNOWN &&
                  aDescID < eCSSFontDesc_COUNT,
                  "aDescID out of range in nsCSSFontFaceRule::SetDesc");

  // FIXME: handle dynamic changes

  mDecl.*nsCSSFontFaceStyleDecl::Fields[aDescID] = aValue;
}

void
nsCSSFontFaceRule::GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue)
{
  NS_PRECONDITION(aDescID > eCSSFontDesc_UNKNOWN &&
                  aDescID < eCSSFontDesc_COUNT,
                  "aDescID out of range in nsCSSFontFaceRule::GetDesc");

  aValue = mDecl.*nsCSSFontFaceStyleDecl::Fields[aDescID];
}

/* virtual */ size_t
nsCSSFontFaceRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mDecl
}


// -----------------------------------
// nsCSSFontFeatureValuesRule
//

/* virtual */ already_AddRefed<css::Rule>
nsCSSFontFeatureValuesRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new nsCSSFontFeatureValuesRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF(nsCSSFontFeatureValuesRule)
NS_IMPL_RELEASE(nsCSSFontFeatureValuesRule)

DOMCI_DATA(CSSFontFeatureValuesRule, nsCSSFontFeatureValuesRule)

// QueryInterface implementation for nsCSSFontFeatureValuesRule
NS_INTERFACE_MAP_BEGIN(nsCSSFontFeatureValuesRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFeatureValuesRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSFontFeatureValuesRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(nsCSSFontFeatureValuesRule, Rule)

static void
FamilyListToString(const nsTArray<nsString>& aFamilyList, nsAString& aOutStr)
{
  uint32_t i, n = aFamilyList.Length();

  for (i = 0; i < n; i++) {
    nsStyleUtil::AppendEscapedCSSString(aFamilyList[i], aOutStr);
    if (i != n - 1) {
      aOutStr.AppendLiteral(", ");
    }
  }
}

static void
FeatureValuesToString(
  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aFeatureValues,
  nsAString& aOutStr)
{
  uint32_t i, n;

  // append values
  n = aFeatureValues.Length();
  for (i = 0; i < n; i++) {
    const gfxFontFeatureValueSet::FeatureValues& fv = aFeatureValues[i];

    // @alternate
    aOutStr.AppendLiteral("  @");
    nsAutoString functAlt;
    nsStyleUtil::GetFunctionalAlternatesName(fv.alternate, functAlt);
    aOutStr.Append(functAlt);
    aOutStr.AppendLiteral(" {");

    // for each ident-values tuple
    uint32_t j, numValues = fv.valuelist.Length();
    for (j = 0; j < numValues; j++) {
      aOutStr.AppendLiteral(" ");
      const gfxFontFeatureValueSet::ValueList& vlist = fv.valuelist[j];
      nsStyleUtil::AppendEscapedCSSIdent(vlist.name, aOutStr);
      aOutStr.AppendLiteral(":");

      uint32_t k, numSelectors = vlist.featureSelectors.Length();
      for (k = 0; k < numSelectors; k++) {
        aOutStr.AppendLiteral(" ");
        aOutStr.AppendInt(vlist.featureSelectors[k]);
      }

      aOutStr.AppendLiteral(";");
    }
    aOutStr.AppendLiteral(" }\n");
  }
}

static void
FontFeatureValuesRuleToString(
  const nsTArray<nsString>& aFamilyList,
  const nsTArray<gfxFontFeatureValueSet::FeatureValues>& aFeatureValues,
  nsAString& aOutStr)
{
  aOutStr.AssignLiteral("@font-feature-values ");
  nsAutoString familyListStr, valueTextStr;
  FamilyListToString(aFamilyList, familyListStr);
  aOutStr.Append(familyListStr);
  aOutStr.AppendLiteral(" {\n");
  FeatureValuesToString(aFeatureValues, valueTextStr);
  aOutStr.Append(valueTextStr);
  aOutStr.AppendLiteral("}");
}

#ifdef DEBUG
void
nsCSSFontFeatureValuesRule::List(FILE* out, int32_t aIndent) const
{
  nsAutoString text;
  FontFeatureValuesRuleToString(mFamilyList, mFeatureValues, text);
  NS_ConvertUTF16toUTF8 utf8(text);

  // replace newlines with newlines plus indent spaces
  char* indent = new char[(aIndent + 1) * 2];
  int32_t i;
  for (i = 1; i < (aIndent + 1) * 2 - 1; i++) {
    indent[i] = 0x20;
  }
  indent[0] = 0xa;
  indent[aIndent * 2 + 1] = 0;
  utf8.ReplaceSubstring("\n", indent);
  delete [] indent;

  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fprintf(out, "%s\n", utf8.get());
}
#endif

/* virtual */ int32_t
nsCSSFontFeatureValuesRule::GetType() const
{
  return Rule::FONT_FEATURE_VALUES_RULE;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::FONT_FEATURE_VALUES_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetCssText(nsAString& aCssText)
{
  FontFeatureValuesRuleToString(mFamilyList, mFeatureValues, aCssText);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::SetCssText(const nsAString& aCssText)
{
  // FIXME: implement???
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetFontFamily(nsAString& aFontFamily)
{
  FamilyListToString(mFamilyList, aFontFamily);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::SetFontFamily(const nsAString& aFontFamily)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::GetValueText(nsAString& aValueText)
{
  FeatureValuesToString(mFeatureValues, aValueText);
  return NS_OK;
}

NS_IMETHODIMP
nsCSSFontFeatureValuesRule::SetValueText(const nsAString& aValueText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

struct MakeFamilyArray {
  MakeFamilyArray(nsTArray<nsString>& aFamilyArray)
    : familyArray(aFamilyArray), hasGeneric(false)
  {}

  static bool
  AddFamily(const nsString& aFamily, bool aGeneric, void* aData)
  {
    MakeFamilyArray *familyArr = reinterpret_cast<MakeFamilyArray*> (aData);
    if (!aGeneric && !aFamily.IsEmpty()) {
      familyArr->familyArray.AppendElement(aFamily);
    }
    if (aGeneric) {
      familyArr->hasGeneric = true;
    }
    return true;
  }

  nsTArray<nsString>& familyArray;
  bool hasGeneric;
};

void
nsCSSFontFeatureValuesRule::SetFamilyList(const nsAString& aFamilyList,
                                          bool& aContainsGeneric)
{
  nsFont font(aFamilyList, 0, 0, 0, 0, 0, 0);
  MakeFamilyArray families(mFamilyList);
  font.EnumerateFamilies(MakeFamilyArray::AddFamily, (void*) &families);
  aContainsGeneric = families.hasGeneric;
}

void
nsCSSFontFeatureValuesRule::AddValueList(int32_t aVariantAlternate,
                     nsTArray<gfxFontFeatureValueSet::ValueList>& aValueList)
{
  uint32_t i, len = mFeatureValues.Length();
  bool foundAlternate = false;

  // add to an existing list for a given property value
  for (i = 0; i < len; i++) {
    gfxFontFeatureValueSet::FeatureValues& f = mFeatureValues.ElementAt(i);

    if (f.alternate == uint32_t(aVariantAlternate)) {
      f.valuelist.AppendElements(aValueList);
      foundAlternate = true;
      break;
    }
  }

  // create a new list for a given property value
  if (!foundAlternate) {
    gfxFontFeatureValueSet::FeatureValues &f = *mFeatureValues.AppendElement();
    f.alternate = aVariantAlternate;
    f.valuelist.AppendElements(aValueList);
  }
}

size_t
nsCSSFontFeatureValuesRule::SizeOfIncludingThis(
  nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

// -------------------------------------------
// nsCSSKeyframeStyleDeclaration
//

nsCSSKeyframeStyleDeclaration::nsCSSKeyframeStyleDeclaration(nsCSSKeyframeRule *aRule)
  : mRule(aRule)
{
}

nsCSSKeyframeStyleDeclaration::~nsCSSKeyframeStyleDeclaration()
{
  NS_ASSERTION(!mRule, "DropReference not called.");
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSKeyframeStyleDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSKeyframeStyleDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsCSSKeyframeStyleDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSKeyframeStyleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

css::Declaration*
nsCSSKeyframeStyleDeclaration::GetCSSDeclaration(bool aAllocate)
{
  if (mRule) {
    return mRule->Declaration();
  } else {
    return nullptr;
  }
}

void
nsCSSKeyframeStyleDeclaration::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(mRule, aCSSParseEnv);
}

NS_IMETHODIMP
nsCSSKeyframeStyleDeclaration::GetParentRule(nsIDOMCSSRule **aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  NS_IF_ADDREF(*aParent = mRule);
  return NS_OK;
}

nsresult
nsCSSKeyframeStyleDeclaration::SetCSSDeclaration(css::Declaration* aDecl)
{
  NS_ABORT_IF_FALSE(aDecl, "must be non-null");
  mRule->ChangeDeclaration(aDecl);
  return NS_OK;
}

nsIDocument*
nsCSSKeyframeStyleDeclaration::DocToUpdate()
{
  return nullptr;
}

nsINode*
nsCSSKeyframeStyleDeclaration::GetParentObject()
{
  return mRule ? mRule->GetDocument() : nullptr;
}

// -------------------------------------------
// nsCSSKeyframeRule
//

nsCSSKeyframeRule::nsCSSKeyframeRule(const nsCSSKeyframeRule& aCopy)
  // copy everything except our reference count and mDOMDeclaration
  : Rule(aCopy)
  , mKeys(aCopy.mKeys)
  , mDeclaration(new css::Declaration(*aCopy.mDeclaration))
{
}

nsCSSKeyframeRule::~nsCSSKeyframeRule()
{
  if (mDOMDeclaration) {
    mDOMDeclaration->DropReference();
  }
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSKeyframeRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new nsCSSKeyframeRule(*this);
  return clone.forget();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSKeyframeRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSKeyframeRule)


NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCSSKeyframeRule)
  if (tmp->mDOMDeclaration) {
    tmp->mDOMDeclaration->DropReference();
    tmp->mDOMDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCSSKeyframeRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(MozCSSKeyframeRule, nsCSSKeyframeRule)

// QueryInterface implementation for nsCSSKeyframeRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSKeyframeRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozCSSKeyframeRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozCSSKeyframeRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT_GET_DOM_RULE_WEAK(nsCSSKeyframeRule, Rule)

/* virtual */ void
nsCSSKeyframeRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  // We need to implement MapRuleInfoInto because the animation manager
  // constructs a rule node pointing to us in order to compute the
  // styles it needs to animate.

  // The spec says that !important declarations should just be ignored
  NS_ASSERTION(!mDeclaration->HasImportantData(),
               "Keyframe rules has !important data");

  mDeclaration->MapNormalRuleInfoInto(aRuleData);
}

#ifdef DEBUG
void
nsCSSKeyframeRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t index = aIndent; --index >= 0; ) fputs("  ", out);

  nsAutoString tmp;
  DoGetKeyText(tmp);
  fputs(NS_ConvertUTF16toUTF8(tmp).get(), out);
  fputs(" ", out);
  mDeclaration->List(out, aIndent);
  fputs("\n", out);
}
#endif

/* virtual */ int32_t
nsCSSKeyframeRule::GetType() const
{
  return Rule::KEYFRAME_RULE;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::KEYFRAME_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetCssText(nsAString& aCssText)
{
  DoGetKeyText(aCssText);
  aCssText.AppendLiteral(" { ");
  nsAutoString tmp;
  mDeclaration->ToString(tmp);
  aCssText.Append(tmp);
  aCssText.AppendLiteral(" }");
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::SetCssText(const nsAString& aCssText)
{
  // FIXME: implement???
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetKeyText(nsAString& aKeyText)
{
  DoGetKeyText(aKeyText);
  return NS_OK;
}

void
nsCSSKeyframeRule::DoGetKeyText(nsAString& aKeyText) const
{
  aKeyText.Truncate();
  uint32_t i = 0, i_end = mKeys.Length();
  NS_ABORT_IF_FALSE(i_end != 0, "must have some keys");
  for (;;) {
    aKeyText.AppendFloat(mKeys[i] * 100.0f);
    aKeyText.Append(PRUnichar('%'));
    if (++i == i_end) {
      break;
    }
    aKeyText.AppendLiteral(", ");
  }
}

NS_IMETHODIMP
nsCSSKeyframeRule::SetKeyText(const nsAString& aKeyText)
{
  nsCSSParser parser;

  InfallibleTArray<float> newSelectors;
  // FIXME: pass filename and line number
  if (parser.ParseKeyframeSelectorString(aKeyText, nullptr, 0, newSelectors)) {
    newSelectors.SwapElements(mKeys);
  } else {
    // for now, we don't do anything if the parse fails
  }

  nsCSSStyleSheet* sheet = GetStyleSheet();
  if (sheet) {
    sheet->SetModifiedByChildRule();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  if (!mDOMDeclaration) {
    mDOMDeclaration = new nsCSSKeyframeStyleDeclaration(this);
  }
  NS_ADDREF(*aStyle = mDOMDeclaration);
  return NS_OK;
}

void
nsCSSKeyframeRule::ChangeDeclaration(css::Declaration* aDeclaration)
{
  // Be careful to not assign to an nsAutoPtr if we would be assigning
  // the thing it already holds.
  if (aDeclaration != mDeclaration) {
    mDeclaration = aDeclaration;
  }

  nsCSSStyleSheet* sheet = GetStyleSheet();
  if (sheet) {
    sheet->SetModifiedByChildRule();
  }
}

/* virtual */ size_t
nsCSSKeyframeRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mKeys
  // - mDeclaration
  // - mDOMDeclaration
}


// -------------------------------------------
// nsCSSKeyframesRule
//

nsCSSKeyframesRule::nsCSSKeyframesRule(const nsCSSKeyframesRule& aCopy)
  // copy everything except our reference count.  GroupRule's copy
  // constructor also doesn't copy the lazily-constructed
  // mRuleCollection.
  : GroupRule(aCopy),
    mName(aCopy.mName)
{
}

nsCSSKeyframesRule::~nsCSSKeyframesRule()
{
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSKeyframesRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new nsCSSKeyframesRule(*this);
  return clone.forget();
}

NS_IMPL_ADDREF_INHERITED(nsCSSKeyframesRule, css::GroupRule)
NS_IMPL_RELEASE_INHERITED(nsCSSKeyframesRule, css::GroupRule)

DOMCI_DATA(MozCSSKeyframesRule, nsCSSKeyframesRule)

// QueryInterface implementation for nsCSSKeyframesRule
NS_INTERFACE_MAP_BEGIN(nsCSSKeyframesRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMozCSSKeyframesRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(MozCSSKeyframesRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

#ifdef DEBUG
void
nsCSSKeyframesRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fprintf(out, "@keyframes %s", NS_ConvertUTF16toUTF8(mName).get());
  GroupRule::List(out, aIndent);
}
#endif

/* virtual */ int32_t
nsCSSKeyframesRule::GetType() const
{
  return Rule::KEYFRAMES_RULE;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::KEYFRAMES_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@keyframes ");
  aCssText.Append(mName);
  aCssText.AppendLiteral(" {\n");
  nsAutoString tmp;
  for (uint32_t i = 0, i_end = mRules.Count(); i != i_end; ++i) {
    static_cast<nsCSSKeyframeRule*>(mRules[i])->GetCssText(tmp);
    aCssText.Append(tmp);
    aCssText.AppendLiteral("\n");
  }
  aCssText.AppendLiteral("}");
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::SetCssText(const nsAString& aCssText)
{
  // FIXME: implement???
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return GroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return GroupRule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::SetName(const nsAString& aName)
{
  mName = aName;

  nsCSSStyleSheet* sheet = GetStyleSheet();
  if (sheet) {
    sheet->SetModifiedByChildRule();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
nsCSSKeyframesRule::AppendRule(const nsAString& aRule)
{
  // The spec is confusing, and I think we should just append the rule,
  // which also turns out to match WebKit:
  // http://lists.w3.org/Archives/Public/www-style/2011Apr/0034.html
  nsCSSParser parser;

  // FIXME: pass filename and line number
  nsRefPtr<nsCSSKeyframeRule> rule =
    parser.ParseKeyframeRule(aRule, nullptr, 0);
  if (rule) {
    AppendStyleRule(rule);
  }

  return NS_OK;
}

static const uint32_t RULE_NOT_FOUND = uint32_t(-1);

uint32_t
nsCSSKeyframesRule::FindRuleIndexForKey(const nsAString& aKey)
{
  nsCSSParser parser;

  InfallibleTArray<float> keys;
  // FIXME: pass filename and line number
  if (parser.ParseKeyframeSelectorString(aKey, nullptr, 0, keys)) {
    // The spec isn't clear, but we'll match on the key list, which
    // mostly matches what WebKit does, except we'll do last-match
    // instead of first-match, and handling parsing differences better.
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0036.html
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0037.html
    for (uint32_t i = mRules.Count(); i-- != 0; ) {
      if (static_cast<nsCSSKeyframeRule*>(mRules[i])->GetKeys() == keys) {
        return i;
      }
    }
  }

  return RULE_NOT_FOUND;
}

NS_IMETHODIMP
nsCSSKeyframesRule::DeleteRule(const nsAString& aKey)
{
  uint32_t index = FindRuleIndexForKey(aKey);
  if (index != RULE_NOT_FOUND) {
    mRules.RemoveObjectAt(index);
    nsCSSStyleSheet* sheet = GetStyleSheet();
    if (sheet) {
      sheet->SetModifiedByChildRule();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::FindRule(const nsAString& aKey,
                             nsIDOMMozCSSKeyframeRule** aResult)
{
  uint32_t index = FindRuleIndexForKey(aKey);
  if (index == RULE_NOT_FOUND) {
    *aResult = nullptr;
  } else {
    NS_ADDREF(*aResult = static_cast<nsCSSKeyframeRule*>(mRules[index]));
  }
  return NS_OK;
}

// GroupRule interface
/* virtual */ bool
nsCSSKeyframesRule::UseForPresentation(nsPresContext* aPresContext,
                                       nsMediaQueryResultCacheKey& aKey)
{
  NS_ABORT_IF_FALSE(false, "should not be called");
  return false;
}

/* virtual */ size_t
nsCSSKeyframesRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += GroupRule::SizeOfExcludingThis(aMallocSizeOf);

  // Measurement of the following members may be added later if DMD finds it is
  // worthwhile:
  // - mName

  return n;
}

// -------------------------------------------
// nsCSSPageStyleDeclaration
//

nsCSSPageStyleDeclaration::nsCSSPageStyleDeclaration(nsCSSPageRule* aRule)
  : mRule(aRule)
{
}

nsCSSPageStyleDeclaration::~nsCSSPageStyleDeclaration()
{
  NS_ASSERTION(!mRule, "DropReference not called.");
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSPageStyleDeclaration)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSPageStyleDeclaration)

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(nsCSSPageStyleDeclaration)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSPageStyleDeclaration)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsDOMCSSDeclaration)

css::Declaration*
nsCSSPageStyleDeclaration::GetCSSDeclaration(bool aAllocate)
{
  if (mRule) {
    return mRule->Declaration();
  } else {
    return nullptr;
  }
}

void
nsCSSPageStyleDeclaration::GetCSSParsingEnvironment(CSSParsingEnvironment& aCSSParseEnv)
{
  GetCSSParsingEnvironmentForRule(mRule, aCSSParseEnv);
}

NS_IMETHODIMP
nsCSSPageStyleDeclaration::GetParentRule(nsIDOMCSSRule** aParent)
{
  NS_ENSURE_ARG_POINTER(aParent);

  NS_IF_ADDREF(*aParent = mRule);
  return NS_OK;
}

nsresult
nsCSSPageStyleDeclaration::SetCSSDeclaration(css::Declaration* aDecl)
{
  NS_ABORT_IF_FALSE(aDecl, "must be non-null");
  mRule->ChangeDeclaration(aDecl);
  return NS_OK;
}

nsIDocument*
nsCSSPageStyleDeclaration::DocToUpdate()
{
  return nullptr;
}

nsINode*
nsCSSPageStyleDeclaration::GetParentObject()
{
  return mRule ? mRule->GetDocument() : nullptr;
}

// -------------------------------------------
// nsCSSPageRule
//

nsCSSPageRule::nsCSSPageRule(const nsCSSPageRule& aCopy)
  // copy everything except our reference count and mDOMDeclaration
  : Rule(aCopy)
  , mDeclaration(new css::Declaration(*aCopy.mDeclaration))
{
}

nsCSSPageRule::~nsCSSPageRule()
{
  if (mDOMDeclaration) {
    mDOMDeclaration->DropReference();
  }
}

/* virtual */ already_AddRefed<css::Rule>
nsCSSPageRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new nsCSSPageRule(*this);
  return clone.forget();
}

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsCSSPageRule)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsCSSPageRule)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsCSSPageRule)
  if (tmp->mDOMDeclaration) {
    tmp->mDOMDeclaration->DropReference();
    tmp->mDOMDeclaration = nullptr;
  }
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsCSSPageRule)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mDOMDeclaration)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

DOMCI_DATA(CSSPageRule, nsCSSPageRule)

// QueryInterface implementation for nsCSSPageRule
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsCSSPageRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSPageRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSPageRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT_GET_DOM_RULE_WEAK(nsCSSPageRule, Rule)

#ifdef DEBUG
void
nsCSSPageRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@page ", out);
  mDeclaration->List(out, aIndent);
  fputs("\n", out);
}
#endif

/* virtual */ int32_t
nsCSSPageRule::GetType() const
{
  return Rule::PAGE_RULE;
}

NS_IMETHODIMP
nsCSSPageRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::PAGE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSPageRule::GetCssText(nsAString& aCssText)
{
  aCssText.AppendLiteral("@page { ");
  nsAutoString tmp;
  mDeclaration->ToString(tmp);
  aCssText.Append(tmp);
  aCssText.AppendLiteral(" }");
  return NS_OK;
}

NS_IMETHODIMP
nsCSSPageRule::SetCssText(const nsAString& aCssText)
{
  // FIXME: implement???
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSPageRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return Rule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSPageRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return Rule::GetParentRule(aParentRule);
}

css::ImportantRule*
nsCSSPageRule::GetImportantRule()
{
  if (!mDeclaration->HasImportantData()) {
    return nullptr;
  }
  if (!mImportantRule) {
    mImportantRule = new css::ImportantRule(mDeclaration);
  }
  return mImportantRule;
}

/* virtual */ void
nsCSSPageRule::MapRuleInfoInto(nsRuleData* aRuleData)
{
  mDeclaration->MapNormalRuleInfoInto(aRuleData);
}

NS_IMETHODIMP
nsCSSPageRule::GetStyle(nsIDOMCSSStyleDeclaration** aStyle)
{
  if (!mDOMDeclaration) {
    mDOMDeclaration = new nsCSSPageStyleDeclaration(this);
  }
  NS_ADDREF(*aStyle = mDOMDeclaration);
  return NS_OK;
}

void
nsCSSPageRule::ChangeDeclaration(css::Declaration* aDeclaration)
{
  mImportantRule = nullptr;
  // Be careful to not assign to an nsAutoPtr if we would be assigning
  // the thing it already holds.
  if (aDeclaration != mDeclaration) {
    mDeclaration = aDeclaration;
  }

  nsCSSStyleSheet* sheet = GetStyleSheet();
  if (sheet) {
    sheet->SetModifiedByChildRule();
  }
}

/* virtual */ size_t
nsCSSPageRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  return aMallocSizeOf(this);
}

namespace mozilla {

CSSSupportsRule::CSSSupportsRule(bool aConditionMet,
                                 const nsString& aCondition)
  : mUseGroup(aConditionMet),
    mCondition(aCondition)
{
}

CSSSupportsRule::CSSSupportsRule(const CSSSupportsRule& aCopy)
  : css::GroupRule(aCopy),
    mUseGroup(aCopy.mUseGroup),
    mCondition(aCopy.mCondition)
{
}

#ifdef DEBUG
/* virtual */ void
CSSSupportsRule::List(FILE* out, int32_t aIndent) const
{
  for (int32_t indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@supports ", out);
  fputs(NS_ConvertUTF16toUTF8(mCondition).get(), out);
  css::GroupRule::List(out, aIndent);
}
#endif

/* virtual */ int32_t
CSSSupportsRule::GetType() const
{
  return Rule::SUPPORTS_RULE;
}

/* virtual */ already_AddRefed<mozilla::css::Rule>
CSSSupportsRule::Clone() const
{
  nsRefPtr<css::Rule> clone = new CSSSupportsRule(*this);
  return clone.forget();
}

/* virtual */ bool
CSSSupportsRule::UseForPresentation(nsPresContext* aPresContext,
                                   nsMediaQueryResultCacheKey& aKey)
{
  return mUseGroup;
}

NS_IMPL_ADDREF_INHERITED(CSSSupportsRule, css::GroupRule)
NS_IMPL_RELEASE_INHERITED(CSSSupportsRule, css::GroupRule)

// QueryInterface implementation for CSSSupportsRule
NS_INTERFACE_MAP_BEGIN(CSSSupportsRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSGroupingRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSConditionRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSSupportsRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSSupportsRule)
NS_INTERFACE_MAP_END_INHERITING(GroupRule)

// nsIDOMCSSRule methods
NS_IMETHODIMP
CSSSupportsRule::GetType(uint16_t* aType)
{
  *aType = nsIDOMCSSRule::SUPPORTS_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSSupportsRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@supports ");
  aCssText.Append(mCondition);
  return css::GroupRule::AppendRulesToCssText(aCssText);
}

NS_IMETHODIMP
CSSSupportsRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSSupportsRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return css::GroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSSupportsRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return css::GroupRule::GetParentRule(aParentRule);
}

// nsIDOMCSSGroupingRule methods
NS_IMETHODIMP
CSSSupportsRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return css::GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
CSSSupportsRule::InsertRule(const nsAString & aRule, uint32_t aIndex, uint32_t* _retval)
{
  return css::GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
CSSSupportsRule::DeleteRule(uint32_t aIndex)
{
  return css::GroupRule::DeleteRule(aIndex);
}

// nsIDOMCSSConditionRule methods
NS_IMETHODIMP
CSSSupportsRule::GetConditionText(nsAString& aConditionText)
{
  aConditionText.Assign(mCondition);
  return NS_OK;
}

NS_IMETHODIMP
CSSSupportsRule::SetConditionText(const nsAString& aConditionText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* virtual */ size_t
CSSSupportsRule::SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const
{
  size_t n = aMallocSizeOf(this);
  n += css::GroupRule::SizeOfExcludingThis(aMallocSizeOf);
  n += mCondition.SizeOfExcludingThisIfUnshared(aMallocSizeOf);
  return n;
}

} // namespace mozilla

// Must be outside namespace
DOMCI_DATA(CSSSupportsRule, mozilla::CSSSupportsRule)

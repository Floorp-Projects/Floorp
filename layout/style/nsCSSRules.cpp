/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* rules in a CSS stylesheet other than style rules (e.g., @import rules) */

#include "mozilla/Attributes.h"

#include "nsCSSRules.h"
#include "nsCSSValue.h"
#include "mozilla/css/ImportRule.h"
#include "mozilla/css/NameSpaceRule.h"

#include "nsString.h"
#include "nsIAtom.h"
#include "nsIURL.h"

#include "nsCSSProps.h"
#include "nsCSSStyleSheet.h"

#include "nsCOMPtr.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIMediaList.h"
#include "nsICSSRuleList.h"
#include "nsIDocument.h"
#include "nsPresContext.h"
#include "nsRuleNode.h"

#include "nsContentUtils.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"
#include "nsStyleUtil.h"
#include "mozilla/css/Declaration.h"
#include "nsCSSParser.h"
#include "nsPrintfCString.h"

namespace css = mozilla::css;

#define IMPL_STYLE_RULE_INHERIT_GET_DOM_RULE_WEAK(class_, super_) \
/* virtual */ nsIDOMCSSRule* class_::GetDOMRule() \
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

NS_IMPL_ADDREF(Rule)
NS_IMPL_RELEASE(Rule)

/* virtual */ void
Rule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  // We don't reference count this up reference. The style sheet
  // will tell us when it's going away or when we're detached from
  // it.
  mSheet = aSheet;
}

nsresult
Rule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    NS_IF_ADDREF(*aParentRule = mParentRule->GetDOMRule());
  } else {
    *aParentRule = nsnull;
  }
  return NS_OK;
}

nsresult
Rule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);

  NS_IF_ADDREF(*aSheet = mSheet);
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

  virtual nsIDOMCSSRule* GetItemAt(PRUint32 aIndex, nsresult* aResult);

  void DropReference() { mGroupRule = nsnull; }

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
GroupRuleRuleList::GetLength(PRUint32* aLength)
{
  if (mGroupRule) {
    *aLength = (PRUint32)mGroupRule->StyleRuleCount();
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

nsIDOMCSSRule*
GroupRuleRuleList::GetItemAt(PRUint32 aIndex, nsresult* aResult)
{
  *aResult = NS_OK;

  if (mGroupRule) {
    nsRefPtr<Rule> rule = mGroupRule->GetStyleRuleAt(aIndex);
    if (rule) {
      return rule->GetDOMRule();
    }
  }

  return nsnull;
}

NS_IMETHODIMP
GroupRuleRuleList::Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn)
{
  nsresult rv;
  nsIDOMCSSRule* rule = GetItemAt(aIndex, &rv);
  if (!rule) {
    *aReturn = nsnull;
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

NS_IMPL_ADDREF_INHERITED(CharsetRule, Rule)
NS_IMPL_RELEASE_INHERITED(CharsetRule, Rule)

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
CharsetRule::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@charset \"", out);
  fputs(NS_LossyConvertUTF16toASCII(mEncoding).get(), out);
  fputs("\"\n", out);
}
#endif

/* virtual */ PRInt32
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
CharsetRule::GetType(PRUint16* aType)
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
      aCopy.mChildSheet->Clone(nsnull, this, nsnull, nsnull);
    SetSheet(sheet);
    // SetSheet sets mMedia appropriately
  }
}

ImportRule::~ImportRule()
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nsnull);
  }
}

NS_IMPL_ADDREF_INHERITED(ImportRule, Rule)
NS_IMPL_RELEASE_INHERITED(ImportRule, Rule)

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
ImportRule::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@import \"", out);
  fputs(NS_LossyConvertUTF16toASCII(mURLSpec).get(), out);
  fputs("\" ", out);

  nsAutoString mediaText;
  mMedia->GetText(mediaText);
  fputs(NS_LossyConvertUTF16toASCII(mediaText).get(), out);
  fputs("\n", out);
}
#endif

/* virtual */ PRInt32
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
ImportRule::GetType(PRUint16* aType)
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
  mRules.EnumerateForwards(SetParentRuleReference, nsnull);
  if (mRuleCollection) {
    mRuleCollection->DropReference();
  }
}

IMPL_STYLE_RULE_INHERIT_MAP_RULE_INFO_INTO(GroupRule, Rule)

static bool
SetStyleSheetReference(Rule* aRule, void* aSheet)
{
  nsCSSStyleSheet* sheet = (nsCSSStyleSheet*)aSheet;
  aRule->SetStyleSheet(sheet);
  return true;
}

/* virtual */ void
GroupRule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  mRules.EnumerateForwards(SetStyleSheetReference, aSheet);
  Rule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
/* virtual */ void
GroupRule::List(FILE* out, PRInt32 aIndent) const
{
  fputs(" {\n", out);

  for (PRInt32 index = 0, count = mRules.Count(); index < count; ++index) {
    mRules.ObjectAt(index)->List(out, aIndent + 1);
  }
  fputs("}\n", out);
}
#endif

void
GroupRule::AppendStyleRule(Rule* aRule)
{
  mRules.AppendObject(aRule);
  aRule->SetStyleSheet(mSheet);
  aRule->SetParentRule(this);
  if (mSheet) {
    mSheet->SetModifiedByChildRule();
  }
}

Rule*
GroupRule::GetStyleRuleAt(PRInt32 aIndex) const
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
 * The next two methods (DeleteStyleRuleAt and InsertStyleRulesAt)
 * should never be called unless you have first called WillDirty() on
 * the parents tylesheet.  After they are called, DidDirty() needs to
 * be called on the sheet
 */
nsresult
GroupRule::DeleteStyleRuleAt(PRUint32 aIndex)
{
  Rule* rule = mRules.SafeObjectAt(aIndex);
  if (rule) {
    rule->SetStyleSheet(nsnull);
    rule->SetParentRule(nsnull);
  }
  return mRules.RemoveObjectAt(aIndex) ? NS_OK : NS_ERROR_ILLEGAL_VALUE;
}

nsresult
GroupRule::InsertStyleRulesAt(PRUint32 aIndex,
                              nsCOMArray<Rule>& aRules)
{
  aRules.EnumerateForwards(SetStyleSheetReference, mSheet);
  aRules.EnumerateForwards(SetParentRuleReference, this);
  if (! mRules.InsertObjectsAt(aRules, aIndex)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
GroupRule::ReplaceStyleRule(Rule* aOld, Rule* aNew)
{
  PRInt32 index = mRules.IndexOf(aOld);
  NS_ENSURE_TRUE(index != -1, NS_ERROR_UNEXPECTED);
  mRules.ReplaceObjectAt(aNew, index);
  aNew->SetStyleSheet(mSheet);
  aNew->SetParentRule(this);
  aOld->SetStyleSheet(nsnull);
  aOld->SetParentRule(nsnull);
  return NS_OK;
}

nsresult
GroupRule::AppendRulesToCssText(nsAString& aCssText)
{
  aCssText.AppendLiteral(" {\n");

  // get all the rules
  for (PRInt32 index = 0, count = mRules.Count(); index < count; ++index) {
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
GroupRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  NS_ENSURE_TRUE(mSheet, NS_ERROR_FAILURE);
  
  if (aIndex > PRUint32(mRules.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ASSERTION(PRUint32(mRules.Count()) <= PR_INT32_MAX,
               "Too many style rules!");

  return mSheet->InsertRuleIntoGroup(aRule, this, aIndex, _retval);
}

nsresult
GroupRule::DeleteRule(PRUint32 aIndex)
{
  NS_ENSURE_TRUE(mSheet, NS_ERROR_FAILURE);

  if (aIndex >= PRUint32(mRules.Count()))
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  NS_ASSERTION(PRUint32(mRules.Count()) <= PR_INT32_MAX,
               "Too many style rules!");

  return mSheet->DeleteRuleFromGroup(this, aIndex);
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
      mMedia->SetStyleSheet(aCopy.mSheet);
    }
  }
}

MediaRule::~MediaRule()
{
  if (mMedia) {
    mMedia->SetStyleSheet(nsnull);
  }
}

NS_IMPL_ADDREF_INHERITED(MediaRule, GroupRule)
NS_IMPL_RELEASE_INHERITED(MediaRule, GroupRule)

// QueryInterface implementation for MediaRule
NS_INTERFACE_MAP_BEGIN(MediaRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMediaRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSMediaRule)
NS_INTERFACE_MAP_END

/* virtual */ void
MediaRule::SetStyleSheet(nsCSSStyleSheet* aSheet)
{
  if (mMedia) {
    // Set to null so it knows it's leaving one sheet and joining another.
    mMedia->SetStyleSheet(nsnull);
    mMedia->SetStyleSheet(aSheet);
  }

  GroupRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
/* virtual */ void
MediaRule::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

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

/* virtual */ PRInt32
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
    mMedia->SetStyleSheet(mSheet);
  return NS_OK;
}

// nsIDOMCSSRule methods
NS_IMETHODIMP
MediaRule::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
MediaRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@media ");
  // get all the media
  if (mMedia) {
    nsAutoString mediaText;
    mMedia->GetText(mediaText);
    aCssText.Append(mediaText);
  }

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

// nsIDOMCSSMediaRule methods
NS_IMETHODIMP
MediaRule::GetMedia(nsIDOMMediaList* *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  NS_IF_ADDREF(*aMedia = mMedia);
  return NS_OK;
}

NS_IMETHODIMP
MediaRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
MediaRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
MediaRule::DeleteRule(PRUint32 aIndex)
{
  return GroupRule::DeleteRule(aIndex);
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
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSMozDocumentRule)
NS_INTERFACE_MAP_END

#ifdef DEBUG
/* virtual */ void
DocumentRule::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsCAutoString str;
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
    nsCAutoString escapedURL(url->url);
    escapedURL.ReplaceSubstring("\"", "\\\""); // escape quotes
    str.Append(escapedURL);
    str.AppendLiteral("\"), ");
  }
  str.Cut(str.Length() - 2, 1); // remove last ,
  fputs(str.get(), out);

  GroupRule::List(out, aIndent);
}
#endif

/* virtual */ PRInt32
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
DocumentRule::GetType(PRUint16* aType)
{
  // XXX What should really happen here?
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
  return NS_OK;
}

NS_IMETHODIMP
DocumentRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@-moz-document ");
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
  aCssText.Cut(aCssText.Length() - 2, 1); // remove last ,

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

NS_IMETHODIMP
DocumentRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
DocumentRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  return GroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
DocumentRule::DeleteRule(PRUint32 aIndex)
{
  return GroupRule::DeleteRule(aIndex);
}

// GroupRule interface
/* virtual */ bool
DocumentRule::UseForPresentation(nsPresContext* aPresContext,
                                 nsMediaQueryResultCacheKey& aKey)
{
  nsIDocument *doc = aPresContext->Document();
  nsIURI *docURI = doc->GetDocumentURI();
  nsCAutoString docURISpec;
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
        nsCAutoString host;
        if (docURI)
          docURI->GetHost(host);
        PRInt32 lenDiff = host.Length() - url->url.Length();
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

NS_IMPL_ADDREF_INHERITED(NameSpaceRule, Rule)
NS_IMPL_RELEASE_INHERITED(NameSpaceRule, Rule)

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
NameSpaceRule::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

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

/* virtual */ PRInt32
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
NameSpaceRule::GetType(PRUint16* aType)
{
  // XXX What should really happen here?
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
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
AppendSerializedFontSrc(const nsCSSValue& src, nsAString & aResult NS_OUTPARAM)
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
AppendSerializedUnicodePoint(PRUint32 aCode, nsACString &aBuf NS_OUTPARAM)
{
  aBuf.Append(nsPrintfCString("%04X", aCode));
}

// A unicode-range: descriptor is represented as an array of integers,
// to be interpreted as a sequence of pairs: min max min max ...
// It is in source order.  (Possibly it should be sorted and overlaps
// consolidated, but right now we don't do that.)
static void
AppendSerializedUnicodeRange(nsCSSValue const & aValue,
                             nsAString & aResult NS_OUTPARAM)
{
  NS_PRECONDITION(aValue.GetUnit() == eCSSUnit_Null ||
                  aValue.GetUnit() == eCSSUnit_Array,
                  "improper value unit for unicode-range:");
  aResult.Truncate();
  if (aValue.GetUnit() != eCSSUnit_Array)
    return;

  nsCSSValue::Array const & sources = *aValue.GetArrayValue();
  nsCAutoString buf;

  NS_ABORT_IF_FALSE(sources.Count() % 2 == 0,
                    "odd number of entries in a unicode-range: array");

  for (PRUint32 i = 0; i < sources.Count(); i += 2) {
    PRUint32 min = sources[i].GetIntValue();
    PRUint32 max = sources[i+1].GetIntValue();

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

DOMCI_DATA(CSSFontFaceStyleDecl, nsCSSFontFaceStyleDecl)

// QueryInterface implementation for nsCSSFontFaceStyleDecl
NS_INTERFACE_MAP_BEGIN(nsCSSFontFaceStyleDecl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSStyleDeclaration)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSFontFaceStyleDecl)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())
NS_IMPL_RELEASE_USING_AGGREGATOR(nsCSSFontFaceStyleDecl, ContainingRule())

// helper for string GetPropertyValue and RemovePropertyValue
nsresult
nsCSSFontFaceStyleDecl::GetPropertyValue(nsCSSFontDesc aFontDescID,
                                         nsAString & aResult NS_OUTPARAM) const
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
                                         nsAString & aResult NS_OUTPARAM)
{
  return GetPropertyValue(nsCSSProps::LookupFontDesc(propertyName), aResult);
}

// nsIDOMCSSValue getPropertyCSSValue (in DOMString propertyName);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetPropertyCSSValue(const nsAString & propertyName,
                                            nsIDOMCSSValue **aResult NS_OUTPARAM)
{
  // ??? nsDOMCSSDeclaration returns null/NS_OK, but that seems wrong.
  return NS_ERROR_NOT_IMPLEMENTED;
}

// DOMString removeProperty (in DOMString propertyName) raises (DOMException);
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::RemoveProperty(const nsAString & propertyName,
                                       nsAString & aResult NS_OUTPARAM)
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
                                            nsAString & aResult NS_OUTPARAM)
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
nsCSSFontFaceStyleDecl::GetLength(PRUint32 *aLength)
{
  PRUint32 len = 0;
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
nsCSSFontFaceStyleDecl::Item(PRUint32 index, nsAString & aResult NS_OUTPARAM)
 {
  PRInt32 nset = -1;
  for (nsCSSFontDesc id = nsCSSFontDesc(eCSSFontDesc_UNKNOWN + 1);
       id < eCSSFontDesc_COUNT;
       id = nsCSSFontDesc(id + 1)) {
    if ((this->*nsCSSFontFaceStyleDecl::Fields[id]).GetUnit()
        != eCSSUnit_Null) {
      nset++;
      if (nset == PRInt32(index)) {
        aResult.AssignASCII(nsCSSProps::GetStringValue(id).get());
        return NS_OK;
      }
    }
  }
  aResult.Truncate();
  return NS_OK;
}

// readonly attribute nsIDOMCSSRule parentRule;
NS_IMETHODIMP
nsCSSFontFaceStyleDecl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  NS_IF_ADDREF(*aParentRule = ContainingRule()->GetDOMRule());
  return NS_OK;
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

NS_IMPL_ADDREF_INHERITED(nsCSSFontFaceRule, Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSFontFaceRule, Rule)

DOMCI_DATA(CSSFontFaceRule, nsCSSFontFaceRule)

// QueryInterface implementation for nsCSSFontFaceRule
NS_INTERFACE_MAP_BEGIN(nsCSSFontFaceRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSFontFaceRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStyleRule)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO(CSSFontFaceRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(nsCSSFontFaceRule, Rule)

#ifdef DEBUG
void
nsCSSFontFaceRule::List(FILE* out, PRInt32 aIndent) const
{
  nsCString baseInd, descInd;
  for (PRInt32 indent = aIndent; --indent >= 0; ) {
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

/* virtual */ PRInt32
nsCSSFontFaceRule::GetType() const
{
  return Rule::FONT_FACE_RULE;
}

NS_IMETHODIMP
nsCSSFontFaceRule::GetType(PRUint16* aType)
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

NS_IMPL_ADDREF(nsCSSKeyframeStyleDeclaration)
NS_IMPL_RELEASE(nsCSSKeyframeStyleDeclaration)

css::Declaration*
nsCSSKeyframeStyleDeclaration::GetCSSDeclaration(bool aAllocate)
{
  if (mRule) {
    return mRule->Declaration();
  } else {
    return nsnull;
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
  return nsnull;
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

NS_IMPL_ADDREF_INHERITED(nsCSSKeyframeRule, Rule)
NS_IMPL_RELEASE_INHERITED(nsCSSKeyframeRule, Rule)

DOMCI_DATA(MozCSSKeyframeRule, nsCSSKeyframeRule)

// QueryInterface implementation for nsCSSKeyframeRule
NS_INTERFACE_MAP_BEGIN(nsCSSKeyframeRule)
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

  // FIXME (spec): The spec doesn't say what to do with !important.
  // We'll just map them.
  if (mDeclaration->HasImportantData()) {
    mDeclaration->MapImportantRuleInfoInto(aRuleData);
  }
  mDeclaration->MapNormalRuleInfoInto(aRuleData);
}

#ifdef DEBUG
void
nsCSSKeyframeRule::List(FILE* out, PRInt32 aIndent) const
{
  // FIXME: WRITE ME
}
#endif

/* virtual */ PRInt32
nsCSSKeyframeRule::GetType() const
{
  return Rule::KEYFRAME_RULE;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::MOZ_KEYFRAME_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::GetCssText(nsAString& aCssText)
{
  nsCSSKeyframeRule::GetKeyText(aCssText);
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
  aKeyText.Truncate();
  PRUint32 i = 0, i_end = mKeys.Length();
  NS_ABORT_IF_FALSE(i_end != 0, "must have some keys");
  for (;;) {
    aKeyText.AppendFloat(mKeys[i] * 100.0f);
    aKeyText.Append(PRUnichar('%'));
    if (++i == i_end) {
      break;
    }
    aKeyText.AppendLiteral(", ");
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframeRule::SetKeyText(const nsAString& aKeyText)
{
  nsCSSParser parser;

  InfallibleTArray<float> newSelectors;
  // FIXME: pass filename and line number
  if (parser.ParseKeyframeSelectorString(aKeyText, nsnull, 0, newSelectors)) {
    newSelectors.SwapElements(mKeys);
  } else {
    // for now, we don't do anything if the parse fails
  }

  if (mSheet) {
    mSheet->SetModifiedByChildRule();
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

  if (mSheet) {
    mSheet->SetModifiedByChildRule();
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
NS_INTERFACE_MAP_END

#ifdef DEBUG
void
nsCSSKeyframesRule::List(FILE* out, PRInt32 aIndent) const
{
  // FIXME: WRITE ME
}
#endif

/* virtual */ PRInt32
nsCSSKeyframesRule::GetType() const
{
  return Rule::KEYFRAMES_RULE;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::MOZ_KEYFRAMES_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@-moz-keyframes ");
  aCssText.Append(mName);
  aCssText.AppendLiteral(" {\n");
  nsAutoString tmp;
  for (PRUint32 i = 0, i_end = mRules.Count(); i != i_end; ++i) {
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

  if (mSheet) {
    mSheet->SetModifiedByChildRule();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return GroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
nsCSSKeyframesRule::InsertRule(const nsAString& aRule)
{
  // The spec is confusing, and I think we should just append the rule,
  // which also turns out to match WebKit:
  // http://lists.w3.org/Archives/Public/www-style/2011Apr/0034.html
  nsCSSParser parser;

  // FIXME: pass filename and line number
  nsRefPtr<nsCSSKeyframeRule> rule =
    parser.ParseKeyframeRule(aRule, nsnull, 0);
  if (rule) {
    AppendStyleRule(rule);
  }

  return NS_OK;
}

static const PRUint32 RULE_NOT_FOUND = PRUint32(-1);

PRUint32
nsCSSKeyframesRule::FindRuleIndexForKey(const nsAString& aKey)
{
  nsCSSParser parser;

  InfallibleTArray<float> keys;
  // FIXME: pass filename and line number
  if (parser.ParseKeyframeSelectorString(aKey, nsnull, 0, keys)) {
    // The spec isn't clear, but we'll match on the key list, which
    // mostly matches what WebKit does, except we'll do last-match
    // instead of first-match, and handling parsing differences better.
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0036.html
    // http://lists.w3.org/Archives/Public/www-style/2011Apr/0037.html
    for (PRUint32 i = mRules.Count(); i-- != 0; ) {
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
  PRUint32 index = FindRuleIndexForKey(aKey);
  if (index != RULE_NOT_FOUND) {
    mRules.RemoveObjectAt(index);
    if (mSheet) {
      mSheet->SetModifiedByChildRule();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSKeyframesRule::FindRule(const nsAString& aKey,
                             nsIDOMMozCSSKeyframeRule** aResult)
{
  PRUint32 index = FindRuleIndexForKey(aKey);
  if (index == RULE_NOT_FOUND) {
    *aResult = nsnull;
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



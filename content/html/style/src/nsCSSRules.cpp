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
#include "nsCSSRules.h"
#include "nsICSSImportRule.h"
#include "nsICSSNameSpaceRule.h"

#include "nsString.h"
#include "nsIAtom.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"

#include "nsCSSRule.h"
#include "nsLayoutAtoms.h"
#include "nsICSSStyleSheet.h"

#include "nsCOMPtr.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRule.h"
#include "nsIDOMCSSImportRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSCharsetRule.h"
#include "nsIMediaList.h"
#include "nsIDOMMediaList.h"
#include "nsIDOMCSSRuleList.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDocument.h"
#include "nsPresContext.h"

#include "nsContentUtils.h"
#include "nsStyleConsts.h"
#include "nsDOMError.h"

#define IMPL_STYLE_RULE_INHERIT(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::SetStyleSheet(nsICSSStyleSheet* aSheet) { return super::SetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::SetParentRule(nsICSSGroupRule* aRule) { return super::SetParentRule(aRule); }  \
NS_IMETHODIMP _class::GetDOMRule(nsIDOMCSSRule** aDOMRule) { return CallQueryInterface(this, aDOMRule); }  \
NS_IMETHODIMP _class::MapRuleInfoInto(nsRuleData* aRuleData) { return NS_OK; } 

#define IMPL_STYLE_RULE_INHERIT2(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::SetParentRule(nsICSSGroupRule* aRule) { return super::SetParentRule(aRule); }  \
NS_IMETHODIMP _class::GetDOMRule(nsIDOMCSSRule** aDOMRule) { return CallQueryInterface(this, aDOMRule); }  \
NS_IMETHODIMP _class::MapRuleInfoInto(nsRuleData* aRuleData) { return NS_OK; } 

// -------------------------------
// Style Rule List for group rules
//
class CSSGroupRuleRuleListImpl : public nsIDOMCSSRuleList
{
public:
  CSSGroupRuleRuleListImpl(nsICSSGroupRule *aGroupRule);

  NS_DECL_ISUPPORTS

  NS_DECL_NSIDOMCSSRULELIST

  void DropReference() { mGroupRule = nsnull; }

protected:
  virtual ~CSSGroupRuleRuleListImpl(void);

private:
  nsICSSGroupRule* mGroupRule;
};

CSSGroupRuleRuleListImpl::CSSGroupRuleRuleListImpl(nsICSSGroupRule *aGroupRule)
{
  // Not reference counted to avoid circular references.
  // The rule will tell us when its going away.
  mGroupRule = aGroupRule;
}

CSSGroupRuleRuleListImpl::~CSSGroupRuleRuleListImpl()
{
}

// QueryInterface implementation for CSSGroupRuleRuleList
NS_INTERFACE_MAP_BEGIN(CSSGroupRuleRuleListImpl)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRuleList)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSGroupRuleRuleList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(CSSGroupRuleRuleListImpl)
NS_IMPL_RELEASE(CSSGroupRuleRuleListImpl)

NS_IMETHODIMP    
CSSGroupRuleRuleListImpl::GetLength(PRUint32* aLength)
{
  if (mGroupRule) {
    PRInt32 count;
    mGroupRule->StyleRuleCount(count);
    *aLength = (PRUint32)count;
  } else {
    *aLength = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP    
CSSGroupRuleRuleListImpl::Item(PRUint32 aIndex, nsIDOMCSSRule** aReturn)
{
  nsresult result = NS_OK;

  *aReturn = nsnull;
  if (mGroupRule) {
    nsCOMPtr<nsICSSRule> rule;

    result = mGroupRule->GetStyleRuleAt(aIndex, *getter_AddRefs(rule));
    if (rule) {
      result = rule->GetDOMRule(aReturn);
    } else if (result == NS_ERROR_ILLEGAL_VALUE) {
      result = NS_OK; // per spec: "Return Value ... null if ... not a valid index."
    }
  }
  
  return result;
}

// -------------------------------------------
// CharsetRule
//
class CSSCharsetRuleImpl : public nsCSSRule,
                           public nsICSSRule,
                           public nsIDOMCSSCharsetRule
{
public:
  CSSCharsetRuleImpl(const nsAString& aEncoding);
  CSSCharsetRuleImpl(const CSSCharsetRuleImpl& aCopy);
  virtual ~CSSCharsetRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE
  
  // nsIDOMCSSCharsetRule methods
  NS_IMETHOD GetEncoding(nsAString& aEncoding);
  NS_IMETHOD SetEncoding(const nsAString& aEncoding);

protected:
  nsString  mEncoding;
};

CSSCharsetRuleImpl::CSSCharsetRuleImpl(const nsAString& aEncoding)
  : nsCSSRule(),
    mEncoding(aEncoding)
{
}

CSSCharsetRuleImpl::CSSCharsetRuleImpl(const CSSCharsetRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mEncoding(aCopy.mEncoding)
{
}

CSSCharsetRuleImpl::~CSSCharsetRuleImpl(void)
{
}

NS_IMPL_ADDREF_INHERITED(CSSCharsetRuleImpl, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(CSSCharsetRuleImpl, nsCSSRule)

// QueryInterface implementation for CSSCharsetRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSCharsetRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSCharsetRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSCharsetRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CSSCharsetRuleImpl, nsCSSRule)

#ifdef DEBUG
NS_IMETHODIMP
CSSCharsetRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@charset \"", out);
  fputs(NS_LossyConvertUCS2toASCII(mEncoding).get(), out);
  fputs("\"\n", out);

  return NS_OK;
}
#endif

NS_IMETHODIMP
CSSCharsetRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::CHARSET_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSCharsetRuleImpl* clone = new CSSCharsetRuleImpl(*this);
  if (clone) {
    return CallQueryInterface(clone, &aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetEncoding(nsAString& aEncoding)
{
  aEncoding = mEncoding;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::SetEncoding(const nsAString& aEncoding)
{
  mEncoding = aEncoding;
  return NS_OK;
}


nsresult
NS_NewCSSCharsetRule(nsICSSRule** aInstancePtrResult, const nsAString& aEncoding)
{
  if (! aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSCharsetRuleImpl* it = new CSSCharsetRuleImpl(aEncoding);

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return CallQueryInterface(it, aInstancePtrResult);
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::CHARSET_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@charset \"");
  aCssText.Append(mEncoding);
  aCssText.AppendLiteral("\";");
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);

  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    return mParentRule->GetDOMRule(aParentRule);
  }
  *aParentRule = nsnull;
  return NS_OK;
}



// -------------------------------------------
// nsICSSImportRule
//
class CSSImportRuleImpl : public nsCSSRule,
                          public nsICSSImportRule,
                          public nsIDOMCSSImportRule
{
public:
  CSSImportRuleImpl(nsISupportsArray* aMedia);
  CSSImportRuleImpl(const CSSImportRuleImpl& aCopy);
  virtual ~CSSImportRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSImportRule methods
  NS_IMETHOD SetURLSpec(const nsString& aURLSpec);
  NS_IMETHOD GetURLSpec(nsString& aURLSpec) const;

  NS_IMETHOD SetMedia(const nsString& aMedia);
  NS_IMETHOD GetMedia(nsString& aMedia) const;

  NS_IMETHOD SetSheet(nsICSSStyleSheet*);
  
  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSImportRule interface
  NS_DECL_NSIDOMCSSIMPORTRULE

protected:
  nsString  mURLSpec;
  nsCOMPtr<nsIMediaList> mMedia;
  nsCOMPtr<nsICSSStyleSheet> mChildSheet;
};

CSSImportRuleImpl::CSSImportRuleImpl(nsISupportsArray* aMedia)
  : nsCSSRule(),
    mURLSpec()
{
  // XXXbz This is really silly.... the mMedia we create here will be
  // clobbered if we manage to load a sheet.  Which should really
  // never fail nowadays, in sane cases.
  NS_NewMediaList(aMedia, nsnull, getter_AddRefs(mMedia));
}

CSSImportRuleImpl::CSSImportRuleImpl(const CSSImportRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mURLSpec(aCopy.mURLSpec)
{
  
  nsCOMPtr<nsICSSStyleSheet> sheet;
  if (aCopy.mChildSheet) {
    aCopy.mChildSheet->Clone(nsnull, this, nsnull, nsnull,
                             getter_AddRefs(sheet));
  }
  SetSheet(sheet);
}

CSSImportRuleImpl::~CSSImportRuleImpl(void)
{
  if (mChildSheet) {
    mChildSheet->SetOwnerRule(nsnull);
  }
}

NS_IMPL_ADDREF_INHERITED(CSSImportRuleImpl, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(CSSImportRuleImpl, nsCSSRule)

// QueryInterface implementation for CSSImportRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSImportRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSImportRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSImportRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSImportRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSImportRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CSSImportRuleImpl, nsCSSRule)

#ifdef DEBUG
NS_IMETHODIMP
CSSImportRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@import \"", out);
  fputs(NS_LossyConvertUCS2toASCII(mURLSpec).get(), out);
  fputs("\" ", out);

  nsAutoString mediaText;
  mMedia->GetText(mediaText);
  fputs(NS_LossyConvertUCS2toASCII(mediaText).get(), out);
  fputs("\n", out);

  return NS_OK;
}
#endif

NS_IMETHODIMP
CSSImportRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::IMPORT_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSImportRuleImpl* clone = new CSSImportRuleImpl(*this);
  if (clone) {
    return CallQueryInterface(clone, &aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
CSSImportRuleImpl::SetURLSpec(const nsString& aURLSpec)
{
  mURLSpec = aURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetURLSpec(nsString& aURLSpec) const
{
  aURLSpec = mURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::SetMedia(const nsString& aMedia)
{
  if (mMedia) {
    return mMedia->SetText(aMedia);
  } else {
    return NS_OK;
  }
}

NS_IMETHODIMP
CSSImportRuleImpl::GetMedia(nsString& aMedia) const
{
  if (mMedia) {
    return mMedia->GetText(aMedia);
  } else {
    aMedia.Truncate();
    return NS_OK;
  }
}

NS_IMETHODIMP
CSSImportRuleImpl::SetSheet(nsICSSStyleSheet* aSheet)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aSheet);
  
  // set the new sheet
  mChildSheet = aSheet;
  aSheet->SetOwnerRule(this);

  // set our medialist to be the same as the sheet's medialist
  nsCOMPtr<nsIDOMStyleSheet> sheet(do_QueryInterface(mChildSheet, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIDOMMediaList> mediaList;
  rv = sheet->GetMedia(getter_AddRefs(mediaList));
  NS_ENSURE_SUCCESS(rv, rv);
  mMedia = do_QueryInterface(mediaList);
  
  return NS_OK;
}

nsresult
NS_NewCSSImportRule(nsICSSImportRule** aInstancePtrResult, 
                    const nsString& aURLSpec,
                    nsISupportsArray* aMedia)
{
  NS_ENSURE_ARG_POINTER(aInstancePtrResult);

  CSSImportRuleImpl* it = new CSSImportRuleImpl(aMedia);

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetURLSpec(aURLSpec);
  return CallQueryInterface(it, aInstancePtrResult);
}

NS_IMETHODIMP
CSSImportRuleImpl::GetType(PRUint16* aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = nsIDOMCSSRule::IMPORT_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@import url(");
  aCssText.Append(mURLSpec);
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
CSSImportRuleImpl::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  NS_ENSURE_ARG_POINTER(aSheet);
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    return mParentRule->GetDOMRule(aParentRule);
  }
  *aParentRule = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetHref(nsAString & aHref)
{
  aHref = mURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetMedia(nsIDOMMediaList * *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  if (!mMedia) {
    *aMedia = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(mMedia, aMedia);
}

NS_IMETHODIMP
CSSImportRuleImpl::GetStyleSheet(nsIDOMCSSStyleSheet * *aStyleSheet)
{
  NS_ENSURE_ARG_POINTER(aStyleSheet);
  if (!mChildSheet) {
    *aStyleSheet = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(mChildSheet, aStyleSheet);
}

nsCSSGroupRule::nsCSSGroupRule()
  : nsCSSRule()
  , mRuleCollection(nsnull)
{
}

static PRBool
CloneRuleInto(nsISupports* aRule, void* aArray)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  nsICSSRule* clone = nsnull;
  rule->Clone(clone);
  if (clone) {
    nsISupportsArray* array = (nsISupportsArray*)aArray;
    array->AppendElement(clone);
    NS_RELEASE(clone);
  }
  return PR_TRUE;
}

static PRBool
SetParentRuleReference(nsISupports* aRule, void* aParentRule)
{
  nsICSSRule* rule = NS_STATIC_CAST(nsICSSRule*, aRule);
  nsCSSGroupRule* parentRule = NS_STATIC_CAST(nsCSSGroupRule*, aParentRule);
  rule->SetParentRule(parentRule);
  return PR_TRUE;
}

nsCSSGroupRule::nsCSSGroupRule(const nsCSSGroupRule& aCopy)
  : nsCSSRule(aCopy)
  , mRuleCollection(nsnull) // lazily constructed
{
  if (aCopy.mRules) {
    NS_NewISupportsArray(getter_AddRefs(mRules));
    if (mRules) {
      aCopy.mRules->EnumerateForwards(CloneRuleInto, mRules);
      mRules->EnumerateForwards(SetParentRuleReference, this);
    }
  }
}

nsCSSGroupRule::~nsCSSGroupRule()
{
  if (mRules) {
    mRules->EnumerateForwards(SetParentRuleReference, nsnull);
  }
  if (mRuleCollection) {
    mRuleCollection->DropReference();
    NS_RELEASE(mRuleCollection);
  }
}

IMPL_STYLE_RULE_INHERIT2(nsCSSGroupRule, nsCSSRule)

static PRBool
SetStyleSheetReference(nsISupports* aRule, void* aSheet)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)aSheet;
  rule->SetStyleSheet(sheet);
  return PR_TRUE;
}

NS_IMETHODIMP
nsCSSGroupRule::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  if (mRules) {
    mRules->EnumerateForwards(SetStyleSheetReference, aSheet);
  }

  return nsCSSRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
nsresult
nsCSSGroupRule::List(FILE* out, PRInt32 aIndent) const
{
  fputs(" {\n", out);

  if (mRules) {
    PRUint32 index = 0;
    PRUint32 count;
    mRules->Count(&count);
    while (index < count) {
      nsCOMPtr<nsICSSRule> rule = dont_AddRef((nsICSSRule*)mRules->ElementAt(index++));
      rule->List(out, aIndent + 1);
    }
  }
  fputs("}\n", out);
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsCSSGroupRule::AppendStyleRule(nsICSSRule* aRule)
{
  nsresult result = NS_OK;
  if (!mRules) {
    result = NS_NewISupportsArray(getter_AddRefs(mRules));
  }
  if (NS_SUCCEEDED(result) && mRules) {
    mRules->AppendElement(aRule);
    aRule->SetStyleSheet(mSheet);
    aRule->SetParentRule(this);
    if (mSheet) {
      // XXXldb Shouldn't we be using |WillDirty| and |DidDirty| (and
      // shouldn't |SetModified| be removed?
      mSheet->SetModified(PR_TRUE);
    }
  }
  return result;
}

NS_IMETHODIMP
nsCSSGroupRule::StyleRuleCount(PRInt32& aCount) const
{
  if (mRules) {
    PRUint32 count;
    mRules->Count(&count);
    aCount = (PRInt32)count;
  }
  else {
    aCount = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSGroupRule::GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const
{
  if (mRules) {
    PRInt32 count = 0;
    nsresult rv = StyleRuleCount(count);
    NS_ENSURE_SUCCESS(rv, rv);
    if (aIndex >= count) {
      aRule = nsnull;
      return NS_ERROR_ILLEGAL_VALUE;
    }      
    aRule = (nsICSSRule*)mRules->ElementAt(aIndex);
    return NS_OK;
  }
  return NS_ERROR_ILLEGAL_VALUE;
}

NS_IMETHODIMP
nsCSSGroupRule::EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const
{
  if (mRules) {
    return ((mRules->EnumerateForwards(aFunc, aData)) ? NS_OK : NS_ENUMERATOR_FALSE);
  }
  return NS_OK;
}

/*
 * The next two methods (DeleteStyleRuleAt and InsertStyleRulesAt)
 * should never be called unless you have first called WillDirty() on
 * the parents tylesheet.  After they are called, DidDirty() needs to
 * be called on the sheet
 */
NS_IMETHODIMP
nsCSSGroupRule::DeleteStyleRuleAt(PRUint32 aIndex)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_FAILURE);

  nsCOMPtr<nsICSSRule> rule = dont_AddRef((nsICSSRule*)mRules->ElementAt(aIndex));
  if (rule) {
    rule->SetStyleSheet(nsnull);
    rule->SetParentRule(nsnull);
  }
  return mRules->DeleteElementAt(aIndex);
}

NS_IMETHODIMP
nsCSSGroupRule::InsertStyleRulesAt(PRUint32 aIndex, nsISupportsArray* aRules)
{
  NS_ENSURE_TRUE(mRules, NS_ERROR_FAILURE);

  aRules->EnumerateForwards(SetStyleSheetReference, mSheet);
  aRules->EnumerateForwards(SetParentRuleReference, this);
  // There is no xpcom-compatible version of InsertElementsAt.... :(
  if (! mRules->InsertElementsAt(aRules, aIndex)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsCSSGroupRule::ReplaceStyleRule(nsICSSRule* aOld, nsICSSRule* aNew)
{
  PRInt32 index = mRules->IndexOf(aOld);
  NS_ENSURE_TRUE(index != -1, NS_ERROR_UNEXPECTED);
  mRules->ReplaceElementAt(aNew, index);
  aNew->SetStyleSheet(mSheet);
  aNew->SetParentRule(this);
  aOld->SetStyleSheet(nsnull);
  aOld->SetParentRule(nsnull);
  return NS_OK;
}

nsresult
nsCSSGroupRule::AppendRulesToCssText(nsAString& aCssText)
{
  aCssText.AppendLiteral(" {\n");

  // get all the rules
  if (mRules) {
    PRUint32 count;
    mRules->Count(&count);
    for (PRUint32 index = 0; index < count; index++) {
      nsCOMPtr<nsICSSRule> rule;
      mRules->GetElementAt(index, getter_AddRefs(rule));
      nsCOMPtr<nsIDOMCSSRule> domRule;
      rule->GetDOMRule(getter_AddRefs(domRule));
      if (domRule) {
        nsAutoString cssText;
        domRule->GetCssText(cssText);
        aCssText.Append(NS_LITERAL_STRING("  ") +
                        cssText +
                        NS_LITERAL_STRING("\n"));
      }
    }
  }

  aCssText.AppendLiteral("}");
  
  return NS_OK;
}

nsresult
nsCSSGroupRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

nsresult
nsCSSGroupRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    return mParentRule->GetDOMRule(aParentRule);
  }
  *aParentRule = nsnull;
  return NS_OK;
}

// nsIDOMCSSMediaRule or nsIDOMCSSMozDocumentRule methods
nsresult
nsCSSGroupRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  if (!mRuleCollection) {
    mRuleCollection = new CSSGroupRuleRuleListImpl(this);
    if (!mRuleCollection) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(mRuleCollection);
  }

  return CallQueryInterface(mRuleCollection, aRuleList);
}

nsresult
nsCSSGroupRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  NS_ENSURE_TRUE(mSheet, NS_ERROR_FAILURE);
  
  if (!mRules) {
    nsresult rv = NS_NewISupportsArray(getter_AddRefs(mRules));
    if (NS_FAILED(rv))
      return rv;
  }

  PRUint32 count;
  mRules->Count(&count);
  if (aIndex > count)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  
  return mSheet->InsertRuleIntoGroup(aRule, this, aIndex, _retval);
}

nsresult
nsCSSGroupRule::DeleteRule(PRUint32 aIndex)
{
  NS_ENSURE_TRUE(mSheet, NS_ERROR_FAILURE);
  if (!mRules)  {
    return NS_ERROR_DOM_INDEX_SIZE_ERR;
  }
  
  PRUint32 count;
  mRules->Count(&count);
  if (aIndex >= count)
    return NS_ERROR_DOM_INDEX_SIZE_ERR;

  return mSheet->DeleteRuleFromGroup(this, aIndex);
}

// -------------------------------------------
// nsICSSMediaRule
//
nsCSSMediaRule::nsCSSMediaRule()
{
}

nsCSSMediaRule::nsCSSMediaRule(const nsCSSMediaRule& aCopy)
  : nsCSSGroupRule(aCopy)
{
  if (aCopy.mMedia) {
    NS_NewMediaList(aCopy.mMedia, aCopy.mSheet, getter_AddRefs(mMedia));
  }
}

nsCSSMediaRule::~nsCSSMediaRule()
{
  if (mMedia) {
    mMedia->DropReference();
  }
}

NS_IMPL_ADDREF_INHERITED(nsCSSMediaRule, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(nsCSSMediaRule, nsCSSRule)

// QueryInterface implementation for nsCSSMediaRule
NS_INTERFACE_MAP_BEGIN(nsCSSMediaRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSGroupRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMediaRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsCSSGroupRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSMediaRule)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsCSSMediaRule::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  if (mMedia) {
    nsresult rv;
    nsCOMPtr<nsISupportsArray> oldMedia(do_QueryInterface(mMedia, &rv));
    if (NS_FAILED(rv))
      return rv;
    mMedia->DropReference();
    rv = NS_NewMediaList(oldMedia, aSheet, getter_AddRefs(mMedia));
    if (NS_FAILED(rv))
      return rv;
  }

  return nsCSSGroupRule::SetStyleSheet(aSheet);
}

#ifdef DEBUG
NS_IMETHODIMP
nsCSSMediaRule::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@media ", out);

  if (mMedia) {
    PRUint32 index = 0;
    PRUint32 count;
    mMedia->Count(&count);
    while (index < count) {
      nsCOMPtr<nsIAtom> medium = dont_AddRef((nsIAtom*)mMedia->ElementAt(index++));
      medium->ToString(buffer);
      fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
      if (index < count) {
        fputs(", ", out);
      }
    }
  }

  return nsCSSGroupRule::List(out, aIndent);
}
#endif

NS_IMETHODIMP
nsCSSMediaRule::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSMediaRule::Clone(nsICSSRule*& aClone) const
{
  nsCSSMediaRule* clone = new nsCSSMediaRule(*this);
  if (clone) {
    return CallQueryInterface(clone, &aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

nsresult
nsCSSMediaRule::SetMedia(nsISupportsArray* aMedia)
{
  return NS_NewMediaList(aMedia, mSheet, getter_AddRefs(mMedia));
}

// nsIDOMCSSRule methods
NS_IMETHODIMP
nsCSSMediaRule::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSMediaRule::GetCssText(nsAString& aCssText)
{
  PRUint32 index;
  PRUint32 count;
  aCssText.AssignLiteral("@media ");
  // get all the media
  if (mMedia) {
    mMedia->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIAtom> medium = dont_AddRef((nsIAtom*)mMedia->ElementAt(index));
      if (medium) {
        nsAutoString tempString;
        if (index > 0)
          aCssText.AppendLiteral(", ");
        medium->ToString(tempString);
        aCssText.Append(tempString);
      }
    }
  }

  return nsCSSGroupRule::AppendRulesToCssText(aCssText);
}

NS_IMETHODIMP
nsCSSMediaRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSMediaRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return nsCSSGroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSMediaRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return nsCSSGroupRule::GetParentRule(aParentRule);
}

// nsIDOMCSSMediaRule methods
NS_IMETHODIMP
nsCSSMediaRule::GetMedia(nsIDOMMediaList* *aMedia)
{
  NS_ENSURE_ARG_POINTER(aMedia);
  if (!mMedia) {
    *aMedia = nsnull;
    return NS_OK;
  }

  return CallQueryInterface(mMedia, aMedia);
}

NS_IMETHODIMP
nsCSSMediaRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return nsCSSGroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
nsCSSMediaRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  return nsCSSGroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
nsCSSMediaRule::DeleteRule(PRUint32 aIndex)
{
  return nsCSSGroupRule::DeleteRule(aIndex);
}

// nsICSSGroupRule interface
NS_IMETHODIMP_(PRBool)
nsCSSMediaRule::UseForPresentation(nsPresContext* aPresContext)
{
  if (mMedia) {
    PRBool matches = PR_FALSE;
    mMedia->MatchesMedium(aPresContext->Medium(), &matches);
    return matches;
  }
  return PR_TRUE;
}


nsCSSDocumentRule::nsCSSDocumentRule(void)
{
}

nsCSSDocumentRule::nsCSSDocumentRule(const nsCSSDocumentRule& aCopy)
  : nsCSSGroupRule(aCopy)
  , mURLs(new URL(*aCopy.mURLs))
{
}

nsCSSDocumentRule::~nsCSSDocumentRule(void)
{
}

NS_IMPL_ADDREF_INHERITED(nsCSSDocumentRule, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(nsCSSDocumentRule, nsCSSRule)

// QueryInterface implementation for nsCSSDocumentRule
NS_INTERFACE_MAP_BEGIN(nsCSSDocumentRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSGroupRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSMozDocumentRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsCSSGroupRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSMozDocumentRule)
NS_INTERFACE_MAP_END

#ifdef DEBUG
NS_IMETHODIMP
nsCSSDocumentRule::List(FILE* out, PRInt32 aIndent) const
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
    }
    nsCAutoString escapedURL(url->url);
    escapedURL.ReplaceSubstring("\"", "\\\""); // escape quotes
    str.Append(escapedURL);
    str.AppendLiteral("\"), ");
  }
  str.Cut(str.Length() - 2, 1); // remove last ,
  fputs(str.get(), out);

  return nsCSSGroupRule::List(out, aIndent);
}
#endif

NS_IMETHODIMP
nsCSSDocumentRule::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::DOCUMENT_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSDocumentRule::Clone(nsICSSRule*& aClone) const
{
  nsCSSDocumentRule* clone = new nsCSSDocumentRule(*this);
  if (clone) {
    return CallQueryInterface(clone, &aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

// nsIDOMCSSRule methods
NS_IMETHODIMP
nsCSSDocumentRule::GetType(PRUint16* aType)
{
  // XXX What should really happen here?
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
  return NS_OK;
}

NS_IMETHODIMP
nsCSSDocumentRule::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@-moz-document ");
  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL:
        aCssText.AppendLiteral("url(\"");
        break;
      case eURLPrefix:
        aCssText.AppendLiteral("url-prefix(\"");
        break;
      case eDomain:
        aCssText.AppendLiteral("domain(\"");
        break;
    }
    nsCAutoString escapedURL(url->url);
    escapedURL.ReplaceSubstring("\"", "\\\""); // escape quotes
    AppendUTF8toUTF16(escapedURL, aCssText);
    aCssText.AppendLiteral("\"), ");
  }
  aCssText.Cut(aCssText.Length() - 2, 1); // remove last ,

  return nsCSSGroupRule::AppendRulesToCssText(aCssText);
}

NS_IMETHODIMP
nsCSSDocumentRule::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCSSDocumentRule::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  return nsCSSGroupRule::GetParentStyleSheet(aSheet);
}

NS_IMETHODIMP
nsCSSDocumentRule::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return nsCSSGroupRule::GetParentRule(aParentRule);
}

NS_IMETHODIMP
nsCSSDocumentRule::GetCssRules(nsIDOMCSSRuleList* *aRuleList)
{
  return nsCSSGroupRule::GetCssRules(aRuleList);
}

NS_IMETHODIMP
nsCSSDocumentRule::InsertRule(const nsAString & aRule, PRUint32 aIndex, PRUint32* _retval)
{
  return nsCSSGroupRule::InsertRule(aRule, aIndex, _retval);
}

NS_IMETHODIMP
nsCSSDocumentRule::DeleteRule(PRUint32 aIndex)
{
  return nsCSSGroupRule::DeleteRule(aIndex);
}

// nsICSSGroupRule interface
NS_IMETHODIMP_(PRBool)
nsCSSDocumentRule::UseForPresentation(nsPresContext* aPresContext)
{
  nsIURI *docURI = aPresContext->GetDocument()->GetDocumentURI();
  nsCAutoString docURISpec;
  if (docURI)
    docURI->GetSpec(docURISpec);

  for (URL *url = mURLs; url; url = url->next) {
    switch (url->func) {
      case eURL: {
        if (docURISpec == url->url)
          return PR_TRUE;
      } break;
      case eURLPrefix: {
        if (StringBeginsWith(docURISpec, url->url))
          return PR_TRUE;
      } break;
      case eDomain: {
        nsCAutoString host;
        if (docURI)
          docURI->GetHost(host);
        PRInt32 lenDiff = host.Length() - url->url.Length();
        if (lenDiff == 0) {
          if (host == url->url)
            return PR_TRUE;
        } else {
          if (StringEndsWith(host, url->url) &&
              host.CharAt(lenDiff - 1) == '.')
            return PR_TRUE;
        }
      } break;
    }
  }

  return PR_FALSE;
}


// -------------------------------------------
// nsICSSNameSpaceRule
//
class CSSNameSpaceRuleImpl : public nsCSSRule,
                             public nsICSSNameSpaceRule,
                             public nsIDOMCSSRule
{
public:
  CSSNameSpaceRuleImpl(void);
  CSSNameSpaceRuleImpl(const CSSNameSpaceRuleImpl& aCopy);
  virtual ~CSSNameSpaceRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
#ifdef DEBUG
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSNameSpaceRule methods
  NS_IMETHOD GetPrefix(nsIAtom*& aPrefix) const;
  NS_IMETHOD SetPrefix(nsIAtom* aPrefix);

  NS_IMETHOD GetURLSpec(nsString& aURLSpec) const;
  NS_IMETHOD SetURLSpec(const nsString& aURLSpec);

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE
  
protected:
  nsIAtom*  mPrefix;
  nsString  mURLSpec;
};

CSSNameSpaceRuleImpl::CSSNameSpaceRuleImpl(void)
  : nsCSSRule(),
    mPrefix(nsnull),
    mURLSpec()
{
}

CSSNameSpaceRuleImpl::CSSNameSpaceRuleImpl(const CSSNameSpaceRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mPrefix(aCopy.mPrefix),
    mURLSpec(aCopy.mURLSpec)
{
  NS_IF_ADDREF(mPrefix);
}

CSSNameSpaceRuleImpl::~CSSNameSpaceRuleImpl(void)
{
  NS_IF_RELEASE(mPrefix);
}

NS_IMPL_ADDREF_INHERITED(CSSNameSpaceRuleImpl, nsCSSRule)
NS_IMPL_RELEASE_INHERITED(CSSNameSpaceRuleImpl, nsCSSRule)

// QueryInterface implementation for CSSNameSpaceRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSNameSpaceRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSNameSpaceRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSNameSpaceRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSNameSpaceRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CSSNameSpaceRuleImpl, nsCSSRule)

#ifdef DEBUG
NS_IMETHODIMP
CSSNameSpaceRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@namespace ", out);

  if (mPrefix) {
    mPrefix->ToString(buffer);
    fputs(NS_LossyConvertUCS2toASCII(buffer).get(), out);
    fputs(" ", out);
  }

  fputs("url(", out);
  fputs(NS_LossyConvertUCS2toASCII(mURLSpec).get(), out);
  fputs(")\n", out);
  return NS_OK;
}
#endif

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::NAMESPACE_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSNameSpaceRuleImpl* clone = new CSSNameSpaceRuleImpl(*this);
  if (clone) {
    return CallQueryInterface(clone, &aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetPrefix(nsIAtom*& aPrefix) const
{
  aPrefix = mPrefix;
  NS_IF_ADDREF(aPrefix);
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::SetPrefix(nsIAtom* aPrefix)
{
  NS_IF_RELEASE(mPrefix);
  mPrefix = aPrefix;
  NS_IF_ADDREF(mPrefix);
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetURLSpec(nsString& aURLSpec) const
{
  aURLSpec = mURLSpec;
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::SetURLSpec(const nsString& aURLSpec)
{
  mURLSpec = aURLSpec;
  return NS_OK;
}

nsresult
NS_NewCSSNameSpaceRule(nsICSSNameSpaceRule** aInstancePtrResult, 
                       nsIAtom* aPrefix, const nsString& aURLSpec)
{
  if (! aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSNameSpaceRuleImpl* it = new CSSNameSpaceRuleImpl();

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetPrefix(aPrefix);
  it->SetURLSpec(aURLSpec);
  return CallQueryInterface(it, aInstancePtrResult);
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetType(PRUint16* aType)
{
  // XXX What should really happen here?
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetCssText(nsAString& aCssText)
{
  aCssText.AssignLiteral("@namespace ");
  if (mPrefix) {
    nsString atomStr;
    mPrefix->ToString(atomStr);
    aCssText.Append(atomStr);
    aCssText.AppendLiteral(" ");
  }
  aCssText.AppendLiteral("url(");
  aCssText.Append(mURLSpec);
  aCssText.Append(NS_LITERAL_STRING(");"));
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::SetCssText(const nsAString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  if (mParentRule) {
    return mParentRule->GetDOMRule(aParentRule);
  }
  *aParentRule = nsnull;
  return NS_OK;
}


/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsICSSCharsetRule.h"
#include "nsICSSImportRule.h"
#include "nsICSSMediaRule.h"
#include "nsICSSNameSpaceRule.h"

#include "nsString.h"
#include "nsIAtom.h"
#include "nsISupportsArray.h"
#include "nsIURL.h"

#include "nsCSSRule.h"
#include "nsLayoutAtoms.h"
#include "nsICSSStyleSheet.h"

#include "nsCOMPtr.h"
#include "nsIStyleSet.h"
#include "nsISizeOfHandler.h"
#include "nsIDOMCSSStyleSheet.h"
#include "nsIDOMCSSRule.h"

#include "nsContentUtils.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

#define DECL_STYLE_RULE_INHERIT  \
NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const; \
NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet); \
NS_IMETHOD GetStrength(PRInt32& aStrength) const; \
NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

#define IMPL_STYLE_RULE_INHERIT(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::SetStyleSheet(nsICSSStyleSheet* aSheet) { return super::SetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::GetStrength(PRInt32& aStrength) const { return super::GetStrength(aStrength); }   \
NS_IMETHODIMP _class::MapRuleInfoInto(nsRuleData* aRuleData) { return NS_OK; } 

#define IMPL_STYLE_RULE_INHERIT2(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::GetStrength(PRInt32& aStrength) const { return super::GetStrength(aStrength); }   \
NS_IMETHODIMP _class::MapRuleInfoInto(nsRuleData* aRuleData) { return NS_OK; } 

// -------------------------------------------
// nsICSSCharsetRule
//
class CSSCharsetRuleImpl : public nsCSSRule,
                           public nsICSSCharsetRule,
                           public nsIDOMCSSRule
{
public:
  CSSCharsetRuleImpl(void);
  CSSCharsetRuleImpl(const CSSCharsetRuleImpl& aCopy);
  virtual ~CSSCharsetRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD  Init(const nsString& aEncoding);

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSCharsetRule methods
  NS_IMETHOD  GetEncoding(nsString& aEncoding) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE
  
protected:
  nsString  mEncoding;
};

CSSCharsetRuleImpl::CSSCharsetRuleImpl(void)
  : nsCSSRule(),
    mEncoding()
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

NS_IMPL_ADDREF_INHERITED(CSSCharsetRuleImpl, nsCSSRule);
NS_IMPL_RELEASE_INHERITED(CSSCharsetRuleImpl, nsCSSRule);

// QueryInterface implementation for CSSCharsetRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSCharsetRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSCharsetRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSCharsetRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSCharsetRule)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
CSSCharsetRuleImpl::Init(const nsString& aEncoding)
{
  mEncoding = aEncoding;
  return NS_OK;
}

IMPL_STYLE_RULE_INHERIT(CSSCharsetRuleImpl, nsCSSRule);

NS_IMETHODIMP
CSSCharsetRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@charset \"", out);
  fputs(mEncoding, out);
  fputs("\"\n", out);

  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSCharSetRuleImpl's size): 
*    1) sizeof(*this) + the size of the mEncoding string
*
*  Contained / Aggregated data (not reported as CSSCharsetRuleImpl's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSCharsetRuleImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSCharsetRuleImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  // add the string for encoding value
  mEncoding.SizeOf(aSizeOfHandler, &localSize);
  aSize += localSize;
  aSize -= sizeof(mEncoding); // counted in sizeof(*this) and nsString->SizeOf()
  aSizeOfHandler->AddSize(tag,aSize);
}

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
    return clone->QueryInterface(NS_GET_IID(nsICSSRule), (void **)&aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetEncoding(nsString& aEncoding) const
{
  aEncoding = mEncoding;
  return NS_OK;
}


NS_HTML nsresult
NS_NewCSSCharsetRule(nsICSSCharsetRule** aInstancePtrResult, const nsString& aEncoding)
{
  if (! aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSCharsetRuleImpl* it = new CSSCharsetRuleImpl();

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->Init(aEncoding);
  return it->QueryInterface(NS_GET_IID(nsICSSCharsetRule), (void **) aInstancePtrResult);
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::CHARSET_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetCssText(nsAWritableString& aCssText)
{
  aCssText.Assign(NS_LITERAL_STRING("@charset \""));
  aCssText.Append(mEncoding);
  aCssText.Append(NS_LITERAL_STRING("\";"));
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::SetCssText(const nsAReadableString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSCharsetRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}



// -------------------------------------------
// nsICSSImportRule
//
class CSSImportRuleImpl : public nsCSSRule,
                          public nsICSSImportRule,
                          public nsIDOMCSSRule
{
public:
  CSSImportRuleImpl(void);
  CSSImportRuleImpl(const CSSImportRuleImpl& aCopy);
  virtual ~CSSImportRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSImportRule methods
  NS_IMETHOD SetURLSpec(const nsString& aURLSpec);
  NS_IMETHOD GetURLSpec(nsString& aURLSpec) const;

  NS_IMETHOD SetMedia(const nsString& aMedia);
  NS_IMETHOD GetMedia(nsString& aMedia) const;
  
  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

protected:
  nsString  mURLSpec;
  nsString  mMedia;
};

CSSImportRuleImpl::CSSImportRuleImpl(void)
  : nsCSSRule(),
    mURLSpec(),
    mMedia()
{
}

CSSImportRuleImpl::CSSImportRuleImpl(const CSSImportRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mURLSpec(aCopy.mURLSpec),
    mMedia(aCopy.mMedia)
{
}

CSSImportRuleImpl::~CSSImportRuleImpl(void)
{
}

NS_IMPL_ADDREF_INHERITED(CSSImportRuleImpl, nsCSSRule);
NS_IMPL_RELEASE_INHERITED(CSSImportRuleImpl, nsCSSRule);

// QueryInterface implementation for CSSImportRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSImportRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSImportRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSImportRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSImportRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CSSImportRuleImpl, nsCSSRule);

NS_IMETHODIMP
CSSImportRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  // Indent
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  fputs("@import \"", out);
  fputs(mURLSpec, out);
  fputs("\" ", out);

  fputs(mMedia, out);
  fputs("\n", out);

  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSImportRuleImpl's size): 
*    1) sizeof(*this) + the size of the mURLSpec string + 
*       the size of the mMedia string
*
*  Contained / Aggregated data (not reported as CSSImportRuleImpl's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSImportRuleImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSImportRuleImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);

  // add the strings for the URLSpec and the Media
  mURLSpec.SizeOf(aSizeOfHandler, &localSize);
  aSize += localSize;
  aSize -= sizeof(mURLSpec); // counted in sizeof(*this) and nsString->SizeOf()
  mMedia.SizeOf(aSizeOfHandler, &localSize);
  aSize += localSize;
  aSize -= sizeof(mMedia); // counted in sizeof(*this) and nsString->SizeOf()
  aSizeOfHandler->AddSize(tag,aSize);

}

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
    return clone->QueryInterface(NS_GET_IID(nsICSSRule), (void **)&aClone);
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
  mMedia = aMedia;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetMedia(nsString& aMedia) const
{
  aMedia = mMedia;
  return NS_OK;
}

NS_HTML nsresult
NS_NewCSSImportRule(nsICSSImportRule** aInstancePtrResult, 
                    const nsString& aURLSpec,
                    const nsString& aMedia)
{
  if (! aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSImportRuleImpl* it = new CSSImportRuleImpl();

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->SetURLSpec(aURLSpec);
  it->SetMedia(aMedia);
  return it->QueryInterface(NS_GET_IID(nsICSSImportRule), (void **) aInstancePtrResult);
}

NS_IMETHODIMP
CSSImportRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::IMPORT_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetCssText(nsAWritableString& aCssText)
{
  aCssText.Assign(NS_LITERAL_STRING("@import url("));
  aCssText.Append(mURLSpec);
  aCssText.Append(NS_LITERAL_STRING(") "));
  aCssText.Append(mMedia);
  aCssText.Append(NS_LITERAL_STRING(";"));
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::SetCssText(const nsAReadableString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSImportRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// -------------------------------------------
// nsICSSMediaRule
//
class CSSMediaRuleImpl : public nsCSSRule,
                         public nsICSSMediaRule,
                         public nsIDOMCSSRule
{
public:
  CSSMediaRuleImpl(void);
  CSSMediaRuleImpl(const CSSMediaRuleImpl& aCopy);
  virtual ~CSSMediaRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSMediaRule methods
  NS_IMETHOD  SetMedia(nsISupportsArray* aMedia);
  NS_IMETHOD  UseForMedium(nsIAtom* aMedium) const;

  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule);

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;

  NS_IMETHOD  EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

protected:
  nsISupportsArray* mMedia;
  nsISupportsArray* mRules;
};

CSSMediaRuleImpl::CSSMediaRuleImpl(void)
  : nsCSSRule(),
    mMedia(nsnull),
    mRules(nsnull)
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

CSSMediaRuleImpl::CSSMediaRuleImpl(const CSSMediaRuleImpl& aCopy)
  : nsCSSRule(aCopy),
    mMedia(nsnull),
    mRules(nsnull)
{
  if (aCopy.mMedia) {
    NS_NewISupportsArray(&mMedia);
    if (mMedia) {
      mMedia->AppendElements(aCopy.mMedia);
    }
  }

  if (aCopy.mRules) {
    NS_NewISupportsArray(&mRules);
    if (mRules) {
      aCopy.mRules->EnumerateForwards(CloneRuleInto, mRules);
    }
  }
}

CSSMediaRuleImpl::~CSSMediaRuleImpl(void)
{
  NS_IF_RELEASE(mMedia);
  NS_IF_RELEASE(mRules);
}

NS_IMPL_ADDREF_INHERITED(CSSMediaRuleImpl, nsCSSRule);
NS_IMPL_RELEASE_INHERITED(CSSMediaRuleImpl, nsCSSRule);

// QueryInterface implementation for CSSMediaRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSMediaRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSMediaRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSMediaRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSMediaRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT2(CSSMediaRuleImpl, nsCSSRule);

static PRBool
SetStyleSheetReference(nsISupports* aRule, void* aSheet)
{
  nsICSSRule* rule = (nsICSSRule*)aRule;
  nsICSSStyleSheet* sheet = (nsICSSStyleSheet*)aSheet;
  rule->SetStyleSheet(sheet);
  return PR_TRUE;
}

NS_IMETHODIMP
CSSMediaRuleImpl::SetStyleSheet(nsICSSStyleSheet* aSheet)
{
  if (mRules) {
    mRules->EnumerateForwards(SetStyleSheetReference, aSheet);
  }
  return nsCSSRule::SetStyleSheet(aSheet);
}

NS_IMETHODIMP
CSSMediaRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@media ", out);

  if (mMedia) {
    PRUint32 index = 0;
    PRUint32 count;
    mMedia->Count(&count);
    while (index < count) {
      nsIAtom* medium = (nsIAtom*)mMedia->ElementAt(index++);
      medium->ToString(buffer);
      fputs(buffer, out);
      if (index < count) {
        fputs(", ", out);
      }
      NS_RELEASE(medium);
    }
  }
  fputs(" {\n", out);

  if (mRules) {
    PRUint32 index = 0;
    PRUint32 count;
    mRules->Count(&count);
    while (index < count) {
      nsICSSRule* rule = (nsICSSRule*)mRules->ElementAt(index++);
      rule->List(out, aIndent + 1);
      NS_RELEASE(rule);
    }
  }
  fputs("}\n", out);
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSMediaRuleImpl's size): 
*    1) sizeof(*this) + the size of each unique atom in the mMedia collection
*
*  Contained / Aggregated data (not reported as CSSMediaRuleImpl's size):
*    1) Delegate to the rules in the mRules collection to report theri own size
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSMediaRuleImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSMediaRuleImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);

  if (mMedia) {
    // get the sizes of the media atoms (if unique)
    PRUint32 index = 0;
    PRUint32 count;
    mMedia->Count(&count);
    while (index < count) {
      nsIAtom* medium = (nsIAtom*)mMedia->ElementAt(index++);
      if(medium && uniqueItems->AddItem(medium)){
        medium->SizeOf(aSizeOfHandler, &localSize);
        aSize += localSize;
      }
      NS_IF_RELEASE(medium);
    }
  }
  // we are done with the size we report for ourself
  aSizeOfHandler->AddSize(tag,aSize);

  if (mRules) {
    // delegate to the rules themselves (do not sum into our size)
    PRUint32 index = 0;
    PRUint32 count;
    mRules->Count(&count);
    while (index < count) {
      nsICSSRule* rule = (nsICSSRule*)mRules->ElementAt(index++);
      rule->SizeOf(aSizeOfHandler, localSize);
      NS_RELEASE(rule);
    }
  }
}

NS_IMETHODIMP
CSSMediaRuleImpl::GetType(PRInt32& aType) const
{
  aType = nsICSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSMediaRuleImpl::Clone(nsICSSRule*& aClone) const
{
  CSSMediaRuleImpl* clone = new CSSMediaRuleImpl(*this);
  if (clone) {
    return clone->QueryInterface(NS_GET_IID(nsICSSRule), (void **)&aClone);
  }
  aClone = nsnull;
  return NS_ERROR_OUT_OF_MEMORY;
}

// nsICSSMediaRule methods
NS_IMETHODIMP
CSSMediaRuleImpl::SetMedia(nsISupportsArray* aMedia)
{
  nsresult result = NS_OK;
  NS_IF_RELEASE(mMedia);
  mMedia = aMedia;
  NS_IF_ADDREF(mMedia);
  return result;
}

NS_IMETHODIMP
CSSMediaRuleImpl::UseForMedium(nsIAtom* aMedium) const
{
  if (mMedia) {
    if (-1 != mMedia->IndexOf(aMedium)) {
      return NS_OK;
    }
    if (-1 != mMedia->IndexOf(nsLayoutAtoms::all)) {
      return NS_OK;
    }
    return NS_COMFALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP
CSSMediaRuleImpl::AppendStyleRule(nsICSSRule* aRule)
{
  nsresult result = NS_OK;
  if (nsnull == mRules) {
    result = NS_NewISupportsArray(&mRules);
  }
  if (NS_SUCCEEDED(result) && (nsnull != mRules)) {
    mRules->AppendElement(aRule);
    aRule->SetStyleSheet(mSheet);
    if (mSheet) {
      mSheet->SetModified(PR_TRUE);
    }
  }
  return result;
}

NS_IMETHODIMP
CSSMediaRuleImpl::StyleRuleCount(PRInt32& aCount) const
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
CSSMediaRuleImpl::GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const
{
  if (mRules) {
    aRule = (nsICSSRule*)mRules->ElementAt(aIndex);
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}

NS_IMETHODIMP
CSSMediaRuleImpl::EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const
{
  if (mRules) {
    return ((mRules->EnumerateForwards(aFunc, aData)) ? NS_OK : NS_ENUMERATOR_FALSE);
  }
  return NS_OK;
}


nsresult
NS_NewCSSMediaRule(nsICSSMediaRule** aInstancePtrResult)
{
  if (! aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  CSSMediaRuleImpl* it = new CSSMediaRuleImpl();

  if (! it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return it->QueryInterface(NS_GET_IID(nsICSSMediaRule), (void **) aInstancePtrResult);
}

NS_IMETHODIMP
CSSMediaRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::MEDIA_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSMediaRuleImpl::GetCssText(nsAWritableString& aCssText)
{
  PRUint32 index;
  PRUint32 count;
  aCssText.Assign(NS_LITERAL_STRING("@media "));
  // get all the media
  if (mMedia) {
    mMedia->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIAtom> medium (do_QueryInterface(mMedia->ElementAt(index)));
      if (medium) {
        nsAutoString tempString;
        if (index > 0)
          aCssText.Append(NS_LITERAL_STRING(", "));
        medium->ToString(tempString);
        aCssText.Append(tempString);
      }
    }
  }

  aCssText.Append(NS_LITERAL_STRING(" {\n"));

  // get all the rules
  if (mRules) {
    mRules->Count(&count);
    for (index = 0; index < count; index++) {
      nsCOMPtr<nsIDOMCSSRule> rule (do_QueryInterface(mRules->ElementAt(index)));
      if (rule) {
        nsAutoString tempString;
        rule->GetCssText(tempString);
        aCssText.Append(NS_LITERAL_STRING("  "));
        aCssText.Append(tempString);
        aCssText.Append(NS_LITERAL_STRING("\n"));
      }
    }
  }

  aCssText.Append(NS_LITERAL_STRING("}"));
  
  return NS_OK;
}

NS_IMETHODIMP
CSSMediaRuleImpl::SetCssText(const nsAReadableString& aCssText)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CSSMediaRuleImpl::GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet)
{
  if (mSheet) {
    return CallQueryInterface(mSheet, aSheet);
  }
  *aSheet = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
CSSMediaRuleImpl::GetParentRule(nsIDOMCSSRule** aParentRule)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  virtual void SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize);

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

NS_IMPL_ADDREF_INHERITED(CSSNameSpaceRuleImpl, nsCSSRule);
NS_IMPL_RELEASE_INHERITED(CSSNameSpaceRuleImpl, nsCSSRule);

// QueryInterface implementation for CSSNameSpaceRuleImpl
NS_INTERFACE_MAP_BEGIN(CSSNameSpaceRuleImpl)
  NS_INTERFACE_MAP_ENTRY(nsICSSNameSpaceRule)
  NS_INTERFACE_MAP_ENTRY(nsICSSRule)
  NS_INTERFACE_MAP_ENTRY(nsIStyleRule)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCSSRule)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsICSSNameSpaceRule)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CSSNameSpaceRule)
NS_INTERFACE_MAP_END

IMPL_STYLE_RULE_INHERIT(CSSNameSpaceRuleImpl, nsCSSRule);

NS_IMETHODIMP
CSSNameSpaceRuleImpl::List(FILE* out, PRInt32 aIndent) const
{
  for (PRInt32 indent = aIndent; --indent >= 0; ) fputs("  ", out);

  nsAutoString  buffer;

  fputs("@namespace ", out);

  if (mPrefix) {
    mPrefix->ToString(buffer);
    fputs(buffer, out);
    fputs(" ", out);
  }

  fputs("url(", out);
  fputs(mURLSpec, out);
  fputs(")\n", out);
  return NS_OK;
}

/******************************************************************************
* SizeOf method:
*
*  Self (reported as CSSNamespaceRuleImpl's size): 
*    1) sizeof(*this) + the size of the mURLSpec string +
*       the sizeof the mPrefix atom (if it ieists)
*
*  Contained / Aggregated data (not reported as CSSNamespaceRuleImpl's size):
*    none
*
*  Children / siblings / parents:
*    none
*    
******************************************************************************/
void CSSNameSpaceRuleImpl::SizeOf(nsISizeOfHandler *aSizeOfHandler, PRUint32 &aSize)
{
  NS_ASSERTION(aSizeOfHandler != nsnull, "SizeOf handler cannot be null");

  // first get the unique items collection
  UNIQUE_STYLE_ITEMS(uniqueItems);
  if(! uniqueItems->AddItem((void*)this)){
    return;
  }

  PRUint32 localSize=0;

  // create a tag for this instance
  nsCOMPtr<nsIAtom> tag;
  tag = getter_AddRefs(NS_NewAtom("CSSNameSpaceRuleImpl"));
  // get the size of an empty instance and add to the sizeof handler
  aSize = sizeof(*this);
  
  // get the member data as part of this dump
  mURLSpec.SizeOf(aSizeOfHandler, &localSize);
  aSize += localSize;
  aSize -= sizeof(mURLSpec); // counted in sizeof(*this) and nsString->SizeOf()

  if(mPrefix && uniqueItems->AddItem(mPrefix)){
    mPrefix->SizeOf(aSizeOfHandler, &localSize);
    aSize += localSize;
  }
  aSizeOfHandler->AddSize(tag, aSize);
}

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
    return clone->QueryInterface(NS_GET_IID(nsICSSRule), (void **)&aClone);
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
  return it->QueryInterface(NS_GET_IID(nsICSSNameSpaceRule), (void **) aInstancePtrResult);
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetType(PRUint16* aType)
{
  *aType = nsIDOMCSSRule::UNKNOWN_RULE;
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::GetCssText(nsAWritableString& aCssText)
{
  aCssText.Assign(NS_LITERAL_STRING("@namespace "));
  if (mPrefix) {
    nsString atomStr;
    mPrefix->ToString(atomStr);
    aCssText.Append(atomStr);
    aCssText.Append(NS_LITERAL_STRING(" "));
  }
  aCssText.Append(NS_LITERAL_STRING("url("));
  aCssText.Append(mURLSpec);
  aCssText.Append(NS_LITERAL_STRING(");"));
  return NS_OK;
}

NS_IMETHODIMP
CSSNameSpaceRuleImpl::SetCssText(const nsAReadableString& aCssText)
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
  return NS_ERROR_NOT_IMPLEMENTED;
}

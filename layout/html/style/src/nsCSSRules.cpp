/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
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


static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIStyleRuleIID, NS_ISTYLE_RULE_IID);
static NS_DEFINE_IID(kICSSRuleIID, NS_ICSS_RULE_IID);
static NS_DEFINE_IID(kICSSCharsetRuleIID, NS_ICSS_CHARSET_RULE_IID);
static NS_DEFINE_IID(kICSSImportRuleIID, NS_ICSS_IMPORT_RULE_IID);
static NS_DEFINE_IID(kICSSMediaRuleIID, NS_ICSS_MEDIA_RULE_IID);
static NS_DEFINE_IID(kICSSNameSpaceRuleIID, NS_ICSS_NAMESPACE_RULE_IID);

#define DECL_STYLE_RULE_INHERIT  \
NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const; \
NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet); \
NS_IMETHOD GetStrength(PRInt32& aStrength) const; \
NS_IMETHOD MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext); \
NS_IMETHOD MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext); \

#define IMPL_STYLE_RULE_INHERIT(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::SetStyleSheet(nsICSSStyleSheet* aSheet) { return super::SetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::GetStrength(PRInt32& aStrength) const { return super::GetStrength(aStrength); }   \
NS_IMETHODIMP _class::MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) { return NS_OK; } \
NS_IMETHODIMP _class::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) { return NS_OK; } 

#define IMPL_STYLE_RULE_INHERIT2(_class, super) \
NS_IMETHODIMP _class::GetStyleSheet(nsIStyleSheet*& aSheet) const { return super::GetStyleSheet(aSheet); }  \
NS_IMETHODIMP _class::GetStrength(PRInt32& aStrength) const { return super::GetStrength(aStrength); }   \
NS_IMETHODIMP _class::MapFontStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) { return NS_OK; } \
NS_IMETHODIMP _class::MapStyleInto(nsIStyleContext* aContext, nsIPresContext* aPresContext) { return NS_OK; } 

// -------------------------------------------
// nsICSSCharsetRule
//
class CSSCharsetRuleImpl : public nsCSSRule,
                           public nsICSSCharsetRule 
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

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSCharsetRule methods
  NS_IMETHOD  GetEncoding(nsString& aEncoding) const;

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

nsresult 
CSSCharsetRuleImpl::QueryInterface(const nsIID& aIID,
                                   void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kICSSCharsetRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSCharsetRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICSSRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICSSCharsetRule *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
    return clone->QueryInterface(kICSSRuleIID, (void **)&aClone);
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
  return it->QueryInterface(kICSSCharsetRuleIID, (void **) aInstancePtrResult);
}


// -------------------------------------------
// nsICSSImportRule
//
class CSSImportRuleImpl : public nsCSSRule,
                          public nsICSSImportRule 
{
public:
  CSSImportRuleImpl(void);
  CSSImportRuleImpl(const CSSImportRuleImpl& aCopy);
  virtual ~CSSImportRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSImportRule methods
  NS_IMETHOD SetURLSpec(const nsString& aURLSpec);
  NS_IMETHOD GetURLSpec(nsString& aURLSpec) const;

  NS_IMETHOD SetMedia(const nsString& aMedia);
  NS_IMETHOD GetMedia(nsString& aMedia) const;

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

nsresult 
CSSImportRuleImpl::QueryInterface(const nsIID& aIID,
                                  void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kICSSImportRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSImportRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICSSRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICSSImportRule *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
    return clone->QueryInterface(kICSSRuleIID, (void **)&aClone);
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
  return it->QueryInterface(kICSSImportRuleIID, (void **) aInstancePtrResult);
}

// -------------------------------------------
// nsICSSMediaRule
//
class CSSMediaRuleImpl : public nsCSSRule,
                         public nsICSSMediaRule 
{
public:
  CSSMediaRuleImpl(void);
  CSSMediaRuleImpl(const CSSMediaRuleImpl& aCopy);
  virtual ~CSSMediaRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSMediaRule methods
  NS_IMETHOD  SetMedia(nsISupportsArray* aMedia);
  NS_IMETHOD  HasMedium(nsIAtom* aMedium) const;

  NS_IMETHOD  AppendStyleRule(nsICSSRule* aRule);

  NS_IMETHOD  StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD  GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;

  NS_IMETHOD  EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const;

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

nsresult 
CSSMediaRuleImpl::QueryInterface(const nsIID& aIID,
                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kICSSMediaRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSMediaRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICSSRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICSSMediaRule *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
    return clone->QueryInterface(kICSSRuleIID, (void **)&aClone);
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
CSSMediaRuleImpl::HasMedium(nsIAtom* aMedium) const
{
  if (mMedia) {
    if (-1 != mMedia->IndexOf(aMedium)) {
      return NS_OK;
    }
    if (-1 != mMedia->IndexOf(nsLayoutAtoms::all)) {
      return NS_OK;
    }
  }
  return NS_COMFALSE;
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
    return ((mRules->EnumerateForwards(aFunc, aData)) ? NS_OK : NS_COMFALSE);
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

  return it->QueryInterface(kICSSMediaRuleIID, (void **) aInstancePtrResult);
}


// -------------------------------------------
// nsICSSNameSpaceRule
//
class CSSNameSpaceRuleImpl : public nsCSSRule,
                             public nsICSSNameSpaceRule 
{
public:
  CSSNameSpaceRuleImpl(void);
  CSSNameSpaceRuleImpl(const CSSNameSpaceRuleImpl& aCopy);
  virtual ~CSSNameSpaceRuleImpl(void);

  NS_DECL_ISUPPORTS_INHERITED

  DECL_STYLE_RULE_INHERIT

  // nsIStyleRule methods
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsICSSNameSpaceRule methods
  NS_IMETHOD GetPrefix(nsIAtom*& aPrefix) const;
  NS_IMETHOD SetPrefix(nsIAtom* aPrefix);

  NS_IMETHOD GetURLSpec(nsString& aURLSpec) const;
  NS_IMETHOD SetURLSpec(const nsString& aURLSpec);

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

nsresult 
CSSNameSpaceRuleImpl::QueryInterface(const nsIID& aIID,
                                     void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kICSSNameSpaceRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSNameSpaceRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kICSSRuleIID)) {
    *aInstancePtrResult = (void*) ((nsICSSRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStyleRuleIID)) {
    *aInstancePtrResult = (void*) ((nsIStyleRule*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    nsICSSNameSpaceRule *tmp = this;
    nsISupports *tmp2 = tmp;
    *aInstancePtrResult = (void*) tmp2;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

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
    return clone->QueryInterface(kICSSRuleIID, (void **)&aClone);
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
  return it->QueryInterface(kICSSNameSpaceRuleIID, (void **) aInstancePtrResult);
}


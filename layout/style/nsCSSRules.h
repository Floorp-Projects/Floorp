/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=2:et:sw=2:
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
 *   L. David Baron <dbaron@dbaron.org>
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

#ifndef nsCSSRules_h_
#define nsCSSRules_h_

#include "nsCSSRule.h"
#include "nsICSSGroupRule.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsIDOMCSSFontFaceRule.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsAutoPtr.h"
#include "nsCSSProperty.h"
#include "nsCSSValue.h"

class CSSGroupRuleRuleListImpl;
class nsMediaList;
template<class T> struct already_AddRefed;

#define DECL_STYLE_RULE_INHERIT_NO_DOMRULE  \
virtual already_AddRefed<nsIStyleSheet> GetStyleSheet() const; \
virtual void SetStyleSheet(nsCSSStyleSheet* aSheet); \
virtual void SetParentRule(nsICSSGroupRule* aRule); \
virtual void MapRuleInfoInto(nsRuleData* aRuleData);

#define DECL_STYLE_RULE_INHERIT  \
DECL_STYLE_RULE_INHERIT_NO_DOMRULE \
nsIDOMCSSRule* GetDOMRuleWeak(nsresult* aResult);

// inherits from nsCSSRule and also implements methods on nsICSSGroupRule
// so they can be shared between nsCSSMediaRule and nsCSSDocumentRule
class nsCSSGroupRule : public nsCSSRule, public nsICSSGroupRule
{
protected:
  nsCSSGroupRule();
  nsCSSGroupRule(const nsCSSGroupRule& aCopy);
  ~nsCSSGroupRule();

  // Implement part of nsISupports.
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
protected:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
public:

  // implement part of nsIStyleRule and nsICSSRule
  DECL_STYLE_RULE_INHERIT_NO_DOMRULE

  // to help implement nsIStyleRule
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

public:
  // implement nsICSSGroupRule
  NS_IMETHOD AppendStyleRule(nsICSSRule* aRule);
  NS_IMETHOD StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;
  NS_IMETHOD_(PRBool) EnumerateRulesForwards(RuleEnumFunc aFunc, void * aData) const;
  NS_IMETHOD DeleteStyleRuleAt(PRUint32 aIndex);
  NS_IMETHOD InsertStyleRulesAt(PRUint32 aIndex,
                                nsCOMArray<nsICSSRule>& aRules);
  NS_IMETHOD ReplaceStyleRule(nsICSSRule *aOld, nsICSSRule *aNew);

protected:
  // to help implement nsIDOMCSSRule
  nsresult AppendRulesToCssText(nsAString& aCssText);
  // to implement methods on nsIDOMCSSRule
  nsresult GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet);
  nsresult GetParentRule(nsIDOMCSSRule** aParentRule);

  // to implement common methods on nsIDOMCSSMediaRule and
  // nsIDOMCSSMozDocumentRule
  nsresult GetCssRules(nsIDOMCSSRuleList* *aRuleList);
  nsresult InsertRule(const nsAString & aRule, PRUint32 aIndex,
                      PRUint32* _retval);
  nsresult DeleteRule(PRUint32 aIndex);

  nsCOMArray<nsICSSRule> mRules;
  CSSGroupRuleRuleListImpl* mRuleCollection;
};

class nsCSSMediaRule : public nsCSSGroupRule,
                       public nsIDOMCSSMediaRule
{
public:
  nsCSSMediaRule();
  nsCSSMediaRule(const nsCSSMediaRule& aCopy);
  virtual ~nsCSSMediaRule();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  virtual void SetStyleSheet(nsCSSStyleSheet* aSheet); //override nsCSSGroupRule
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<nsICSSRule> Clone() const;
  nsIDOMCSSRule* GetDOMRuleWeak(nsresult *aResult)
  {
    *aResult = NS_OK;
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMediaRule interface
  NS_DECL_NSIDOMCSSMEDIARULE

  // rest of nsICSSGroupRule interface
  NS_IMETHOD_(PRBool) UseForPresentation(nsPresContext* aPresContext,
                                         nsMediaQueryResultCacheKey& aKey);

  // @media rule methods
  nsresult SetMedia(nsMediaList* aMedia);
  
protected:
  nsRefPtr<nsMediaList> mMedia;
};

class nsCSSDocumentRule : public nsCSSGroupRule,
                          public nsIDOMCSSMozDocumentRule
{
public:
  nsCSSDocumentRule(void);
  nsCSSDocumentRule(const nsCSSDocumentRule& aCopy);
  virtual ~nsCSSDocumentRule(void);

  NS_DECL_ISUPPORTS_INHERITED

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  virtual PRInt32 GetType() const;
  virtual already_AddRefed<nsICSSRule> Clone() const;
  nsIDOMCSSRule* GetDOMRuleWeak(nsresult *aResult)
  {
    *aResult = NS_OK;
    return this;
  }

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMozDocumentRule interface
  NS_DECL_NSIDOMCSSMOZDOCUMENTRULE

  // rest of nsICSSGroupRule interface
  NS_IMETHOD_(PRBool) UseForPresentation(nsPresContext* aPresContext,
                                         nsMediaQueryResultCacheKey& aKey);

  enum Function {
    eURL,
    eURLPrefix,
    eDomain
  };

  struct URL {
    Function func;
    nsCString url;
    URL *next;

    URL() : next(nsnull) {}
    URL(const URL& aOther)
      : func(aOther.func)
      , url(aOther.url)
      , next(aOther.next ? new URL(*aOther.next) : nsnull)
    {
    }
    ~URL();
  };

  void SetURLs(URL *aURLs) { mURLs = aURLs; }

protected:
  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};

// A nsCSSFontFaceStyleDecl is always embedded in a nsCSSFontFaceRule.
class nsCSSFontFaceRule;
class nsCSSFontFaceStyleDecl : public nsIDOMCSSStyleDeclaration
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCSSSTYLEDECLARATION

  nsresult GetPropertyValue(nsCSSFontDesc aFontDescID,
                            nsAString & aResult NS_OUTPARAM) const;

protected:
  friend class nsCSSFontFaceRule;
  nsCSSValue mFamily;
  nsCSSValue mStyle;
  nsCSSValue mWeight;
  nsCSSValue mStretch;
  nsCSSValue mSrc;
  nsCSSValue mUnicodeRange;
  nsCSSValue mFontFeatureSettings;
  nsCSSValue mFontLanguageOverride;

  static nsCSSValue nsCSSFontFaceStyleDecl::* const Fields[];  
  inline nsCSSFontFaceRule* ContainingRule();
  inline const nsCSSFontFaceRule* ContainingRule() const;

private:
  // NOT TO BE IMPLEMENTED
  // This object cannot be allocated on its own, only as part of
  // nsCSSFontFaceRule.
  void* operator new(size_t size) CPP_THROW_NEW;
};

class nsCSSFontFaceRule : public nsCSSRule,
                          public nsICSSRule,
                          public nsIDOMCSSFontFaceRule
{
public:
  NS_DECL_ISUPPORTS

  // nsIStyleRule methods
#ifdef DEBUG
  virtual void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  DECL_STYLE_RULE_INHERIT

  virtual PRInt32 GetType() const;
  virtual already_AddRefed<nsICSSRule> Clone() const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSFontFaceRule interface
  NS_DECL_NSIDOMCSSFONTFACERULE

  void SetDesc(nsCSSFontDesc aDescID, nsCSSValue const & aValue);
  void GetDesc(nsCSSFontDesc aDescID, nsCSSValue & aValue);

protected:
  friend class nsCSSFontFaceStyleDecl;
  nsCSSFontFaceStyleDecl mDecl;
};

// nsFontFaceRuleContainer - used for associating sheet type with 
// specific @font-face rules
struct nsFontFaceRuleContainer {
  nsRefPtr<nsCSSFontFaceRule> mRule;
  PRUint8 mSheetType;
};

inline nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule()
{
  return reinterpret_cast<nsCSSFontFaceRule*>
    (reinterpret_cast<char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

inline const nsCSSFontFaceRule*
nsCSSFontFaceStyleDecl::ContainingRule() const
{
  return reinterpret_cast<const nsCSSFontFaceRule*>
    (reinterpret_cast<const char*>(this) - offsetof(nsCSSFontFaceRule, mDecl));
}

#endif /* !defined(nsCSSRules_h_) */

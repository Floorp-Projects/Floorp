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
#ifndef nsCSSRules_h_
#define nsCSSRules_h_

#include "nsCSSRule.h"
#include "nsICSSGroupRule.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsISupportsArray.h"
#include "nsIDOMCSSMediaRule.h"
#include "nsIDOMCSSMozDocumentRule.h"
#include "nsString.h"

class CSSGroupRuleRuleListImpl;
class nsIMediaList;

#define DECL_STYLE_RULE_INHERIT  \
NS_IMETHOD GetStyleSheet(nsIStyleSheet*& aSheet) const; \
NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet); \
NS_IMETHOD SetParentRule(nsICSSGroupRule* aRule); \
NS_IMETHOD GetDOMRule(nsIDOMCSSRule** aDOMRule); \
NS_IMETHOD MapRuleInfoInto(nsRuleData* aRuleData);

// inherits from nsCSSRule and also implements methods on nsICSSGroupRule
// so they can be shared between nsCSSMediaRule and nsCSSDocumentRule
class nsCSSGroupRule : public nsCSSRule, public nsICSSGroupRule
{
protected:
  nsCSSGroupRule();
  nsCSSGroupRule(const nsCSSGroupRule& aCopy);
  ~nsCSSGroupRule();

  // implement part of nsIStyleRule and nsICSSRule
  DECL_STYLE_RULE_INHERIT

  // to help implement nsIStyleRule
#ifdef DEBUG
  nsresult List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

public:
  // implement nsICSSGroupRule
  NS_IMETHOD AppendStyleRule(nsICSSRule* aRule);
  NS_IMETHOD StyleRuleCount(PRInt32& aCount) const;
  NS_IMETHOD GetStyleRuleAt(PRInt32 aIndex, nsICSSRule*& aRule) const;
  NS_IMETHOD EnumerateRulesForwards(nsISupportsArrayEnumFunc aFunc, void * aData) const;
  NS_IMETHOD DeleteStyleRuleAt(PRUint32 aIndex);
  NS_IMETHOD InsertStyleRulesAt(PRUint32 aIndex, nsISupportsArray* aRules);
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

  nsCOMPtr<nsISupportsArray> mRules;
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
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  NS_IMETHOD SetStyleSheet(nsICSSStyleSheet* aSheet); //override nsCSSGroupRule
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMediaRule interface
  NS_DECL_NSIDOMCSSMEDIARULE

  // rest of nsICSSGroupRule interface
  NS_IMETHOD_(PRBool) UseForPresentation(nsPresContext* aPresContext);

  // @media rule methods
  nsresult SetMedia(nsISupportsArray* aMedia);
  
protected:
  nsCOMPtr<nsIMediaList> mMedia;
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
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;
#endif

  // nsICSSRule methods
  NS_IMETHOD GetType(PRInt32& aType) const;
  NS_IMETHOD Clone(nsICSSRule*& aClone) const;

  // nsIDOMCSSRule interface
  NS_DECL_NSIDOMCSSRULE

  // nsIDOMCSSMozDocumentRule interface
  NS_DECL_NSIDOMCSSMOZDOCUMENTRULE

  // rest of nsICSSGroupRule interface
  NS_IMETHOD_(PRBool) UseForPresentation(nsPresContext* aPresContext);

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
      , next(new URL(*aOther.next))
    {
    }
    ~URL() { delete next; }
  };

  void SetURLs(URL *aURLs) { mURLs = aURLs; }

protected:
  nsAutoPtr<URL> mURLs; // linked list of |struct URL| above.
};


#endif /* !defined(nsCSSRules_h_) */

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

/* base class for all rule types in a CSS style sheet */

#ifndef mozilla_css_Rule_h___
#define mozilla_css_Rule_h___

#include "nsIStyleRule.h"
#include "nsIDOMCSSRule.h"

class nsIStyleSheet;
class nsCSSStyleSheet;
struct nsRuleData;
template<class T> struct already_AddRefed;

namespace mozilla {
namespace css {
class GroupRule;

#define DECL_STYLE_RULE_INHERIT_NO_DOMRULE  \
virtual void MapRuleInfoInto(nsRuleData* aRuleData);

#define DECL_STYLE_RULE_INHERIT  \
DECL_STYLE_RULE_INHERIT_NO_DOMRULE \
virtual nsIDOMCSSRule* GetDOMRule();

class Rule : public nsIStyleRule {
protected:
  Rule()
    : mSheet(nsnull),
      mParentRule(nsnull)
  {
  }

  Rule(const Rule& aCopy)
    : mSheet(aCopy.mSheet),
      mParentRule(aCopy.mParentRule)
  {
  }

  virtual ~Rule() {}

public:
  // for implementing nsISupports
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
protected:
  nsAutoRefCnt mRefCnt;
  NS_DECL_OWNINGTHREAD
public:

  // The constants in this list must maintain the following invariants:
  //   If a rule of type N must appear before a rule of type M in stylesheets
  //   then N < M
  // Note that nsCSSStyleSheet::RebuildChildList assumes that no other kinds of
  // rules can come between two rules of type IMPORT_RULE.
  enum {
    UNKNOWN_RULE = 0,
    CHARSET_RULE,
    IMPORT_RULE,
    NAMESPACE_RULE,
    STYLE_RULE,
    MEDIA_RULE,
    FONT_FACE_RULE,
    PAGE_RULE,
    KEYFRAME_RULE,
    KEYFRAMES_RULE,
    DOCUMENT_RULE
  };

  virtual PRInt32 GetType() const = 0;

  nsCSSStyleSheet* GetStyleSheet() const { return mSheet; }

  virtual void SetStyleSheet(nsCSSStyleSheet* aSheet);

  void SetParentRule(GroupRule* aRule) {
    // We don't reference count this up reference. The group rule
    // will tell us when it's going away or when we're detached from
    // it.
    mParentRule = aRule;
  }

  /**
   * Clones |this|. Never returns NULL.
   */
  virtual already_AddRefed<Rule> Clone() const = 0;

  // Note that this returns null for inline style rules since they aren't
  // supposed to have a DOM rule representation (and our code wouldn't work).
  virtual nsIDOMCSSRule* GetDOMRule() = 0;

  // to implement methods on nsIDOMCSSRule
  nsresult GetParentRule(nsIDOMCSSRule** aParentRule);
  nsresult GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet);

  // This is pure virtual because all of Rule's data members are non-owning and
  // thus measured elsewhere.
  virtual NS_MUST_OVERRIDE size_t
    SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const = 0;

  // This is used to measure nsCOMArray<Rule>s.
  static size_t SizeOfCOMArrayElementIncludingThis(css::Rule* aElement,
                                                   nsMallocSizeOfFun aMallocSizeOf,
                                                   void* aData);

protected:
  nsCSSStyleSheet*  mSheet;
  GroupRule*        mParentRule;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Rule_h___ */

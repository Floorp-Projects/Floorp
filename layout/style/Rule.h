/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for all rule types in a CSS style sheet */

#ifndef mozilla_css_Rule_h___
#define mozilla_css_Rule_h___

#include "mozilla/CSSStyleSheet.h"
#include "mozilla/MemoryReporting.h"
#include "nsIStyleRule.h"
#include "nsIDOMCSSRule.h"

class nsIStyleSheet;
class nsIDocument;
struct nsRuleData;
template<class T> struct already_AddRefed;
class nsHTMLCSSStyleSheet;

namespace mozilla {
namespace css {
class GroupRule;

#define DECL_STYLE_RULE_INHERIT_NO_DOMRULE  \
virtual void MapRuleInfoInto(nsRuleData* aRuleData);

#define DECL_STYLE_RULE_INHERIT                   \
  DECL_STYLE_RULE_INHERIT_NO_DOMRULE              \
  virtual nsIDOMCSSRule* GetDOMRule();            \
  virtual nsIDOMCSSRule* GetExistingDOMRule();

class Rule : public nsIStyleRule {
protected:
  Rule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mSheet(0),
      mParentRule(nullptr),
      mLineNumber(aLineNumber),
      mColumnNumber(aColumnNumber),
      mWasMatched(false)
  {
  }

  Rule(const Rule& aCopy)
    : mSheet(aCopy.mSheet),
      mParentRule(aCopy.mParentRule),
      mLineNumber(aCopy.mLineNumber),
      mColumnNumber(aCopy.mColumnNumber),
      mWasMatched(false)
  {
  }

  virtual ~Rule() {}

public:

  // The constants in this list must maintain the following invariants:
  //   If a rule of type N must appear before a rule of type M in stylesheets
  //   then N < M
  // Note that CSSStyleSheet::RebuildChildList assumes that no other kinds of
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
    DOCUMENT_RULE,
    SUPPORTS_RULE,
    FONT_FEATURE_VALUES_RULE,
    COUNTER_STYLE_RULE
  };

  virtual int32_t GetType() const = 0;

  CSSStyleSheet* GetStyleSheet() const;
  nsHTMLCSSStyleSheet* GetHTMLCSSStyleSheet() const;

  // Return the document the rule lives in, if any
  nsIDocument* GetDocument() const
  {
    CSSStyleSheet* sheet = GetStyleSheet();
    return sheet ? sheet->GetDocument() : nullptr;
  }

  virtual void SetStyleSheet(CSSStyleSheet* aSheet);
  // This does not need to be virtual, because GroupRule and MediaRule are not
  // used for inline style.
  void SetHTMLCSSStyleSheet(nsHTMLCSSStyleSheet* aSheet);

  void SetParentRule(GroupRule* aRule) {
    // We don't reference count this up reference. The group rule
    // will tell us when it's going away or when we're detached from
    // it.
    mParentRule = aRule;
  }

  uint32_t GetLineNumber() const { return mLineNumber; }
  uint32_t GetColumnNumber() const { return mColumnNumber; }

  /**
   * Clones |this|. Never returns nullptr.
   */
  virtual already_AddRefed<Rule> Clone() const = 0;

  // Note that this returns null for inline style rules since they aren't
  // supposed to have a DOM rule representation (and our code wouldn't work).
  virtual nsIDOMCSSRule* GetDOMRule() = 0;

  // Like GetDOMRule(), but won't create one if we don't have one yet
  virtual nsIDOMCSSRule* GetExistingDOMRule() = 0;

  // to implement methods on nsIDOMCSSRule
  nsresult GetParentRule(nsIDOMCSSRule** aParentRule);
  nsresult GetParentStyleSheet(nsIDOMCSSStyleSheet** aSheet);
  Rule* GetCSSRule();

  // This is pure virtual because all of Rule's data members are non-owning and
  // thus measured elsewhere.
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE = 0;

  // This is used to measure nsCOMArray<Rule>s.
  static size_t SizeOfCOMArrayElementIncludingThis(css::Rule* aElement,
                                                   mozilla::MallocSizeOf aMallocSizeOf,
                                                   void* aData);

protected:
  // This is either a CSSStyleSheet* or an nsHTMLStyleSheet*.  The former
  // if the low bit is 0, the latter if the low bit is 1.
  uintptr_t         mSheet;
  GroupRule*        mParentRule;

  // Keep the same type so that MSVC packs them.
  uint32_t          mLineNumber;
  uint32_t          mColumnNumber : 31;
  uint32_t          mWasMatched : 1;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Rule_h___ */

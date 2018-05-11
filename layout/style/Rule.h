/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* base class for all rule types in a CSS style sheet */

#ifndef mozilla_css_Rule_h___
#define mozilla_css_Rule_h___

#include "mozilla/dom/CSSRuleBinding.h"
#include "mozilla/dom/DocumentOrShadowRoot.h"
#include "mozilla/StyleSheet.h"
#include "mozilla/MemoryReporting.h"
#include "nsISupports.h"
#include "nsWrapperCache.h"

class nsIDocument;
struct nsRuleData;
template<class T> struct already_AddRefed;
class nsHTMLCSSStyleSheet;

namespace mozilla {
namespace css {
class GroupRule;

class Rule : public nsISupports
           , public nsWrapperCache
{
protected:
  Rule(uint32_t aLineNumber, uint32_t aColumnNumber)
    : mSheet(nullptr),
      mParentRule(nullptr),
      mLineNumber(aLineNumber),
      mColumnNumber(aColumnNumber)
  {
  }

  Rule(const Rule& aCopy)
    : mSheet(aCopy.mSheet),
      mParentRule(aCopy.mParentRule),
      mLineNumber(aCopy.mLineNumber),
      mColumnNumber(aCopy.mColumnNumber)
  {
  }

  virtual ~Rule() = default;

public:

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS(Rule)
  // Return true if this rule is known to be a cycle collection leaf, in the
  // sense that it doesn't have any outgoing owning edges.
  virtual bool IsCCLeaf() const MOZ_MUST_OVERRIDE;

#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const = 0;
#endif

  StyleSheet* GetStyleSheet() const { return mSheet; }

  // Return the document the rule applies to, if any.
  //
  // Suitable for style updates, and that's about it.
  nsIDocument* GetComposedDoc() const
  {
    return mSheet ? mSheet->GetComposedDoc() : nullptr;
  }

  virtual void SetStyleSheet(StyleSheet* aSheet);

  // We don't reference count this up reference. The rule will tell us
  // when it's going away or when we're detached from it.
  void SetParentRule(Rule* aRule) {
#ifdef DEBUG
    if (aRule) {
      int16_t type = aRule->Type();
      MOZ_ASSERT(type == dom::CSSRuleBinding::MEDIA_RULE ||
                 type == dom::CSSRuleBinding::DOCUMENT_RULE ||
                 type == dom::CSSRuleBinding::SUPPORTS_RULE ||
                 (type == dom::CSSRuleBinding::KEYFRAMES_RULE &&
                  Type() == dom::CSSRuleBinding::KEYFRAME_RULE));
    }
#endif
    mParentRule = aRule;
  }

  uint32_t GetLineNumber() const { return mLineNumber; }
  uint32_t GetColumnNumber() const { return mColumnNumber; }

  // This is pure virtual because all of Rule's data members are non-owning and
  // thus measured elsewhere.
  virtual size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf)
    const MOZ_MUST_OVERRIDE = 0;

  // WebIDL interface
  virtual uint16_t Type() const = 0;
  virtual void GetCssText(nsAString& aCssText) const = 0;
  void SetCssText(const nsAString& aCssText);
  Rule* GetParentRule() const;
  StyleSheet* GetParentStyleSheet() const { return GetStyleSheet(); }
  nsINode* GetParentObject() const
  {
    if (!mSheet) {
      return nullptr;
    }
    auto* associated = mSheet->GetAssociatedDocumentOrShadowRoot();
    return associated ? &associated->AsNode() : nullptr;
  }

protected:
  // True if we're known-live for cycle collection purposes.
  bool IsKnownLive() const;

  // This is sometimes null (e.g., for style attributes).
  StyleSheet* mSheet;
  // When the parent GroupRule is destroyed, it will call SetParentRule(nullptr)
  // on this object. (Through SetParentRuleReference);
  Rule* MOZ_NON_OWNING_REF mParentRule;

  // Keep the same type so that MSVC packs them.
  uint32_t          mLineNumber;
  uint32_t          mColumnNumber;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Rule_h___ */

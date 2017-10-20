/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of a declaration block in a CSS stylesheet, or of
 * a style attribute
 */

#ifndef mozilla_DeclarationBlock_h
#define mozilla_DeclarationBlock_h

#include "mozilla/Atomics.h"
#include "mozilla/ServoUtils.h"
#include "mozilla/StyleBackendType.h"

#include "nsCSSPropertyID.h"

class nsHTMLCSSStyleSheet;

namespace mozilla {

class ServoDeclarationBlock;

namespace css {
class Declaration;
class Rule;
} // namespace css

class DeclarationBlock
{
protected:
  explicit DeclarationBlock(StyleBackendType aType)
    : mImmutable(false)
    , mType(aType)
    , mIsDirty(false)
  {
    mContainer.mRaw = 0;
  }

  DeclarationBlock(const DeclarationBlock& aCopy)
    : DeclarationBlock(aCopy.mType) {}

public:
  MOZ_DECL_STYLO_METHODS(css::Declaration, ServoDeclarationBlock)

  inline MozExternalRefCountType AddRef();
  inline MozExternalRefCountType Release();

  inline already_AddRefed<DeclarationBlock> Clone() const;

  /**
   * Return whether |this| may be modified.
   */
  bool IsMutable() const {
    return !mImmutable;
  }

  /**
   * Crash if |this| cannot be modified.
   */
  void AssertMutable() const {
    MOZ_ASSERT(IsMutable(), "someone forgot to call EnsureMutable");
  }

  /**
   * Mark this declaration as unmodifiable.  It's 'const' so it can
   * be called from ToString.
   */
  void SetImmutable() { mImmutable = true; }

  /**
   * Return whether |this| has been restyled after modified.
   */
  bool IsDirty() const { return mIsDirty; }

  /**
   * Mark this declaration as dirty.
   */
  void SetDirty() { mIsDirty = true; }

  /**
   * Mark this declaration as not dirty.
   */
  void UnsetDirty() { mIsDirty = false; }

  /**
   * Copy |this|, if necessary to ensure that it can be modified.
   */
  inline already_AddRefed<DeclarationBlock> EnsureMutable();

  void SetOwningRule(css::Rule* aRule) {
    MOZ_ASSERT(!mContainer.mOwningRule || !aRule,
               "should never overwrite one rule with another");
    mContainer.mOwningRule = aRule;
  }

  css::Rule* GetOwningRule() const {
    if (mContainer.mRaw & 0x1) {
      return nullptr;
    }
    return mContainer.mOwningRule;
  }

  void SetHTMLCSSStyleSheet(nsHTMLCSSStyleSheet* aHTMLCSSStyleSheet) {
    MOZ_ASSERT(!mContainer.mHTMLCSSStyleSheet || !aHTMLCSSStyleSheet,
               "should never overwrite one sheet with another");
    mContainer.mHTMLCSSStyleSheet = aHTMLCSSStyleSheet;
    if (aHTMLCSSStyleSheet) {
      mContainer.mRaw |= uintptr_t(1);
    }
  }

  nsHTMLCSSStyleSheet* GetHTMLCSSStyleSheet() const {
    if (!(mContainer.mRaw & 0x1)) {
      return nullptr;
    }
    auto c = mContainer;
    c.mRaw &= ~uintptr_t(1);
    return c.mHTMLCSSStyleSheet;
  }

  inline void ToString(nsAString& aString) const;

  inline uint32_t Count() const;
  inline bool GetNthProperty(uint32_t aIndex, nsAString& aReturn) const;

  inline void GetPropertyValue(const nsAString& aProperty,
                               nsAString& aValue) const;
  inline void GetPropertyValueByID(nsCSSPropertyID aPropID,
                                   nsAString& aValue) const;
  inline bool GetPropertyIsImportant(const nsAString& aProperty) const;
  inline void RemoveProperty(const nsAString& aProperty);
  // Returns whether the property was removed.
  inline bool RemovePropertyByID(nsCSSPropertyID aProperty);

private:
  union {
    // We only ever have one of these since we have an
    // nsHTMLCSSStyleSheet only for style attributes, and style
    // attributes never have an owning rule.

    // It's an nsHTMLCSSStyleSheet if the low bit is set.

    uintptr_t mRaw;

    // The style rule that owns this declaration.  May be null.
    css::Rule* mOwningRule;

    // The nsHTMLCSSStyleSheet that is responsible for this declaration.
    // Only non-null for style attributes.
    nsHTMLCSSStyleSheet* mHTMLCSSStyleSheet;
  } mContainer;

  // set when declaration put in the rule tree;
  bool mImmutable;

  const StyleBackendType mType;

  // True if this declaration has not been restyled after modified.
  //
  // Since we can clear this flag from style worker threads, we use an Atomic.
  //
  // Note that although a single DeclarationBlock can be shared between
  // different rule nodes (due to the style="" attribute cache), whenever a
  // DeclarationBlock has its mIsDirty flag set to true, we always clone it to
  // a unique object first. So when we clear this flag during Servo traversal,
  // we know that we are clearing it on a DeclarationBlock that has a single
  // reference, and there is no problem with another user of the same
  // DeclarationBlock thinking that it is not dirty.
  Atomic<bool, MemoryOrdering::Relaxed> mIsDirty;
};

} // namespace mozilla

#endif // mozilla_DeclarationBlock_h

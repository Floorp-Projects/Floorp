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
#include "mozilla/ServoBindings.h"

#include "nsCSSPropertyID.h"
#include "nsString.h"

namespace mozilla {

class AttributeStyles;
namespace css {
class Declaration;
class Rule;
}  // namespace css

class DeclarationBlock final {
  DeclarationBlock(const DeclarationBlock& aCopy)
      : mRaw(Servo_DeclarationBlock_Clone(aCopy.mRaw).Consume()),
        mImmutable(false),
        mIsDirty(false) {
    mContainer.mRaw = 0;
  }

 public:
  explicit DeclarationBlock(already_AddRefed<StyleLockedDeclarationBlock> aRaw)
      : mRaw(aRaw), mImmutable(false), mIsDirty(false) {
    mContainer.mRaw = 0;
  }

  DeclarationBlock()
      : DeclarationBlock(Servo_DeclarationBlock_CreateEmpty().Consume()) {}

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(DeclarationBlock)

  already_AddRefed<DeclarationBlock> Clone() const {
    return do_AddRef(new DeclarationBlock(*this));
  }

  /**
   * Return whether |this| may be modified.
   */
  bool IsMutable() const { return !mImmutable; }

  /**
   * Crash in debug builds if |this| cannot be modified.
   */
  void AssertMutable() const {
    MOZ_ASSERT(IsMutable(), "someone forgot to call EnsureMutable");
    MOZ_ASSERT(!OwnerIsReadOnly(), "User Agent sheets shouldn't be modified");
  }

  /**
   * Mark this declaration as unmodifiable.
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
  already_AddRefed<DeclarationBlock> EnsureMutable() {
    MOZ_ASSERT(!OwnerIsReadOnly());

    if (!IsDirty()) {
      // In stylo, the old DeclarationBlock is stored in element's rule node
      // tree directly, to avoid new values replacing the DeclarationBlock in
      // the tree directly, we need to copy the old one here if we haven't yet
      // copied. As a result the new value does not replace rule node tree until
      // traversal happens.
      //
      // FIXME(emilio, bug 1606413): This is a hack for ::first-line and
      // transitions starting due to CSSOM changes when other transitions are
      // already running. Try to simplify this setup, so that rule tree updates
      // find the mutated declaration block properly rather than having to
      // insert the cloned declaration in the tree.
      return Clone();
    }

    if (!IsMutable()) {
      return Clone();
    }

    return do_AddRef(this);
  }

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

  void SetAttributeStyles(AttributeStyles* aAttributeStyles) {
    MOZ_ASSERT(!mContainer.mAttributeStyles || !aAttributeStyles,
               "should never overwrite one sheet with another");
    mContainer.mAttributeStyles = aAttributeStyles;
    if (aAttributeStyles) {
      mContainer.mRaw |= uintptr_t(1);
    }
  }

  AttributeStyles* GetAttributeStyles() const {
    if (!(mContainer.mRaw & 0x1)) {
      return nullptr;
    }
    auto c = mContainer;
    c.mRaw &= ~uintptr_t(1);
    return c.mAttributeStyles;
  }

  bool IsReadOnly() const;

  size_t SizeofIncludingThis(MallocSizeOf);

  static already_AddRefed<DeclarationBlock> FromCssText(
      const nsACString& aCssText, URLExtraData* aExtraData,
      nsCompatibility aMode, css::Loader* aLoader, StyleCssRuleType aRuleType) {
    RefPtr<StyleLockedDeclarationBlock> raw =
        Servo_ParseStyleAttribute(&aCssText, aExtraData, aMode, aLoader,
                                  aRuleType)
            .Consume();
    return MakeAndAddRef<DeclarationBlock>(raw.forget());
  }

  static already_AddRefed<DeclarationBlock> FromCssText(
      const nsAString& aCssText, URLExtraData* aExtraData,
      nsCompatibility aMode, css::Loader* aLoader, StyleCssRuleType aRuleType) {
    NS_ConvertUTF16toUTF8 value(aCssText);
    return FromCssText(value, aExtraData, aMode, aLoader, aRuleType);
  }

  StyleLockedDeclarationBlock* Raw() const { return mRaw; }

  void ToString(nsACString& aResult) const {
    Servo_DeclarationBlock_GetCssText(mRaw, &aResult);
  }

  uint32_t Count() const { return Servo_DeclarationBlock_Count(mRaw); }

  bool GetNthProperty(uint32_t aIndex, nsACString& aReturn) const {
    aReturn.Truncate();
    return Servo_DeclarationBlock_GetNthProperty(mRaw, aIndex, &aReturn);
  }

  void GetPropertyValue(const nsACString& aProperty, nsACString& aValue) const {
    Servo_DeclarationBlock_GetPropertyValue(mRaw, &aProperty, &aValue);
  }

  void GetPropertyValueByID(nsCSSPropertyID aPropID, nsACString& aValue) const {
    Servo_DeclarationBlock_GetPropertyValueById(mRaw, aPropID, &aValue);
  }

  bool GetPropertyIsImportant(const nsACString& aProperty) const {
    return Servo_DeclarationBlock_GetPropertyIsImportant(mRaw, &aProperty);
  }

  // Returns whether the property was removed.
  bool RemoveProperty(const nsACString& aProperty,
                      DeclarationBlockMutationClosure aClosure = {}) {
    AssertMutable();
    return Servo_DeclarationBlock_RemoveProperty(mRaw, &aProperty, aClosure);
  }

  // Returns whether the property was removed.
  bool RemovePropertyByID(nsCSSPropertyID aProperty,
                          DeclarationBlockMutationClosure aClosure = {}) {
    AssertMutable();
    return Servo_DeclarationBlock_RemovePropertyById(mRaw, aProperty, aClosure);
  }

 private:
  ~DeclarationBlock() = default;

  bool OwnerIsReadOnly() const;

  union {
    // We only ever have one of these since we have a AttributeStyles only for
    // style attributes, and style attributes never have an owning rule. It's a
    // AttributeStyles if the low bit is set.

    uintptr_t mRaw;

    // The style rule that owns this declaration.  May be null.
    css::Rule* mOwningRule;

    // The AttributeStyles that is responsible for this declaration. Only
    // non-null for style attributes.
    AttributeStyles* mAttributeStyles;
  } mContainer;

  RefPtr<StyleLockedDeclarationBlock> mRaw;

  // set when declaration put in the rule tree;
  bool mImmutable;

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

}  // namespace mozilla

#endif  // mozilla_DeclarationBlock_h

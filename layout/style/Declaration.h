/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * representation of a declaration block (or style attribute) in a CSS
 * stylesheet
 */

#ifndef mozilla_css_Declaration_h
#define mozilla_css_Declaration_h

// This header is in EXPORTS because it's used in several places in content/,
// but it's not really a public interface.
#ifndef MOZILLA_INTERNAL_API
#error "This file should only be included within libxul"
#endif

#include "mozilla/Attributes.h"
#include "mozilla/MemoryReporting.h"
#include "CSSVariableDeclarations.h"
#include "nsCSSDataBlock.h"
#include "nsCSSProperty.h"
#include "nsCSSProps.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include <stdio.h>

namespace mozilla {
namespace css {

// Declaration objects have unusual lifetime rules.  Every declaration
// begins life in an invalid state which ends when InitializeEmpty or
// CompressFrom is called upon it.  After that, it can be attached to
// exactly one style rule, and will be destroyed when that style rule
// is destroyed.  A declaration becomes immutable when its style rule's
// |RuleMatched| method is called; after that, it must be copied before
// it can be modified, which is taken care of by |EnsureMutable|.

class Declaration {
public:
  /**
   * Construct an |Declaration| that is in an invalid state (null
   * |mData|) and cannot be used until its |CompressFrom| method or
   * |InitializeEmpty| method is called.
   */
  Declaration();

  Declaration(const Declaration& aCopy);

  ~Declaration();

  /**
   * |ValueAppended| must be called to maintain this declaration's
   * |mOrder| whenever a property is parsed into an expanded data block
   * for this declaration.  aProperty must not be a shorthand.
   */
  void ValueAppended(nsCSSProperty aProperty);

  void RemoveProperty(nsCSSProperty aProperty);

  bool HasProperty(nsCSSProperty aProperty) const;

  void GetValue(nsCSSProperty aProperty, nsAString& aValue) const;

  bool HasImportantData() const {
    return mImportantData || mImportantVariables;
  }
  bool GetValueIsImportant(nsCSSProperty aProperty) const;
  bool GetValueIsImportant(const nsAString& aProperty) const;

  /**
   * Adds a custom property declaration to this object.
   *
   * @param aName The variable name (i.e., without the "var-" prefix).
   * @param aType The type of value the variable has.
   * @param aValue The value of the variable, if aType is
   *   CSSVariableDeclarations::eTokenStream.
   * @param aIsImportant Whether the declaration is !important.
   * @param aOverrideImportant When aIsImportant is false, whether an
   *   existing !important declaration will be overridden.
   */
  void AddVariableDeclaration(const nsAString& aName,
                              CSSVariableDeclarations::Type aType,
                              const nsString& aValue,
                              bool aIsImportant,
                              bool aOverrideImportant);

  /**
   * Removes a custom property declaration from this object.
   *
   * @param aName The variable name (i.e., without the "var-" prefix).
   */
  void RemoveVariableDeclaration(const nsAString& aName);

  /**
   * Returns whether a custom property declaration for a variable with
   * a given name exists on this object.
   *
   * @param aName The variable name (i.e., without the "var-" prefix).
   */
  bool HasVariableDeclaration(const nsAString& aName) const;

  /**
   * Gets the string value for a custom property declaration of a variable
   * with a given name.
   *
   * @param aName The variable name (i.e., without the "var-" prefix).
   * @param aValue Out parameter into which the variable's value will be
   *   stored.  If the value is 'initial' or 'inherit', that exact string
   *   will be stored in aValue.
   */
  void GetVariableDeclaration(const nsAString& aName, nsAString& aValue) const;

  /**
   * Returns whether the custom property declaration for a variable with
   * the given name was !important.
   */
  bool GetVariableValueIsImportant(const nsAString& aName) const;

  uint32_t Count() const {
    return mOrder.Length();
  }

  // Returns whether we actually had a property at aIndex
  bool GetNthProperty(uint32_t aIndex, nsAString& aReturn) const;

  void ToString(nsAString& aString) const;

  nsCSSCompressedDataBlock* GetNormalBlock() const { return mData; }
  nsCSSCompressedDataBlock* GetImportantBlock() const { return mImportantData; }

  /**
   * Initialize this declaration as holding no data.  Cannot fail.
   */
  void InitializeEmpty();

  /**
   * Transfer all of the state from |aExpandedData| into this declaration.
   * After calling, |aExpandedData| should be in its initial state.
   * Callers must make sure mOrder is updated as necessary.
   */
  void CompressFrom(nsCSSExpandedDataBlock *aExpandedData) {
    NS_ABORT_IF_FALSE(!mData, "oops");
    NS_ABORT_IF_FALSE(!mImportantData, "oops");
    aExpandedData->Compress(getter_Transfers(mData),
                            getter_Transfers(mImportantData));
    aExpandedData->AssertInitialState();
  }

  /**
   * Transfer all of the state from this declaration into
   * |aExpandedData| and put this declaration temporarily into an
   * invalid state (ended by |CompressFrom| or |InitializeEmpty|) that
   * should last only during parsing.  During this time only
   * |ValueAppended| should be called.
   */
  void ExpandTo(nsCSSExpandedDataBlock *aExpandedData) {
    AssertMutable();
    aExpandedData->AssertInitialState();

    NS_ABORT_IF_FALSE(mData, "oops");
    aExpandedData->Expand(mData.forget(), mImportantData.forget());
  }

  /**
   * Do what |nsIStyleRule::MapRuleInfoInto| needs to do for a style
   * rule using this declaration for storage.
   */
  void MapNormalRuleInfoInto(nsRuleData *aRuleData) const {
    NS_ABORT_IF_FALSE(mData, "called while expanded");
    mData->MapRuleInfoInto(aRuleData);
    if (mVariables) {
      mVariables->MapRuleInfoInto(aRuleData);
    }
  }
  void MapImportantRuleInfoInto(nsRuleData *aRuleData) const {
    NS_ABORT_IF_FALSE(mData, "called while expanded");
    NS_ABORT_IF_FALSE(mImportantData || mImportantVariables,
                      "must have important data or variables");
    if (mImportantData) {
      mImportantData->MapRuleInfoInto(aRuleData);
    }
    if (mImportantVariables) {
      mImportantVariables->MapRuleInfoInto(aRuleData);
    }
  }

  /**
   * Attempt to replace the value for |aProperty| stored in this
   * declaration with the matching value from |aFromBlock|.
   * This method may only be called on a mutable declaration.
   * It will fail (returning false) if |aProperty| is shorthand,
   * is not already in this declaration, or does not have the indicated
   * importance level.  If it returns true, it erases the value in
   * |aFromBlock|.  |aChanged| is set to true if the declaration
   * changed as a result of the call, and to false otherwise.
   */
  bool TryReplaceValue(nsCSSProperty aProperty, bool aIsImportant,
                         nsCSSExpandedDataBlock& aFromBlock,
                         bool* aChanged)
  {
    AssertMutable();
    NS_ABORT_IF_FALSE(mData, "called while expanded");

    if (nsCSSProps::IsShorthand(aProperty)) {
      *aChanged = false;
      return false;
    }
    nsCSSCompressedDataBlock *block = aIsImportant ? mImportantData : mData;
    // mImportantData might be null
    if (!block) {
      *aChanged = false;
      return false;
    }

#ifdef DEBUG
    {
      nsCSSCompressedDataBlock *other = aIsImportant ? mData : mImportantData;
      NS_ABORT_IF_FALSE(!other || !other->ValueFor(aProperty) ||
                        !block->ValueFor(aProperty),
                        "Property both important and not?");
    }
#endif
    return block->TryReplaceValue(aProperty, aFromBlock, aChanged);
  }

  bool HasNonImportantValueFor(nsCSSProperty aProperty) const {
    NS_ABORT_IF_FALSE(!nsCSSProps::IsShorthand(aProperty), "must be longhand");
    return !!mData->ValueFor(aProperty);
  }

  /**
   * Return whether |this| may be modified.
   */
  bool IsMutable() const {
    return !mImmutable;
  }

  /**
   * Copy |this|, if necessary to ensure that it can be modified.
   */
  Declaration* EnsureMutable();

  /**
   * Crash if |this| cannot be modified.
   */
  void AssertMutable() const {
    NS_ABORT_IF_FALSE(IsMutable(), "someone forgot to call EnsureMutable");
  }

  /**
   * Mark this declaration as unmodifiable.  It's 'const' so it can
   * be called from ToString.
   */
  void SetImmutable() const { mImmutable = true; }

  /**
   * Clear the data, in preparation for its replacement with entirely
   * new data by a call to |CompressFrom|.
   */
  void ClearData() {
    AssertMutable();
    mData = nullptr;
    mImportantData = nullptr;
    mVariables = nullptr;
    mImportantVariables = nullptr;
    mOrder.Clear();
    mVariableOrder.Clear();
  }

#ifdef DEBUG
  void List(FILE* out = stdout, int32_t aIndent = 0) const;
#endif

private:
  Declaration& operator=(const Declaration& aCopy) MOZ_DELETE;
  bool operator==(const Declaration& aCopy) const MOZ_DELETE;

  static void AppendImportanceToString(bool aIsImportant, nsAString& aString);
  // return whether there was a value in |aValue| (i.e., it had a non-null unit)
  bool AppendValueToString(nsCSSProperty aProperty, nsAString& aResult) const;
  // Helper for ToString with strange semantics regarding aValue.
  void AppendPropertyAndValueToString(nsCSSProperty aProperty,
                                      nsAutoString& aValue,
                                      nsAString& aResult) const;
  // helper for ToString that serializes a custom property declaration for
  // a variable with the specified name
  void AppendVariableAndValueToString(const nsAString& aName,
                                      nsAString& aResult) const;

public:
  /**
   * Returns the property at the given index in the ordered list of
   * declarations.  For custom properties, eCSSPropertyExtra_variable
   * is returned.
   */
  nsCSSProperty GetPropertyAt(uint32_t aIndex) const {
    uint32_t value = mOrder[aIndex];
    if (value >= eCSSProperty_COUNT) {
      return eCSSPropertyExtra_variable;
    }
    return nsCSSProperty(value);
  }

  /**
   * Gets the name of the custom property at the given index in the ordered
   * list of declarations.
   */
  void GetCustomPropertyNameAt(uint32_t aIndex, nsAString& aResult) const {
    MOZ_ASSERT(mOrder[aIndex] >= eCSSProperty_COUNT);
    aResult.Truncate();
    aResult.AppendLiteral("var-");
    aResult.Append(mVariableOrder[aIndex]);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // The order of properties in this declaration.  Longhand properties are
  // represented by their nsCSSProperty value, and each custom property (var-*)
  // is represented by a value that begins at eCSSProperty_COUNT.
  //
  // Subtracting eCSSProperty_COUNT from those values that represent custom
  // properties results in an index into mVariableOrder, which identifies the
  // specific variable the custom property declaration is for.
  nsAutoTArray<uint32_t, 8> mOrder;

  // variable names of custom properties found in mOrder
  nsTArray<nsString> mVariableOrder;

  // never null, except while expanded, or before the first call to
  // InitializeEmpty or CompressFrom.
  nsAutoPtr<nsCSSCompressedDataBlock> mData;

  // may be null
  nsAutoPtr<nsCSSCompressedDataBlock> mImportantData;

  // may be null
  nsAutoPtr<CSSVariableDeclarations> mVariables;

  // may be null
  nsAutoPtr<CSSVariableDeclarations> mImportantVariables;

  // set by style rules when |RuleMatched| is called;
  // also by ToString (hence the 'mutable').
  mutable bool mImmutable;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Declaration_h */

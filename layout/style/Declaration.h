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
#include "mozilla/DeclarationBlock.h"
#include "mozilla/MemoryReporting.h"
#include "CSSVariableDeclarations.h"
#include "nsCSSDataBlock.h"
#include "nsCSSPropertyID.h"
#include "nsCSSProps.h"
#include "nsIStyleRule.h"
#include "nsStringFwd.h"
#include "nsTArray.h"
#include <stdio.h>

// feec07b8-3fe6-491e-90d5-cc93f853e048
#define NS_CSS_DECLARATION_IMPL_CID \
{ 0xfeec07b8, 0x3fe6, 0x491e, \
  { 0x90, 0xd5, 0xcc, 0x93, 0xf8, 0x53, 0xe0, 0x48 } }

class nsHTMLCSSStyleSheet;

namespace mozilla {
namespace css {

class Rule;
class Declaration;

/**
 * ImportantStyleData is the implementation of nsIStyleRule (a source of
 * style data) representing the style data coming from !important rules;
 * the !important declarations need a separate nsIStyleRule object since
 * they fit at a different point in the cascade.
 *
 * ImportantStyleData is allocated only as part of a Declaration object.
 */
class ImportantStyleData final : public nsIStyleRule
{
public:

  NS_DECL_ISUPPORTS

  inline ::mozilla::css::Declaration* Declaration();

  // nsIStyleRule interface
  virtual void MapRuleInfoInto(nsRuleData* aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
  virtual bool GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                             nsCSSValue* aValue) override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

private:
  ImportantStyleData() {}
  ~ImportantStyleData() {}

  friend class ::mozilla::css::Declaration;
};

// Declaration objects have unusual lifetime rules.  Every declaration
// begins life in an invalid state which ends when InitializeEmpty or
// CompressFrom is called upon it.  After that, it can be attached to
// exactly one style rule, and will be destroyed when that style rule
// is destroyed.  A declaration becomes immutable (via a SetImmutable
// call) when it is matched (put in the rule tree); after that, it must
// be copied before it can be modified, which is taken care of by
// |EnsureMutable|.

class Declaration final : public DeclarationBlock
                        , public nsIStyleRule
{
public:
  /**
   * Construct an |Declaration| that is in an invalid state (null
   * |mData|) and cannot be used until its |CompressFrom| method or
   * |InitializeEmpty| method is called.
   */
  Declaration() : DeclarationBlock(StyleBackendType::Gecko) {}

  Declaration(const Declaration& aCopy);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_CSS_DECLARATION_IMPL_CID)

  // If this ever becomes cycle-collected, please change the CC implementation
  // for StyleRule to traverse it.
  NS_DECL_ISUPPORTS

private:
  ~Declaration();

public:

  // nsIStyleRule implementation
  virtual void MapRuleInfoInto(nsRuleData *aRuleData) override;
  virtual bool MightMapInheritedStyleData() override;
  virtual bool GetDiscretelyAnimatedCSSValue(nsCSSPropertyID aProperty,
                                             nsCSSValue* aValue) override;
#ifdef DEBUG
  virtual void List(FILE* out = stdout, int32_t aIndent = 0) const override;
#endif

  /**
   * |ValueAppended| must be called to maintain this declaration's
   * |mOrder| whenever a property is parsed into an expanded data block
   * for this declaration.  aProperty must not be a shorthand.
   */
  void ValueAppended(nsCSSPropertyID aProperty);

  void GetPropertyValue(const nsAString& aProperty, nsAString& aValue) const;
  void GetPropertyValueByID(nsCSSPropertyID aPropID, nsAString& aValue) const;
  bool GetPropertyIsImportant(const nsAString& aProperty) const;
  void RemoveProperty(const nsAString& aProperty);
  void RemovePropertyByID(nsCSSPropertyID aProperty);

  bool HasProperty(nsCSSPropertyID aProperty) const;

  bool HasImportantData() const {
    return mImportantData || mImportantVariables;
  }

  /**
   * Adds a custom property declaration to this object.
   *
   * @param aName The variable name (i.e., without the "--" prefix).
   * @param aType The type of value the variable has.
   * @param aValue The value of the variable, if aType is
   *   CSSVariableDeclarations::eTokenStream.
   * @param aIsImportant Whether the declaration is !important.
   * @param aOverrideImportant When aIsImportant is false, whether an
   *   existing !important declaration will be overridden.
   */
  void AddVariable(const nsAString& aName,
                   CSSVariableDeclarations::Type aType,
                   const nsString& aValue,
                   bool aIsImportant,
                   bool aOverrideImportant);

  /**
   * Removes a custom property declaration from this object.
   *
   * @param aName The variable name (i.e., without the "--" prefix).
   */
  void RemoveVariable(const nsAString& aName);

  /**
   * Gets the string value for a custom property declaration of a variable
   * with a given name.
   *
   * @param aName The variable name (i.e., without the "--" prefix).
   * @param aValue Out parameter into which the variable's value will be
   *   stored.  If the value is 'initial' or 'inherit', that exact string
   *   will be stored in aValue.
   */
  void GetVariableValue(const nsAString& aName, nsAString& aValue) const;

  /**
   * Returns whether the custom property declaration for a variable with
   * the given name was !important.
   */
  bool GetVariableIsImportant(const nsAString& aName) const;

  uint32_t Count() const {
    return mOrder.Length();
  }

  // Returns whether we actually had a property at aIndex
  bool GetNthProperty(uint32_t aIndex, nsAString& aReturn) const;

  void ToString(nsAString& aString) const;

  nsCSSCompressedDataBlock* GetNormalBlock() const { return mData; }
  nsCSSCompressedDataBlock* GetImportantBlock() const { return mImportantData; }

  void AssertNotExpanded() const {
    MOZ_ASSERT(mData, "should only be called when not expanded");
  }

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
    MOZ_ASSERT(!mData, "oops");
    MOZ_ASSERT(!mImportantData, "oops");
    aExpandedData->Compress(getter_Transfers(mData),
                            getter_Transfers(mImportantData),
                            mOrder);
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

    MOZ_ASSERT(mData, "oops");
    aExpandedData->Expand(mData.forget(), mImportantData.forget());
  }

  void MapImportantRuleInfoInto(nsRuleData *aRuleData) const {
    AssertNotExpanded();
    MOZ_ASSERT(mImportantData || mImportantVariables,
               "must have important data or variables");
    if (mImportantData) {
      mImportantData->MapRuleInfoInto(aRuleData);
    }
    if (mImportantVariables) {
      mImportantVariables->MapRuleInfoInto(aRuleData);
    }
  }

  bool MapsImportantInheritedStyleData() const;

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
  bool TryReplaceValue(nsCSSPropertyID aProperty, bool aIsImportant,
                         nsCSSExpandedDataBlock& aFromBlock,
                         bool* aChanged)
  {
    AssertMutable();
    AssertNotExpanded();

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
      MOZ_ASSERT(!other || !other->ValueFor(aProperty) ||
                 !block->ValueFor(aProperty),
                 "Property both important and not?");
    }
#endif
    return block->TryReplaceValue(aProperty, aFromBlock, aChanged);
  }

  bool HasNonImportantValueFor(nsCSSPropertyID aProperty) const {
    MOZ_ASSERT(!nsCSSProps::IsShorthand(aProperty), "must be longhand");
    return !!mData->ValueFor(aProperty);
  }

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

  ImportantStyleData* GetImportantStyleData() {
    if (HasImportantData()) {
      return &mImportantStyleData;
    }
    return nullptr;
  }

private:
  Declaration& operator=(const Declaration& aCopy) = delete;
  bool operator==(const Declaration& aCopy) const = delete;

  void GetPropertyValueInternal(nsCSSPropertyID aProperty, nsAString& aValue,
                                nsCSSValue::Serialization aValueSerialization,
                                bool* aIsTokenStream = nullptr) const;
  bool GetPropertyIsImportantByID(nsCSSPropertyID aProperty) const;

  static void AppendImportanceToString(bool aIsImportant, nsAString& aString);
  // return whether there was a value in |aValue| (i.e., it had a non-null unit)
  bool AppendValueToString(nsCSSPropertyID aProperty, nsAString& aResult) const;
  bool AppendValueToString(nsCSSPropertyID aProperty, nsAString& aResult,
                           nsCSSValue::Serialization aValueSerialization,
                           bool* aIsTokenStream = nullptr) const;
  // Helper for ToString with strange semantics regarding aValue.
  void AppendPropertyAndValueToString(nsCSSPropertyID aProperty,
                                      nsAString& aResult,
                                      nsAutoString& aValue,
                                      bool aValueIsTokenStream) const;
  // helper for ToString that serializes a custom property declaration for
  // a variable with the specified name
  void AppendVariableAndValueToString(const nsAString& aName,
                                      nsAString& aResult) const;

  void GetImageLayerValue(nsCSSCompressedDataBlock *data,
                          nsAString& aValue,
                          nsCSSValue::Serialization aSerialization,
                          const nsCSSPropertyID aTable[]) const;

  void GetImageLayerPositionValue(nsCSSCompressedDataBlock *data,
                                  nsAString& aValue,
                                  nsCSSValue::Serialization aSerialization,
                                  const nsCSSPropertyID aTable[]) const;

public:
  /**
   * Returns the property at the given index in the ordered list of
   * declarations.  For custom properties, eCSSPropertyExtra_variable
   * is returned.
   */
  nsCSSPropertyID GetPropertyAt(uint32_t aIndex) const {
    uint32_t value = mOrder[aIndex];
    if (value >= eCSSProperty_COUNT) {
      return eCSSPropertyExtra_variable;
    }
    return nsCSSPropertyID(value);
  }

  /**
   * Gets the name of the custom property at the given index in the ordered
   * list of declarations.
   */
  void GetCustomPropertyNameAt(uint32_t aIndex, nsAString& aResult) const {
    MOZ_ASSERT(mOrder[aIndex] >= eCSSProperty_COUNT);
    uint32_t variableIndex = mOrder[aIndex] - eCSSProperty_COUNT;
    aResult.Truncate();
    aResult.AppendLiteral("--");
    aResult.Append(mVariableOrder[variableIndex]);
  }

  size_t SizeOfIncludingThis(mozilla::MallocSizeOf aMallocSizeOf) const;

private:
  // The order of properties in this declaration.  Longhand properties are
  // represented by their nsCSSPropertyID value, and each custom property (--*)
  // is represented by a value that begins at eCSSProperty_COUNT.
  //
  // Subtracting eCSSProperty_COUNT from those values that represent custom
  // properties results in an index into mVariableOrder, which identifies the
  // specific variable the custom property declaration is for.
  AutoTArray<uint32_t, 8> mOrder;

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

  friend class ImportantStyleData;
  ImportantStyleData mImportantStyleData;
};

inline ::mozilla::css::Declaration*
ImportantStyleData::Declaration()
{
  union {
    char* ch; /* for pointer arithmetic */
    ::mozilla::css::Declaration* declaration;
    ImportantStyleData* importantData;
  } u;
  u.importantData = this;
  u.ch -= offsetof(::mozilla::css::Declaration, mImportantStyleData);
  return u.declaration;
}

NS_DEFINE_STATIC_IID_ACCESSOR(Declaration, NS_CSS_DECLARATION_IMPL_CID)

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Declaration_h */

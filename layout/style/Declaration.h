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

#include "mozilla/Attributes.h"

// This header is in EXPORTS because it's used in several places in content/,
// but it's not really a public interface.
#ifndef _IMPL_NS_LAYOUT
#error "This file should only be included within the layout library"
#endif

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

  bool HasImportantData() const { return mImportantData != nullptr; }
  bool GetValueIsImportant(nsCSSProperty aProperty) const;
  bool GetValueIsImportant(const nsAString& aProperty) const;

  PRUint32 Count() const {
    return mOrder.Length();
  }
  void GetNthProperty(PRUint32 aIndex, nsAString& aReturn) const;

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
  }
  void MapImportantRuleInfoInto(nsRuleData *aRuleData) const {
    NS_ABORT_IF_FALSE(mData, "called while expanded");
    NS_ABORT_IF_FALSE(mImportantData, "must have important data");
    mImportantData->MapRuleInfoInto(aRuleData);
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
    mOrder.Clear();
  }

#ifdef DEBUG
  void List(FILE* out = stdout, PRInt32 aIndent = 0) const;
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

public:
  nsCSSProperty OrderValueAt(PRUint32 aValue) const {
    return nsCSSProperty(mOrder.ElementAt(aValue));
  }

  size_t SizeOfIncludingThis(nsMallocSizeOfFun aMallocSizeOf) const;

private:
  nsAutoTArray<PRUint8, 8> mOrder;

  // never null, except while expanded, or before the first call to
  // InitializeEmpty or CompressFrom.
  nsAutoPtr<nsCSSCompressedDataBlock> mData;

  // may be null
  nsAutoPtr<nsCSSCompressedDataBlock> mImportantData;

  // set by style rules when |RuleMatched| is called;
  // also by ToString (hence the 'mutable').
  mutable bool mImmutable;
};

} // namespace css
} // namespace mozilla

#endif /* mozilla_css_Declaration_h */

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * an object that stores the result of determining whether a style struct that
 * was computed can be cached in the rule tree, and if so, what the conditions
 * it relies on are
 */

#ifndef RuleNodeCacheConditions_h_
#define RuleNodeCacheConditions_h_

#include "mozilla/Attributes.h"
#include "nsCoord.h"
#include "nsTArray.h"

class nsStyleContext;

namespace mozilla {

/**
 * nsRuleNodeCacheConditions is used to store information about whether
 * we can store a style struct that we're computing in the rule tree.
 *
 * For inherited structs (i.e., structs with inherited properties), we
 * cache the struct in the rule tree if it does not depend on any data
 * in the style context tree, and otherwise store it in the style
 * context tree.  This means that for inherited structs, setting any
 * conditions is equivalent to making the struct uncacheable.
 *
 * For reset structs (i.e., structs with non-inherited properties), we
 * are also able to cache structs in the rule tree conditionally on
 * certain common conditions.  For these structs, setting conditions
 * (SetFontSizeDependency, SetWritingModeDependency) instead causes the
 * struct to be stored, with the condition, in the rule tree.
 */
class RuleNodeCacheConditions
{
public:
  RuleNodeCacheConditions()
    : mFontSize(0), mBits(0) {}
  RuleNodeCacheConditions(const RuleNodeCacheConditions& aOther)
    : mFontSize(aOther.mFontSize), mBits(aOther.mBits) {}
  RuleNodeCacheConditions& operator=(const RuleNodeCacheConditions& aOther)
  {
    mFontSize = aOther.mFontSize;
    mBits = aOther.mBits;
    return *this;
  }
  bool operator==(const RuleNodeCacheConditions& aOther) const
  {
    return mFontSize == aOther.mFontSize &&
           mBits == aOther.mBits;
  }
  bool operator!=(const RuleNodeCacheConditions& aOther) const
  {
    return !(*this == aOther);
  }

  bool Matches(nsStyleContext* aStyleContext) const;

  /**
   * Record that the data being computed depend on the font-size
   * property of the element for which they are being computed.
   *
   * Note that we sometimes actually call this when there is a
   * dependency on the font-size property of the parent element, but we
   * only do so while computing inherited structs (nsStyleFont), and we
   * only store reset structs conditionally.
   */
  void SetFontSizeDependency(nscoord aCoord)
  {
    MOZ_ASSERT(!(mBits & eHaveFontSize) || mFontSize == aCoord);
    mFontSize = aCoord;
    mBits |= eHaveFontSize;
  }

  /**
   * Record that the data being computed depend on the writing mode of
   * the element for which they are being computed, which in turn
   * depends on its 'writing-mode', 'direction', and 'text-orientation'
   * properties.
   */
  void SetWritingModeDependency(uint8_t aWritingMode)
  {
    MOZ_ASSERT(!(mBits & eHaveWritingMode) || GetWritingMode() == aWritingMode);
    mBits |= (static_cast<uint64_t>(aWritingMode) << eWritingModeShift) |
             eHaveWritingMode;
  }

  void SetUncacheable()
  {
    mBits |= eUncacheable;
  }

  bool Cacheable() const
  {
    return !(mBits & eUncacheable);
  }

  bool CacheableWithDependencies() const
  {
    return !(mBits & eUncacheable) &&
           (mBits & eHaveBitsMask) != 0;
  }

  bool CacheableWithoutDependencies() const
  {
    // We're not uncacheable and we have don't have a font-size or
    // writing mode value.
    return (mBits & eHaveBitsMask) == 0;
  }

#ifdef DEBUG
  void List() const;
#endif

private:
  enum {
    eUncacheable      = 0x0001,
    eHaveFontSize     = 0x0002,
    eHaveWritingMode  = 0x0004,
    eHaveBitsMask     = 0x00ff,
    eWritingModeMask  = 0xff00,
    eWritingModeShift = 8,
  };

  uint8_t GetWritingMode() const
  {
    return static_cast<uint8_t>(
        (mBits & eWritingModeMask) >> eWritingModeShift);
  }

  // The font size from which em units are derived.
  nscoord mFontSize;

  // Values in mBits:
  //   bit 0:      are we set to "uncacheable"?
  //   bit 1:      do we have a font size value?
  //   bit 2:      do we have a writing mode value?
  //   bits 3-7:   unused
  //   bits 8-15:  writing mode (uint8_t)
  //   bits 16-31: unused
  uint32_t mBits;
};

} // namespace mozilla

#endif // !defined(RuleNodeCacheConditions_h_)

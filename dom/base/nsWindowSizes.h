/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWindowSizes_h
#define nsWindowSizes_h

#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"
#include "mozilla/SizeOfState.h"

class nsTabSizes {
public:
  enum Kind {
      DOM,        // DOM stuff.
      Style,      // Style stuff.
      Other       // Everything else.
  };

  nsTabSizes()
    : mDom(0)
    , mStyle(0)
    , mOther(0)
  {
  }

  void add(Kind kind, size_t n)
  {
    switch (kind) {
      case DOM:   mDom   += n; break;
      case Style: mStyle += n; break;
      case Other: mOther += n; break;
      default:    MOZ_CRASH("bad nsTabSizes kind");
    }
  }

  size_t mDom;
  size_t mStyle;
  size_t mOther;
};

#define ZERO_SIZE(kind, mSize)         mSize(0),
#define ADD_TO_TAB_SIZES(kind, mSize)  aSizes->add(nsTabSizes::kind, mSize);
#define ADD_TO_TOTAL_SIZE(kind, mSize) total += mSize;
#define DECL_SIZE(kind, mSize)         size_t mSize;

#define NS_STYLE_SIZES_FIELD(name_) mStyle##name_

struct nsStyleSizes
{
  nsStyleSizes()
    :
      #define STYLE_STRUCT(name_) \
        NS_STYLE_SIZES_FIELD(name_)(0),
      #include "nsStyleStructList.h"
      #undef STYLE_STRUCT

      dummy()
  {}

  void addToTabSizes(nsTabSizes* aSizes) const
  {
    #define STYLE_STRUCT(name_) \
      aSizes->add(nsTabSizes::Style, NS_STYLE_SIZES_FIELD(name_));
    #include "nsStyleStructList.h"
    #undef STYLE_STRUCT
  }

  size_t getTotalSize() const
  {
    size_t total = 0;

    #define STYLE_STRUCT(name_) \
      total += NS_STYLE_SIZES_FIELD(name_);
    #include "nsStyleStructList.h"
    #undef STYLE_STRUCT

    return total;
  }

  #define STYLE_STRUCT(name_) \
    size_t NS_STYLE_SIZES_FIELD(name_);
  #include "nsStyleStructList.h"
  #undef STYLE_STRUCT

  // Present just to absorb the trailing comma in the constructor.
  int dummy;
};

#define NS_ARENA_SIZES_FIELD(classname) mArena##classname

struct nsArenaSizes {
#define FOR_EACH_SIZE(macro) \
  macro(Other, mLineBoxes) \
  macro(Style, mRuleNodes) \
  macro(Style, mComputedStyles)

  nsArenaSizes()
    :
      FOR_EACH_SIZE(ZERO_SIZE)

      #define FRAME_ID(classname, ...) \
        NS_ARENA_SIZES_FIELD(classname)(0),
      #define ABSTRACT_FRAME_ID(...)
      #include "nsFrameIdList.h"
      #undef FRAME_ID
      #undef ABSTRACT_FRAME_ID

      dummy()
  {}

  void addToTabSizes(nsTabSizes* aSizes) const
  {
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES)

    #define FRAME_ID(classname, ...) \
      aSizes->add(nsTabSizes::Other, NS_ARENA_SIZES_FIELD(classname));
    #define ABSTRACT_FRAME_ID(...)
    #include "nsFrameIdList.h"
    #undef FRAME_ID
    #undef ABSTRACT_FRAME_ID
  }

  size_t getTotalSize() const
  {
    size_t total = 0;

    FOR_EACH_SIZE(ADD_TO_TOTAL_SIZE)

    #define FRAME_ID(classname, ...) \
      total += NS_ARENA_SIZES_FIELD(classname);
    #define ABSTRACT_FRAME_ID(...)
    #include "nsFrameIdList.h"
    #undef FRAME_ID
    #undef ABSTRACT_FRAME_ID

    return total;
  }

  FOR_EACH_SIZE(DECL_SIZE)

  #define FRAME_ID(classname, ...) \
    size_t NS_ARENA_SIZES_FIELD(classname);
  #define ABSTRACT_FRAME_ID(...)
  #include "nsFrameIdList.h"
  #undef FRAME_ID
  #undef ABSTRACT_FRAME_ID

  // Present just to absorb the trailing comma in the constructor.
  int dummy;

#undef FOR_EACH_SIZE
};

class nsWindowSizes
{
#define FOR_EACH_SIZE(macro) \
  macro(DOM,   mDOMElementNodesSize) \
  macro(DOM,   mDOMTextNodesSize) \
  macro(DOM,   mDOMCDATANodesSize) \
  macro(DOM,   mDOMCommentNodesSize) \
  macro(DOM,   mDOMEventTargetsSize) \
  macro(DOM,   mDOMMediaQueryLists) \
  macro(DOM,   mDOMPerformanceUserEntries) \
  macro(DOM,   mDOMPerformanceResourceEntries) \
  macro(DOM,   mDOMOtherSize) \
  macro(Style, mLayoutStyleSheetsSize) \
  macro(Other, mLayoutPresShellSize) \
  macro(Style, mLayoutStyleSetsStylistRuleTree) \
  macro(Style, mLayoutStyleSetsStylistElementAndPseudosMaps) \
  macro(Style, mLayoutStyleSetsStylistInvalidationMap) \
  macro(Style, mLayoutStyleSetsStylistRevalidationSelectors) \
  macro(Style, mLayoutStyleSetsStylistOther) \
  macro(Style, mLayoutStyleSetsOther) \
  macro(Style, mLayoutElementDataObjects) \
  macro(Other, mLayoutTextRunsSize) \
  macro(Other, mLayoutPresContextSize) \
  macro(Other, mLayoutFramePropertiesSize) \
  macro(Style, mLayoutComputedValuesDom) \
  macro(Style, mLayoutComputedValuesNonDom) \
  macro(Style, mLayoutComputedValuesVisited) \
  macro(Style, mLayoutComputedValuesStale) \
  macro(Other, mPropertyTablesSize) \
  macro(Other, mBindingsSize) \

public:
  explicit nsWindowSizes(mozilla::SizeOfState& aState)
    :
      FOR_EACH_SIZE(ZERO_SIZE)
      mDOMEventTargetsCount(0),
      mDOMEventListenersCount(0),
      mArenaSizes(),
      mStyleSizes(),
      mState(aState)
  {}

  void addToTabSizes(nsTabSizes* aSizes) const {
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES)
    mArenaSizes.addToTabSizes(aSizes);
    mStyleSizes.addToTabSizes(aSizes);
  }

  size_t getTotalSize() const
  {
    size_t total = 0;

    FOR_EACH_SIZE(ADD_TO_TOTAL_SIZE)
    total += mArenaSizes.getTotalSize();
    total += mStyleSizes.getTotalSize();

    return total;
  }

  FOR_EACH_SIZE(DECL_SIZE);

  uint32_t mDOMEventTargetsCount;
  uint32_t mDOMEventListenersCount;

  nsArenaSizes mArenaSizes;

  nsStyleSizes mStyleSizes;

  mozilla::SizeOfState& mState;

#undef FOR_EACH_SIZE
};

#undef ZERO_SIZE
#undef ADD_TO_TAB_SIZES
#undef ADD_TO_TOTAL_SIZE
#undef DECL_SIZE

#endif // nsWindowSizes_h

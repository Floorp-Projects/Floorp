/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArenaMemoryStats_h
#define nsArenaMemoryStats_h

#include "mozilla/Assertions.h"
#include "mozilla/PodOperations.h"

class nsTabSizes {
public:
  enum Kind {
      DOM,        // DOM stuff.
      Style,      // Style stuff.
      Other       // Everything else.
  };

  nsTabSizes() { mozilla::PodZero(this); }

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

#define FRAME_ID_STAT_FIELD(classname) mArena##classname

struct nsArenaMemoryStats {
#define FOR_EACH_SIZE(macro) \
  macro(Other, mLineBoxes) \
  macro(Style, mRuleNodes) \
  macro(Style, mStyleContexts) \
  macro(Style, mStyleStructs) \
  macro(Other, mOther)

  nsArenaMemoryStats()
    :
      #define ZERO_SIZE(kind, mSize) mSize(0),
      FOR_EACH_SIZE(ZERO_SIZE)
      #undef ZERO_SIZE
      #define FRAME_ID(classname) FRAME_ID_STAT_FIELD(classname)(),
      #include "nsFrameIdList.h"
      #undef FRAME_ID
      dummy()
  {}

  void addToTabSizes(nsTabSizes *sizes) const
  {
    #define ADD_TO_TAB_SIZES(kind, mSize) sizes->add(nsTabSizes::kind, mSize);
    FOR_EACH_SIZE(ADD_TO_TAB_SIZES)
    #undef ADD_TO_TAB_SIZES
    #define FRAME_ID(classname) \
      sizes->add(nsTabSizes::Other, FRAME_ID_STAT_FIELD(classname));
    #include "nsFrameIdList.h"
    #undef FRAME_ID
  }

  size_t getTotalSize() const
  {
    size_t total = 0;
    #define ADD_TO_TOTAL_SIZE(kind, mSize) total += mSize;
    FOR_EACH_SIZE(ADD_TO_TOTAL_SIZE)
    #undef ADD_TO_TOTAL_SIZE
    #define FRAME_ID(classname) \
    total += FRAME_ID_STAT_FIELD(classname);
    #include "nsFrameIdList.h"
    #undef FRAME_ID
    return total;
  }

  #define DECL_SIZE(kind, mSize) size_t mSize;
  FOR_EACH_SIZE(DECL_SIZE)
  #undef DECL_SIZE
  #define FRAME_ID(classname) size_t FRAME_ID_STAT_FIELD(classname);
  #include "nsFrameIdList.h"
  #undef FRAME_ID
  int dummy;  // present just to absorb the trailing comma from FRAME_ID in the
              // constructor

#undef FOR_EACH_SIZE
};

#endif // nsArenaMemoryStats_h

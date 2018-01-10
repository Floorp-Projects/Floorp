/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_CachedAnonBoxStyles_h
#define mozilla_CachedAnonBoxStyles_h

#include "nsAtom.h"
#include "nsTArray.h"

class nsWindowSizes;

namespace mozilla {

class ServoStyleContext;

// Cache of anonymous box styles that inherit from a given style.
//
// To minimize memory footprint, the cache is word-sized with a tagged pointer
// If there is only one entry, it's stored inline. If there are more, they're
// stored in an out-of-line buffer. See bug 1429126 comment 0 and comment 1 for
// the measurements and rationale that influenced the design.
class CachedAnonBoxStyles
{
public:
  void Insert(ServoStyleContext* aStyle);
  ServoStyleContext* Lookup(nsAtom* aAnonBox) const;

  CachedAnonBoxStyles() : mBits(0) {}
  ~CachedAnonBoxStyles()
  {
    if (IsIndirect()) {
      delete AsIndirect();
    } else if (!IsEmpty()) {
      RefPtr<ServoStyleContext> ref = dont_AddRef(AsDirect());
    }
  }

  void AddSizeOfIncludingThis(nsWindowSizes& aSizes, size_t* aCVsSize) const;

private:
  // See bug 1429126 comment 1 for the choice of four here.
  typedef AutoTArray<RefPtr<ServoStyleContext>, 4> IndirectCache;

  bool IsEmpty() const { return !mBits; }
  bool IsIndirect() const { return (mBits & 1); }

  ServoStyleContext* AsDirect() const
  {
    MOZ_ASSERT(!IsIndirect());
    return reinterpret_cast<ServoStyleContext*>(mBits);
  }

  IndirectCache* AsIndirect() const
  {
    MOZ_ASSERT(IsIndirect());
    return reinterpret_cast<IndirectCache*>(mBits & ~1);
  }

  uintptr_t mBits;
};

} // namespace mozilla

#endif // mozilla_CachedAnonBoxStyles_h

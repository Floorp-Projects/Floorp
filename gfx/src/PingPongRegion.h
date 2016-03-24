/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PingPongRegion_h__
#define PingPongRegion_h__

/* This class uses a pair of regions and swaps between them while
 * accumulating to avoid the heap allocations associated with
 * modifying a region in place.
 *
 * It is sizeof(T)*2 + sizeof(T*) and can use end up using
 * approximately double the amount of memory as using single
 * region so use it sparingly.
 */

template <typename T>
class PingPongRegion
{
  typedef typename T::RectType RectType;
public:
  PingPongRegion()
  {
    rgn = &rgn1;
  }

  void SubOut(const RectType& aOther)
  {
    T* nextRgn = nextRegion();
    nextRgn->Sub(*rgn, aOther);
    rgn = nextRgn;
  }

  void OrWith(const RectType& aOther)
  {
    T* nextRgn = nextRegion();
    nextRgn->Or(*rgn, aOther);
    rgn = nextRgn;
  }

  T& Region()
  {
    return *rgn;
  }

private:

  T* nextRegion()
  {
    if (rgn == &rgn1) {
      return &rgn2;
    } else {
      return &rgn1;
    }
  }

  T* rgn;
  T rgn1;
  T rgn2;
};

#endif

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_AnimationComparator_h
#define mozilla_AnimationComparator_h

namespace mozilla {

// Although this file is called AnimationComparator, we don't actually
// implement AnimationComparator (to compare const Animation& parameters)
// since it's not actually needed (yet).

template<typename AnimationPtrType>
class AnimationPtrComparator {
public:
  bool Equals(const AnimationPtrType& a, const AnimationPtrType& b) const
  {
    return a == b;
  }

  bool LessThan(const AnimationPtrType& a, const AnimationPtrType& b) const
  {
    return a->HasLowerCompositeOrderThan(*b);
  }
};

} // namespace mozilla

#endif // mozilla_AnimationComparator_h

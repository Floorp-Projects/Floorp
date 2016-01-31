/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_TextRange_inl_h__
#define mozilla_a11y_TextRange_inl_h__

#include "TextRange.h"
#include "HyperTextAccessible.h"

namespace mozilla {
namespace a11y {

inline Accessible*
TextRange::Container() const
{
  uint32_t pos1 = 0, pos2 = 0;
  nsAutoTArray<Accessible*, 30> parents1, parents2;
  return CommonParent(mStartContainer, mEndContainer,
                      &parents1, &pos1, &parents2, &pos2);
}


} // namespace a11y
} // namespace mozilla

#endif

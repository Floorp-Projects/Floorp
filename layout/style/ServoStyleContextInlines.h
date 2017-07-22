/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ContextInlines_h
#define mozilla_ContextInlines_h


#include "nsStyleStruct.h"
#include "ServoBindings.h"
#include "mozilla/ServoStyleContext.h"

namespace mozilla {

void
ServoStyleContext::ResolveSameStructsAs(const ServoStyleContext* aOther)
{
  // Only resolve structs that are not already resolved in this struct.
  uint64_t ourBits = mBits & NS_STYLE_INHERIT_MASK;
  uint64_t otherBits = aOther->mBits & NS_STYLE_INHERIT_MASK;
  uint64_t newBits = otherBits & ~ourBits & NS_STYLE_INHERIT_MASK;

#define STYLE_STRUCT(name_, checkdata_cb)                                           \
  if (nsStyle##name_::kHasFinishStyle && newBits & NS_STYLE_INHERIT_BIT(name_)) {   \
    const nsStyle##name_* data = ComputedValues()->GetStyle##name_();               \
    const_cast<nsStyle##name_*>(data)->FinishStyle(mPresContext);                   \
  }
#include "nsStyleStructList.h"
#undef STYLE_STRUCT

  mBits |= newBits;
}

} // namespace mozilla

#endif // mozilla_ContextInlines_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants for frame state bits and a type to store them in a uint64_t */

#ifndef nsFrameState_h_
#define nsFrameState_h_

#include <stdint.h>

#ifdef DEBUG
#include "nsString.h"

class nsIFrame;
#endif

typedef uint64_t nsFrameState_size_t;

#define NS_FRAME_STATE_BIT(n_) (nsFrameState(nsFrameState_size_t(1) << (n_)))

enum nsFrameState : nsFrameState_size_t {
#define FRAME_STATE_BIT(group_, value_, name_) \
  name_ = NS_FRAME_STATE_BIT(value_),
#include "nsFrameStateBits.h"
#undef FRAME_STATE_BIT
};

inline nsFrameState operator|(nsFrameState aLeft, nsFrameState aRight)
{
  return nsFrameState(nsFrameState_size_t(aLeft) | nsFrameState_size_t(aRight));
}

inline nsFrameState operator&(nsFrameState aLeft, nsFrameState aRight)
{
  return nsFrameState(nsFrameState_size_t(aLeft) & nsFrameState_size_t(aRight));
}

inline nsFrameState& operator|=(nsFrameState& aLeft, nsFrameState aRight)
{
  aLeft = aLeft | aRight;
  return aLeft;
}

inline nsFrameState& operator&=(nsFrameState& aLeft, nsFrameState aRight)
{
  aLeft = aLeft & aRight;
  return aLeft;
}

inline nsFrameState operator~(nsFrameState aRight)
{
  return nsFrameState(~nsFrameState_size_t(aRight));
}

inline nsFrameState operator^(nsFrameState aLeft, nsFrameState aRight)
{
  return nsFrameState(nsFrameState_size_t(aLeft) ^ nsFrameState_size_t(aRight));
}

inline nsFrameState& operator^=(nsFrameState& aLeft, nsFrameState aRight)
{
  aLeft = aLeft ^ aRight;
  return aLeft;
}

// Bits 20-31 and 60-63 of the frame state are reserved for implementations.
#define NS_FRAME_IMPL_RESERVED                      nsFrameState(0xF0000000FFF00000)
#define NS_FRAME_RESERVED                           ~NS_FRAME_IMPL_RESERVED

namespace mozilla {
#ifdef DEBUG
nsCString GetFrameState(nsIFrame* aFrame);
void PrintFrameState(nsIFrame* aFrame);
#endif
} // namespace mozilla

#endif /* nsFrameState_h_ */ 

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

#if (_MSC_VER == 1600)
/*
 * Visual Studio 2010 has trouble with the sized enum.  Although sized enums
 * are supported, two problems arise:
 *
 *   1. Implicit conversions from the enum type to the equivalently sized
 *      integer size are not performed, leading to many compile errors.
 *   2. Even with explicit casts added to avoid the errors from (1), odd
 *      test failures result, which might well be due to a compiler bug.
 *
 * So with VS2010 we use consts for the state bits and forgo the increased
 * type safety of the enum.  (Visual Studio 2012 has no problem with
 * nsFrameState being a sized enum, however.)
 */

typedef nsFrameState_size_t nsFrameState;

#define FRAME_STATE_BIT(group_, value_, name_) \
const nsFrameState name_ = NS_FRAME_STATE_BIT(value_);
#include "nsFrameStateBits.h"
#undef FRAME_STATE_BIT

#else

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

#endif

// Bits 20-31 and 60-63 of the frame state are reserved for implementations.
#define NS_FRAME_IMPL_RESERVED                      nsFrameState(0xF0000000FFF00000)
#define NS_FRAME_RESERVED                           ~NS_FRAME_IMPL_RESERVED

namespace mozilla {
#ifdef DEBUG
nsCString GetFrameState(nsIFrame* aFrame);
void PrintFrameState(nsIFrame* aFrame);
#endif
}

#endif /* nsFrameState_h_ */ 

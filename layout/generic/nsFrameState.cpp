/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* constants for frame state bits and a type to store them in a uint64_t */

#include "nsFrameState.h"

#include "nsBlockFrame.h"
#include "nsBoxFrame.h"
#include "nsFlexContainerFrame.h"
#include "nsGridContainerFrame.h"
#include "nsGfxScrollFrame.h"
#include "nsIFrame.h"
#include "nsImageFrame.h"
#include "nsInlineFrame.h"
#include "nsPageFrame.h"
#include "nsPlaceholderFrame.h"
#include "nsRubyTextFrame.h"
#include "nsRubyTextContainerFrame.h"
#include "nsTableCellFrame.h"
#include "nsTableRowFrame.h"
#include "nsTableRowGroupFrame.h"
#include "nsTextFrame.h"
#include "mozilla/ISVGDisplayableFrame.h"
#include "mozilla/SVGContainerFrame.h"

namespace mozilla {

#ifdef DEBUG
nsCString GetFrameState(nsIFrame* aFrame) {
  nsCString result;
  AutoTArray<const char*, 3> groups;

  nsFrameState state = aFrame->GetStateBits();

  if (state == nsFrameState(0)) {
    result.Assign('0');
    return result;
  }

#  define FRAME_STATE_GROUP_CLASS(name_, class_)                        \
    {                                                                   \
      class_* frame = do_QueryFrame(aFrame);                            \
      if (frame &&                                                      \
          (groups.IsEmpty() || strcmp(groups.LastElement(), #name_))) { \
        groups.AppendElement(#name_);                                   \
      }                                                                 \
    }
#  define FRAME_STATE_BIT(group_, value_, name_)                            \
    if ((state & NS_FRAME_STATE_BIT(value_)) && groups.Contains(#group_)) { \
      if (!result.IsEmpty()) {                                              \
        result.InsertLiteral(" | ", 0);                                     \
      }                                                                     \
      result.InsertLiteral(#name_, 0);                                      \
      state = state & ~NS_FRAME_STATE_BIT(value_);                          \
    }
#  include "nsFrameStateBits.h"
#  undef FRAME_STATE_GROUP_CLASS
#  undef FRAME_STATE_BIT

  if (state) {
    result.AppendPrintf(" | 0x%0" PRIx64, static_cast<uint64_t>(state));
  }

  return result;
}

void PrintFrameState(nsIFrame* aFrame) {
  printf("%s\n", GetFrameState(aFrame).get());
}

enum class FrameStateGroupId {
#  define FRAME_STATE_GROUP_NAME(name_) name_,
#  include "nsFrameStateBits.h"
#  undef FRAME_STATE_GROUP_NAME

  LENGTH
};

void DebugVerifyFrameStateBits() {
  // Build an array of all of the bits used by each group.  While
  // building this we assert that a bit isn't used multiple times within
  // the same group.
  nsFrameState bitsUsedPerGroup[size_t(FrameStateGroupId::LENGTH)] = {
      nsFrameState(0)};

#  define FRAME_STATE_BIT(group_, value_, name_)                           \
    {                                                                      \
      auto bit = NS_FRAME_STATE_BIT(value_);                               \
      size_t group = size_t(FrameStateGroupId::group_);                    \
      MOZ_ASSERT(!(bitsUsedPerGroup[group] & bit), #name_                  \
                 " must not use a bit already declared within its group"); \
      bitsUsedPerGroup[group] |= bit;                                      \
    }

#  include "nsFrameStateBits.h"
#  undef FRAME_STATE_BIT

  // FIXME: Can we somehow check across the groups as well???  In other
  // words, find the pairs of groups that could be used on the same
  // frame (Generic paired with everything else, and a few other pairs),
  // and check that we don't have bits in common between those pairs.
}

#endif

}  // namespace mozilla

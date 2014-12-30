/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* states and methods used while laying out a ruby segment */

#ifndef mozilla_RubyReflowState_h_
#define mozilla_RubyReflowState_h_

#include "mozilla/Attributes.h"
#include "WritingModes.h"
#include "nsTArray.h"

#define RTC_ARRAY_SIZE 1

class nsRubyTextContainerFrame;

namespace mozilla {

class MOZ_STACK_CLASS RubyReflowState MOZ_FINAL
{
public:
  explicit RubyReflowState(
    WritingMode aLineWM,
    const nsTArray<nsRubyTextContainerFrame*>& aTextContainers);

  struct TextContainerInfo
  {
    nsRubyTextContainerFrame* mFrame;
    LogicalSize mLineSize;

    TextContainerInfo(WritingMode aLineWM, nsRubyTextContainerFrame* aFrame)
      : mFrame(aFrame)
      , mLineSize(aLineWM) { }
  };

  void AdvanceCurrentContainerIndex() { mCurrentContainerIndex++; }

  void SetTextContainerInfo(int32_t aIndex,
                            nsRubyTextContainerFrame* aContainer,
                            const LogicalSize& aLineSize)
  {
    MOZ_ASSERT(mTextContainers[aIndex].mFrame == aContainer);
    mTextContainers[aIndex].mLineSize = aLineSize;
  }

  const TextContainerInfo&
    GetCurrentTextContainerInfo(nsRubyTextContainerFrame* aFrame) const
  {
    MOZ_ASSERT(mTextContainers[mCurrentContainerIndex].mFrame == aFrame);
    return mTextContainers[mCurrentContainerIndex];
  }

private:
  static MOZ_CONSTEXPR_VAR int32_t kBaseContainerIndex = -1;
  // The index of the current reflowing container. When it equals to
  // kBaseContainerIndex, we are reflowing ruby base. Otherwise, it
  // stands for the index of text containers in the ruby segment.
  int32_t mCurrentContainerIndex;

  nsAutoTArray<TextContainerInfo, RTC_ARRAY_SIZE> mTextContainers;
};

}

#endif // mozilla_RubyReflowState_h_

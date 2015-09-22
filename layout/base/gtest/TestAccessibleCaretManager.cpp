/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <string>

#include "AccessibleCaret.h"
#include "AccessibleCaretManager.h"

using ::testing::DefaultValue;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::_;

// -----------------------------------------------------------------------------
// This file tests CaretStateChanged events and the appearance of the two
// AccessibleCarets manipulated by AccessibleCaretManager.

namespace mozilla
{
using dom::CaretChangedReason;

class AccessibleCaretManagerTester : public ::testing::Test
{
public:
  class MockAccessibleCaret : public AccessibleCaret
  {
  public:
    MockAccessibleCaret() : AccessibleCaret(nullptr) {}

    virtual void SetAppearance(Appearance aAppearance) override
    {
      // A simplified version without touching CaretElement().
      mAppearance = aAppearance;
    }

    virtual void SetSelectionBarEnabled(bool aEnabled) override
    {
      // A simplified version without touching CaretElement().
      mSelectionBarEnabled = aEnabled;
    }

    MOCK_METHOD2(SetPosition,
                 PositionChangedResult(nsIFrame* aFrame, int32_t aOffset));

  }; // class MockAccessibleCaret

  class MockAccessibleCaretManager : public AccessibleCaretManager
  {
  public:
    using CaretMode = AccessibleCaretManager::CaretMode;
    using AccessibleCaretManager::UpdateCarets;

    MockAccessibleCaretManager()
      : AccessibleCaretManager(nullptr)
    {
      mFirstCaret = MakeUnique<MockAccessibleCaret>();
      mSecondCaret = MakeUnique<MockAccessibleCaret>();
    }

    MockAccessibleCaret& FirstCaret()
    {
      return static_cast<MockAccessibleCaret&>(*mFirstCaret);
    }

    MockAccessibleCaret& SecondCaret()
    {
      return static_cast<MockAccessibleCaret&>(*mSecondCaret);
    }

    virtual bool CompareTreePosition(nsIFrame* aStartFrame,
                                     nsIFrame* aEndFrame) const override
    {
      return true;
    }

    virtual bool IsCaretDisplayableInCursorMode(
      nsIFrame** aOutFrame = nullptr, int32_t* aOutOffset = nullptr) const override
    {
      return true;
    }

    virtual void UpdateCaretsForTilt() override {}

    MOCK_CONST_METHOD0(GetCaretMode, CaretMode());
    MOCK_CONST_METHOD1(DispatchCaretStateChangedEvent,
                       void(CaretChangedReason aReason));
    MOCK_CONST_METHOD1(HasNonEmptyTextContent, bool(nsINode* aNode));

  }; // class MockAccessibleCaretManager

  using Appearance = AccessibleCaret::Appearance;
  using PositionChangedResult = AccessibleCaret::PositionChangedResult;
  using CaretMode = MockAccessibleCaretManager::CaretMode;

  AccessibleCaretManagerTester()
  {
    DefaultValue<CaretMode>::Set(CaretMode::None);
    DefaultValue<PositionChangedResult>::Set(PositionChangedResult::NotChanged);

    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
      .WillRepeatedly(Return(PositionChangedResult::Changed));

    EXPECT_CALL(mManager.SecondCaret(), SetPosition(_, _))
      .WillRepeatedly(Return(PositionChangedResult::Changed));
  }

  AccessibleCaret::Appearance FirstCaretAppearance()
  {
    return mManager.FirstCaret().GetAppearance();
  }

  AccessibleCaret::Appearance SecondCaretAppearance()
  {
    return mManager.SecondCaret().GetAppearance();
  }

  // Member variables
  MockAccessibleCaretManager mManager;

}; // class AccessibleCaretManagerTester

TEST_F(AccessibleCaretManagerTester, TestUpdatesInSelectionMode)
{
  EXPECT_CALL(mManager, GetCaretMode())
    .WillRepeatedly(Return(CaretMode::Selection));

  EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                CaretChangedReason::Updateposition)).Times(3);

  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);
}

TEST_F(AccessibleCaretManagerTester, TestSingleTapOnNonEmptyInput)
{
  EXPECT_CALL(mManager, GetCaretMode())
    .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
    .WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Visibilitychange)).Times(1);
    EXPECT_CALL(check, Call("mouse down"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("reflow"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("blur"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("mouse up"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("reflow2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
  }

  // Simulate a single tap on a non-empty input.
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("update");

  mManager.OnSelectionChanged(nullptr, nullptr,
                              nsISelectionListener::DRAG_REASON |
                              nsISelectionListener::MOUSEDOWN_REASON);
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("mouse down");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("reflow");

  mManager.OnBlur();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("blur");

  mManager.OnSelectionChanged(nullptr, nullptr,
                              nsISelectionListener::MOUSEUP_REASON);
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("mouse up");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("reflow2");

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
}

TEST_F(AccessibleCaretManagerTester, TestSingleTapOnEmptyInput)
{
  EXPECT_CALL(mManager, GetCaretMode())
    .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
    .WillRepeatedly(Return(false));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Visibilitychange)).Times(1);
    EXPECT_CALL(check, Call("mouse down"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("reflow"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("blur"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("mouse up"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("reflow2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
  }

  // Simulate a single tap on an empty input.
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("update");

  mManager.OnSelectionChanged(nullptr, nullptr,
                              nsISelectionListener::DRAG_REASON |
                              nsISelectionListener::MOUSEDOWN_REASON);
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("mouse down");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("reflow");

  mManager.OnBlur();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("blur");

  mManager.OnSelectionChanged(nullptr, nullptr,
                              nsISelectionListener::MOUSEUP_REASON);
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("mouse up");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("reflow2");

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
}

TEST_F(AccessibleCaretManagerTester, TestTypingAtEndOfInput)
{
  EXPECT_CALL(mManager, GetCaretMode())
    .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
    .WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Updateposition)).Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                  CaretChangedReason::Visibilitychange)).Times(1);
    EXPECT_CALL(check, Call("keyboard"));

    // No CaretStateChanged events should be dispatched since the caret has
    // being hidden in cursor mode.
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
  }

  // Simulate typing the end of the input.
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("update");

  mManager.OnKeyboardEvent();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("keyboard");

  mManager.OnSelectionChanged(nullptr, nullptr,
                              nsISelectionListener::NO_REASON);
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
}

} // namespace mozilla

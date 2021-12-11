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
#include "mozilla/Preferences.h"

using ::testing::_;
using ::testing::DefaultValue;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::MockFunction;
using ::testing::Return;

// -----------------------------------------------------------------------------
// This file tests CaretStateChanged events and the appearance of the two
// AccessibleCarets manipulated by AccessibleCaretManager.

namespace mozilla {
using dom::CaretChangedReason;

class MOZ_RAII AutoRestoreBoolPref final {
 public:
  AutoRestoreBoolPref(const char* aPref, bool aValue) : mPref(aPref) {
    Preferences::GetBool(mPref, &mOldValue);
    Preferences::SetBool(mPref, aValue);
  }

  ~AutoRestoreBoolPref() { Preferences::SetBool(mPref, mOldValue); }

 private:
  const char* mPref = nullptr;
  bool mOldValue = false;
};

class AccessibleCaretManagerTester : public ::testing::Test {
 public:
  class MockAccessibleCaret : public AccessibleCaret {
   public:
    MockAccessibleCaret() : AccessibleCaret(nullptr) {}

    void SetAppearance(Appearance aAppearance) override {
      // A simplified version without touching CaretElement().
      mAppearance = aAppearance;
    }

    MOCK_METHOD2(SetPosition,
                 PositionChangedResult(nsIFrame* aFrame, int32_t aOffset));

  };  // class MockAccessibleCaret

  class MockAccessibleCaretManager : public AccessibleCaretManager {
   public:
    using CaretMode = AccessibleCaretManager::CaretMode;
    using AccessibleCaretManager::HideCaretsAndDispatchCaretStateChangedEvent;
    using AccessibleCaretManager::UpdateCarets;

    MockAccessibleCaretManager()
        : AccessibleCaretManager(nullptr,
                                 Carets{MakeUnique<MockAccessibleCaret>(),
                                        MakeUnique<MockAccessibleCaret>()}) {}

    MockAccessibleCaret& FirstCaret() {
      return static_cast<MockAccessibleCaret&>(*mCarets.GetFirst());
    }

    MockAccessibleCaret& SecondCaret() {
      return static_cast<MockAccessibleCaret&>(*mCarets.GetSecond());
    }

    bool CompareTreePosition(nsIFrame* aStartFrame,
                             nsIFrame* aEndFrame) const override {
      return true;
    }

    bool IsCaretDisplayableInCursorMode(
        nsIFrame** aOutFrame = nullptr,
        int32_t* aOutOffset = nullptr) const override {
      return true;
    }

    bool UpdateCaretsForOverlappingTilt() override { return true; }

    void UpdateCaretsForAlwaysTilt(const nsIFrame* aStartFrame,
                                   const nsIFrame* aEndFrame) override {
      if (mCarets.GetFirst()->IsVisuallyVisible()) {
        mCarets.GetFirst()->SetAppearance(Appearance::Left);
      }
      if (mCarets.GetSecond()->IsVisuallyVisible()) {
        mCarets.GetSecond()->SetAppearance(Appearance::Right);
      }
    }

    Terminated IsTerminated() const override { return Terminated::No; }
    bool IsScrollStarted() const { return mIsScrollStarted; }

    Terminated MaybeFlushLayout() override { return Terminated::No; }

    MOCK_CONST_METHOD0(GetCaretMode, CaretMode());
    MOCK_METHOD1(DispatchCaretStateChangedEvent,
                 void(CaretChangedReason aReason));
    MOCK_CONST_METHOD1(HasNonEmptyTextContent, bool(nsINode* aNode));

  };  // class MockAccessibleCaretManager

  using Appearance = AccessibleCaret::Appearance;
  using PositionChangedResult = AccessibleCaret::PositionChangedResult;
  using CaretMode = MockAccessibleCaretManager::CaretMode;

  AccessibleCaretManagerTester() {
    DefaultValue<CaretMode>::Set(CaretMode::None);
    DefaultValue<PositionChangedResult>::Set(PositionChangedResult::NotChanged);

    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Position));

    EXPECT_CALL(mManager.SecondCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Position));
  }

  AccessibleCaret::Appearance FirstCaretAppearance() {
    return mManager.FirstCaret().GetAppearance();
  }

  AccessibleCaret::Appearance SecondCaretAppearance() {
    return mManager.SecondCaret().GetAppearance();
  }

  // Member variables
  MockAccessibleCaretManager mManager;

};  // class AccessibleCaretManagerTester

TEST_F(AccessibleCaretManagerTester, TestUpdatesInSelectionMode)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Set default preference.
  AutoRestoreBoolPref savedPref("layout.accessiblecaret.always_tilt", false);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Selection));

  EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                            CaretChangedReason::Updateposition))
      .Times(3);

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
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_)).WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Visibilitychange))
        .Times(1);
    EXPECT_CALL(check, Call("mouse down"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("reflow"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("blur"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("mouse up"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("reflow2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
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
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Set default preference.
  AutoRestoreBoolPref savedPref(
      "layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content",
      false);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
      .WillRepeatedly(Return(false));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Visibilitychange))
        .Times(1);
    EXPECT_CALL(check, Call("mouse down"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("reflow"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("blur"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("mouse up"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("reflow2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
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
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_)).WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("update"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Visibilitychange))
        .Times(1);
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

TEST_F(AccessibleCaretManagerTester, TestScrollInSelectionMode)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Set default preference.
  AutoRestoreBoolPref savedPref("layout.accessiblecaret.always_tilt", false);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Selection));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    // Initially, first caret is out of scrollport, and second caret is visible.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("updatecarets"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart1"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("reflow1"));

    // After scroll ended, first caret is visible and second caret is out of
    // scroll port.
    EXPECT_CALL(mManager.SecondCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend1"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("reflow2"));

    // After the scroll ended, both carets are visible.
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend2"));
  }

  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);
  check.Call("updatecarets");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);
  check.Call("scrollstart1");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);
  check.Call("reflow1");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend1");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollstart2");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("reflow2");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Normal);
  check.Call("scrollend2");
}

TEST_F(AccessibleCaretManagerTester,
       TestScrollInSelectionModeWithAlwaysTiltPref)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Simulate Firefox Android preference.
  AutoRestoreBoolPref savedPref("layout.accessiblecaret.always_tilt", true);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Selection));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    // Initially, first caret is out of scrollport, and second caret is visible.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("updatecarets"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart1"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("scrollPositionChanged1"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("reflow1"));

    // After scroll ended, first caret is visible and second caret is out of
    // scroll port.
    EXPECT_CALL(mManager.SecondCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend1"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(_)).Times(0);
    EXPECT_CALL(check, Call("scrollPositionChanged2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("reflow2"));

    // After the scroll ended, both carets are visible.
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend2"));
  }

  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Right);
  check.Call("updatecarets");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Right);
  check.Call("scrollstart1");

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Right);
  check.Call("scrollPositionChanged1");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Right);
  check.Call("reflow1");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Left);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend1");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Left);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollstart2");

  mManager.OnScrollPositionChanged();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Left);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollPositionChanged2");

  mManager.OnReflow();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Left);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::NormalNotShown);
  check.Call("reflow2");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Left);
  EXPECT_EQ(SecondCaretAppearance(), Appearance::Right);
  check.Call("scrollend2");
}

TEST_F(AccessibleCaretManagerTester, TestScrollInCursorModeWhenLogicallyVisible)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_)).WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("updatecarets"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll))
        .Times(1);
    EXPECT_CALL(check, Call("scrollstart1"));

    // After scroll ended, the caret is out of scroll port.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Invisible));
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("scrollend1"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll))
        .Times(1);
    EXPECT_CALL(check, Call("scrollstart2"));

    // After scroll ended, the caret is visible again.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Position));
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("scrollend2"));
  }

  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("updatecarets");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("scrollstart1");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend1");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollstart2");

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("scrollend2");
}

TEST_F(AccessibleCaretManagerTester, TestScrollInCursorModeWhenHidden)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_)).WillRepeatedly(Return(true));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition))
        .Times(1);
    EXPECT_CALL(check, Call("updatecarets"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Visibilitychange))
        .Times(1);
    EXPECT_CALL(check, Call("hidecarets"));

    // After scroll ended, the caret is out of scroll port.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Invisible));
    EXPECT_CALL(check, Call("scrollend1"));

    // After scroll ended, the caret is visible again.
    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillRepeatedly(Return(PositionChangedResult::Position));
    EXPECT_CALL(check, Call("scrollend2"));
  }

  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("updatecarets");

  mManager.HideCaretsAndDispatchCaretStateChangedEvent();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("hidecarets");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("scrollend1");

  mManager.OnScrollStart();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);

  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("scrollend2");
}

TEST_F(AccessibleCaretManagerTester, TestScrollInCursorModeOnEmptyContent)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Set default preference.
  AutoRestoreBoolPref savedPref(
      "layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content",
      false);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
      .WillRepeatedly(Return(false));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("updatecarets"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart1"));

    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend1"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend2"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("scrollstart3"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("scrollend3"));
  }

  // Simulate a pinch-zoom operation before tapping on an empty content.
  mManager.OnScrollStart();
  mManager.OnScrollEnd();
  EXPECT_EQ(mManager.IsScrollStarted(), false);

  // Simulate a single tap on an empty content.
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("updatecarets");

  // Scroll the caret to be out of the viewport.
  mManager.OnScrollStart();
  check.Call("scrollstart1");
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend1");

  // Scroll the caret into the viewport.
  mManager.OnScrollStart();
  check.Call("scrollstart2");
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend2");

  // Scroll the caret within the viewport.
  mManager.OnScrollStart();
  check.Call("scrollstart3");
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("scrollend3");
}

TEST_F(AccessibleCaretManagerTester,
       TestScrollInCursorModeWithCaretShownWhenLongTappingOnEmptyContentPref)
MOZ_CAN_RUN_SCRIPT_FOR_DEFINITION {
  // Simulate Firefox Android preference.
  AutoRestoreBoolPref savedPref(
      "layout.accessiblecaret.caret_shown_when_long_tapping_on_empty_content",
      true);

  EXPECT_CALL(mManager, GetCaretMode())
      .WillRepeatedly(Return(CaretMode::Cursor));

  EXPECT_CALL(mManager, HasNonEmptyTextContent(_))
      .WillRepeatedly(Return(false));

  MockFunction<void(std::string aCheckPointName)> check;
  {
    InSequence dummy;

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("singletap updatecarets"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("longtap updatecarets"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("longtap scrollstart1"));

    EXPECT_CALL(mManager.FirstCaret(), SetPosition(_, _))
        .WillOnce(Return(PositionChangedResult::Invisible));
    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("longtap scrollend1"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("longtap scrollstart2"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("longtap scrollend2"));

    EXPECT_CALL(mManager,
                DispatchCaretStateChangedEvent(CaretChangedReason::Scroll));
    EXPECT_CALL(check, Call("longtap scrollstart3"));

    EXPECT_CALL(mManager, DispatchCaretStateChangedEvent(
                              CaretChangedReason::Updateposition));
    EXPECT_CALL(check, Call("longtap scrollend3"));
  }

  // Simulate a single tap on an empty input.
  mManager.FirstCaret().SetAppearance(Appearance::None);
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);
  check.Call("singletap updatecarets");

  // Scroll the caret within the viewport.
  mManager.OnScrollStart();
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::None);

  // Simulate a long tap on an empty input.
  mManager.FirstCaret().SetAppearance(Appearance::Normal);
  mManager.UpdateCarets();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("longtap updatecarets");

  // Scroll the caret to be out of the viewport.
  mManager.OnScrollStart();
  check.Call("longtap scrollstart1");
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::NormalNotShown);
  check.Call("longtap scrollend1");

  // Scroll the caret into the viewport.
  mManager.OnScrollStart();
  check.Call("longtap scrollstart2");
  mManager.OnScrollPositionChanged();
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("longtap scrollend2");

  // Scroll the caret within the viewport.
  mManager.OnScrollStart();
  check.Call("longtap scrollstart3");
  mManager.OnScrollPositionChanged();
  mManager.OnScrollEnd();
  EXPECT_EQ(FirstCaretAppearance(), Appearance::Normal);
  check.Call("longtap scrollend3");
}

}  // namespace mozilla

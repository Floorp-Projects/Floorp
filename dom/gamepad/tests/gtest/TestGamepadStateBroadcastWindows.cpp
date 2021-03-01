/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadStateBroadcaster.h"
#include "mozilla/dom/GamepadStateReceiver.h"
#include "mozilla/dom/GamepadStateBroadcastReceiverInfo.h"
#include "mozilla/dom/GamepadEventTypes.h"
#include <array>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "gtest/gtest.h"

#define MYASSERT(x)                                   \
  do {                                                \
    if (!(x)) {                                       \
      ADD_FAILURE() << "MYASSERT FAILED: `" #x "`\n"; \
      abort();                                        \
    }                                                 \
  } while (false)

namespace mozilla::dom {

// This class has friend access to GamepadHandle and can assist in creating
// them
class GamepadTestHelper {
 public:
  static void SendTestCommand(GamepadStateBroadcaster* p, uint32_t commandId) {
    p->SendTestCommand(commandId);
  }
  static bool StartMonitoringThreadForTesting(
      GamepadStateReceiver* p,
      std::function<void(const GamepadChangeEvent&)> aMonitorFn,
      std::function<void(uint32_t)> aTestCommandFn) {
    return p->StartMonitoringThreadForTesting(aMonitorFn, aTestCommandFn);
  }
  static GamepadHandle GetTestHandle(uint32_t value) {
    return GamepadHandle(value, GamepadHandleKind::GamepadPlatformManager);
  }
};

namespace {

template <typename T>
class EventQueue {
 public:
  void Enqueue(T t) {
    std::unique_lock<std::mutex> lock(mMutex);
    mList.emplace_back(std::move(t));
    mCond.notify_one();
  }
  T Dequeue() {
    std::unique_lock<std::mutex> lock(mMutex);
    while (mList.empty()) {
      mCond.wait(lock);
    }
    T result = std::move(mList.front());
    mList.pop_front();
    return result;
  }

  EventQueue() = default;
  ~EventQueue() = default;

  // Disallow copy and move
  EventQueue(const EventQueue&) = delete;
  EventQueue& operator=(const EventQueue&) = delete;

  EventQueue(EventQueue&&) = delete;
  EventQueue& operator=(EventQueue&&) = delete;

 private:
  std::mutex mMutex{};
  std::condition_variable mCond{};
  std::list<T> mList{};
};

class Signal {
 public:
  void SetSignal() {
    std::unique_lock<std::mutex> lock(mMutex);
    mSignalled = true;
    mCond.notify_one();
  }
  void WaitForSignal() {
    std::unique_lock<std::mutex> lock(mMutex);
    while (!mSignalled) {
      mCond.wait(lock);
    }
    mSignalled = false;
  }

  Signal() = default;
  ~Signal() = default;

  // Disallow copy and move
  Signal(const Signal&) = delete;
  Signal& operator=(const Signal&) = delete;

  Signal(Signal&&) = delete;
  Signal& operator=(Signal&&) = delete;

 private:
  std::mutex mMutex{};
  std::condition_variable mCond{};
  bool mSignalled{};
};

constexpr uint32_t kMaxButtons = 8;
constexpr uint32_t kMaxAxes = 4;

constexpr uint32_t kCommandSelfCheck = 1;
constexpr uint32_t kCommandQuit = 2;

struct GamepadButton {
  double value{};
  bool pressed{};
  bool touched{};
};

struct GamepadInfo {
  nsString id{};
  GamepadMappingType mapping{};
  GamepadHand hand{};
  uint32_t numButtons{};
  uint32_t numAxes{};
  std::array<GamepadButton, kMaxButtons> buttons{};
  std::array<double, kMaxAxes> axes{};
};

static void ExpectEqual(const GamepadButton& a, const GamepadButton& b) {
  EXPECT_EQ(a.value, b.value);
  EXPECT_EQ(a.pressed, b.pressed);
  EXPECT_EQ(a.touched, b.touched);
}

static void ExpectEqual(const GamepadInfo& a, const GamepadInfo& b) {
  EXPECT_EQ(a.id, b.id);
  EXPECT_EQ(a.mapping, b.mapping);
  EXPECT_EQ(a.hand, b.hand);
  EXPECT_EQ(a.numButtons, b.numButtons);
  EXPECT_EQ(a.numAxes, b.numAxes);

  for (size_t i = 0; i < a.numButtons; ++i) {
    ExpectEqual(a.buttons[i], b.buttons[i]);
  }

  for (size_t i = 0; i < a.numAxes; ++i) {
    EXPECT_EQ(a.axes[i], b.axes[i]);
  }
}

static void ExpectEqual(const std::map<GamepadHandle, GamepadInfo>& a,
                        const std::map<GamepadHandle, GamepadInfo>& b) {
  for (auto& pair : a) {
    EXPECT_TRUE(b.count(pair.first) == 1);
    ExpectEqual(a.at(pair.first), b.at(pair.first));
  }
}

class TestThread {
 public:
  TestThread() = default;
  ~TestThread() = default;

  void Run(GamepadStateBroadcastReceiverInfo remoteInfo) {
    mThread = std::thread(&TestThread::ThreadFunc, this, remoteInfo);
  }

  void Join() { mThread.join(); }

  void SetExpectedState(std::map<GamepadHandle, GamepadInfo> expectedState) {
    std::unique_lock<std::mutex> lock(mExpectedStateMutex);
    mExpectedState = std::move(expectedState);
  }

  void WaitCommandProcessed() { mCommandProcessed.WaitForSignal(); }

  // Disallow move and copy
  TestThread(const TestThread&) = delete;
  TestThread& operator=(const TestThread&) = delete;

  TestThread(TestThread&&) = delete;
  TestThread& operator=(TestThread&&) = delete;

 private:
  void HandleGamepadEvent(std::map<GamepadHandle, GamepadInfo>& gamepads,
                          const GamepadChangeEvent& event) {
    GamepadHandle handle = event.handle();

    switch (event.body().type()) {
      case GamepadChangeEventBody::TGamepadAdded: {
        MYASSERT(gamepads.count(handle) == 0);

        const GamepadAdded& x = event.body().get_GamepadAdded();

        GamepadInfo info{};
        info.id = x.id();
        info.mapping = x.mapping();
        info.hand = x.hand();
        info.numButtons = x.num_buttons();
        info.numAxes = x.num_axes();

        gamepads.insert(std::make_pair(handle, info));

        break;
      }
      case GamepadChangeEventBody::TGamepadRemoved: {
        MYASSERT(gamepads.count(handle) == 1);
        gamepads.erase(handle);
        break;
      }
      case GamepadChangeEventBody::TGamepadAxisInformation: {
        const GamepadAxisInformation& x =
            event.body().get_GamepadAxisInformation();

        gamepads[handle].axes[x.axis()] = x.value();

        break;
      }
      case GamepadChangeEventBody::TGamepadButtonInformation: {
        const GamepadButtonInformation& x =
            event.body().get_GamepadButtonInformation();

        gamepads[handle].buttons[x.button()].value = x.value();
        gamepads[handle].buttons[x.button()].pressed = x.pressed();
        gamepads[handle].buttons[x.button()].touched = x.touched();

        break;
      }
      case GamepadChangeEventBody::T__None:
      case GamepadChangeEventBody::TGamepadHandInformation:
      case GamepadChangeEventBody::TGamepadLightIndicatorTypeInformation:
      case GamepadChangeEventBody::TGamepadPoseInformation:
      case GamepadChangeEventBody::TGamepadTouchInformation: {
        MYASSERT(false);
        break;
      }
    }
  }

  bool ProcessCommand(std::map<GamepadHandle, GamepadInfo>& gamepads,
                      uint32_t commandId) {
    if (commandId == kCommandQuit) {
      return true;
    }

    MYASSERT(commandId == kCommandSelfCheck);

    std::unique_lock<std::mutex> lock(mExpectedStateMutex);
    ExpectEqual(mExpectedState, gamepads);

    return false;
  }

  void ThreadFunc(GamepadStateBroadcastReceiverInfo remoteInfo) {
    std::random_device rd;
    std::mt19937 mersenne_twister(rd());
    std::uniform_int_distribution<uint32_t> randomDelayGenerator(1, 1000);

    Maybe<GamepadStateReceiver> maybeReceiver =
        GamepadStateReceiver::Create(remoteInfo);
    MYASSERT(maybeReceiver);

    EventQueue<std::variant<GamepadChangeEvent, uint32_t>> eventQueue{};

    bool result = GamepadTestHelper::StartMonitoringThreadForTesting(
        &*maybeReceiver,
        [&](const GamepadChangeEvent& e) {
          // Simulate that sometimes there will be thread noise and make sure
          // that we still end up with the correct state anyway
          uint32_t randomDelay = randomDelayGenerator(mersenne_twister);
          std::this_thread::sleep_for(std::chrono::microseconds(randomDelay));
          eventQueue.Enqueue(e);
        },
        [&](uint32_t x) { eventQueue.Enqueue(x); });
    MYASSERT(result);

    std::map<GamepadHandle, GamepadInfo> gamepads{};

    bool quit = false;
    while (!quit) {
      auto event = eventQueue.Dequeue();

      if (std::holds_alternative<GamepadChangeEvent>(event)) {
        HandleGamepadEvent(gamepads, std::get<GamepadChangeEvent>(event));
      } else {
        quit = ProcessCommand(gamepads, std::get<uint32_t>(event));
        mCommandProcessed.SetSignal();
      }
    }

    maybeReceiver->StopMonitoringThread();
  }

  std::thread mThread{};

  std::mutex mExpectedStateMutex{};
  std::map<GamepadHandle, GamepadInfo> mExpectedState{};

  Signal mCommandProcessed{};
};

}  // anonymous namespace

TEST(GamepadStateBroadcastTest, Multithreaded)
{
  constexpr uint8_t kNumThreads = 4;

  const GamepadHandle testHandle0 = GamepadTestHelper::GetTestHandle(1234);
  const GamepadHandle testHandle1 = GamepadTestHelper::GetTestHandle(2345);
  const GamepadHandle testHandle2 = GamepadTestHelper::GetTestHandle(3456);

  std::map<GamepadHandle, GamepadInfo> expected{};

  Maybe<GamepadStateBroadcaster> maybeBroadcaster =
      GamepadStateBroadcaster::Create();
  MYASSERT(maybeBroadcaster);

  std::array<TestThread, kNumThreads> remoteThreads{};

  for (uint8_t i = 0; i < kNumThreads; ++i) {
    GamepadStateBroadcastReceiverInfo remoteInfo{};
    bool result = maybeBroadcaster->AddReceiverAndGenerateRemoteInfo(
        nullptr, &remoteInfo);
    MYASSERT(result);

    remoteThreads[i].Run(remoteInfo);
  }

  // This function can be used at any time to tell the threads to verify
  // that their state matches `expected` after they've finished processing
  // all previous updates
  auto checkExpectedState = [&] {
    for (uint8_t i = 0; i < kNumThreads; ++i) {
      remoteThreads[i].SetExpectedState(expected);
    }

    GamepadTestHelper::SendTestCommand(&*maybeBroadcaster, kCommandSelfCheck);

    for (uint8_t i = 0; i < kNumThreads; ++i) {
      remoteThreads[i].WaitCommandProcessed();
    }
  };

  // Simple one - Just add a gamepad and verify it shows up

  maybeBroadcaster->AddGamepad(testHandle0, "TestId0",
                               GamepadMappingType::Standard, GamepadHand::Left,
                               kMaxButtons, kMaxAxes, 0, 0, 0);

  GamepadInfo* info0 = &expected[testHandle0];
  info0->id = nsString(u"TestId0");
  info0->mapping = GamepadMappingType::Standard;
  info0->hand = GamepadHand::Left;
  info0->numButtons = kMaxButtons;
  info0->numAxes = kMaxAxes;

  checkExpectedState();

  // Try adding 2 more and see that they also show up
  maybeBroadcaster->AddGamepad(testHandle1, "TestId1",
                               GamepadMappingType::Standard, GamepadHand::Left,
                               2, 2, 0, 0, 0);
  maybeBroadcaster->AddGamepad(testHandle2, "TestId2",
                               GamepadMappingType::Standard, GamepadHand::Left,
                               3, 2, 0, 0, 0);

  GamepadInfo* info1 = &expected[testHandle1];
  info1->id = nsString(u"TestId1");
  info1->mapping = GamepadMappingType::Standard;
  info1->hand = GamepadHand::Left;
  info1->numButtons = 2;
  info1->numAxes = 2;

  GamepadInfo* info2 = &expected[testHandle2];
  info2->id = nsString(u"TestId2");
  info2->mapping = GamepadMappingType::Standard;
  info2->hand = GamepadHand::Left;
  info2->numButtons = 3;
  info2->numAxes = 2;

  checkExpectedState();

  // Press some buttons, move some axes, unplug one of the gamepads

  maybeBroadcaster->NewButtonEvent(testHandle1, 0, true, true, 0.5);
  maybeBroadcaster->NewButtonEvent(testHandle2, 0, true, false, 0.25);
  maybeBroadcaster->NewButtonEvent(testHandle2, 1, true, true, 0.35);
  maybeBroadcaster->NewAxisMoveEvent(testHandle1, 1, 0.5);
  maybeBroadcaster->NewAxisMoveEvent(testHandle1, 1, 0.75);
  maybeBroadcaster->NewButtonEvent(testHandle0, 0, true, true, 0.1);
  maybeBroadcaster->NewAxisMoveEvent(testHandle1, 1, 0.8);
  maybeBroadcaster->NewAxisMoveEvent(testHandle2, 0, 1.0);
  maybeBroadcaster->NewButtonEvent(testHandle2, 0, false, false, 0.0);
  maybeBroadcaster->RemoveGamepad(testHandle0);

  info0 = nullptr;
  expected.erase(testHandle0);

  info1->buttons[0].pressed = true;
  info1->buttons[0].touched = true;
  info1->buttons[0].value = 0.5;
  info1->axes[1] = 0.8;

  info2->buttons[0].pressed = false;
  info2->buttons[0].touched = false;
  info2->buttons[0].value = 0.0;
  info2->buttons[1].pressed = true;
  info2->buttons[1].touched = true;
  info2->buttons[1].value = 0.35;
  info2->axes[0] = 1.0;

  checkExpectedState();

  // Re-add gamepad0 and ensure it starts off in a null state
  maybeBroadcaster->AddGamepad(testHandle0, "TestId0",
                               GamepadMappingType::Standard, GamepadHand::Left,
                               kMaxButtons, kMaxAxes, 0, 0, 0);

  info0 = &expected[testHandle0];
  info0->id = nsString(u"TestId0");
  info0->mapping = GamepadMappingType::Standard;
  info0->hand = GamepadHand::Left;
  info0->numButtons = kMaxButtons;
  info0->numAxes = kMaxAxes;

  checkExpectedState();

  // Tell the receiver to shut everything down and exit its thread
  GamepadTestHelper::SendTestCommand(&*maybeBroadcaster, kCommandQuit);

  for (uint8_t i = 0; i < kNumThreads; ++i) {
    remoteThreads[i].WaitCommandProcessed();
  }

  for (uint8_t i = 0; i < kNumThreads; ++i) {
    remoteThreads[i].Join();
  }
}

}  // namespace mozilla::dom

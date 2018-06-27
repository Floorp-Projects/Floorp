/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadServiceTest.h"

#include "mozilla/ErrorResult.h"
#include "mozilla/Unused.h"

#include "mozilla/dom/GamepadManager.h"
#include "mozilla/dom/GamepadPlatformService.h"
#include "mozilla/dom/GamepadServiceTestBinding.h"
#include "mozilla/dom/GamepadTestChannelChild.h"

#include "mozilla/ipc/BackgroundChild.h"
#include "mozilla/ipc/PBackgroundChild.h"

#include "mozilla/Unused.h"

#include "nsIObserver.h"
#include "nsIObserverService.h"

namespace mozilla {
namespace dom {

/*
 * Implementation of the test service. This is just to provide a simple binding
 * of the GamepadService to JavaScript via WebIDL so that we can write Mochitests
 * that add and remove fake gamepads, avoiding the platform-specific backends.
 */

NS_IMPL_CYCLE_COLLECTION_INHERITED(GamepadServiceTest, DOMEventTargetHelper,
                                   mWindow)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(GamepadServiceTest)
NS_INTERFACE_MAP_END_INHERITING(DOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(GamepadServiceTest, DOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(GamepadServiceTest, DOMEventTargetHelper)

// static
already_AddRefed<GamepadServiceTest>
GamepadServiceTest::CreateTestService(nsPIDOMWindowInner* aWindow)
{
  MOZ_ASSERT(aWindow);
  RefPtr<GamepadServiceTest> service = new GamepadServiceTest(aWindow);
  service->InitPBackgroundActor();
  return service.forget();
}

void
GamepadServiceTest::Shutdown()
{
  MOZ_ASSERT(!mShuttingDown);
  mShuttingDown = true;
  DestroyPBackgroundActor();
  mWindow = nullptr;
}

GamepadServiceTest::GamepadServiceTest(nsPIDOMWindowInner* aWindow)
  : mService(GamepadManager::GetService()),
    mWindow(aWindow),
    mEventNumber(0),
    mShuttingDown(false),
    mChild(nullptr)
{}

GamepadServiceTest::~GamepadServiceTest() {}

void
GamepadServiceTest::InitPBackgroundActor()
{
  MOZ_ASSERT(!mChild);

  PBackgroundChild* actor = BackgroundChild::GetOrCreateForCurrentThread();
  if (NS_WARN_IF(!actor)) {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }

  mChild = new GamepadTestChannelChild();
  PGamepadTestChannelChild* initedChild =
    actor->SendPGamepadTestChannelConstructor(mChild);
  if (NS_WARN_IF(!initedChild)) {
    MOZ_CRASH("Failed to create a PBackgroundChild actor!");
  }
}

void
GamepadServiceTest::DestroyPBackgroundActor()
{
  mChild->SendShutdownChannel();
  mChild = nullptr;
}

already_AddRefed<Promise>
GamepadServiceTest::AddGamepad(const nsAString& aID,
                               GamepadMappingType aMapping,
                               GamepadHand aHand,
                               uint32_t aNumButtons,
                               uint32_t aNumAxes,
                               uint32_t aNumHaptics,
                               ErrorResult& aRv)
{
  if (mShuttingDown) {
    return nullptr;
  }

  // Only VR controllers has displayID, we give 0 to the general gamepads.
  GamepadAdded a(nsString(aID),
                 aMapping, aHand, 0,
                 aNumButtons, aNumAxes, aNumHaptics);
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(0, GamepadServiceType::Standard, body);

  RefPtr<Promise> p = Promise::Create(mWindow->AsGlobal(), aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  uint32_t id = ++mEventNumber;

  mChild->AddPromise(id, p);
  mChild->SendGamepadTestEvent(id, e);

  return p.forget();
}

void
GamepadServiceTest::RemoveGamepad(uint32_t aIndex)
{
  if (mShuttingDown) {
    return;
  }

  GamepadRemoved a;
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  uint32_t id = ++mEventNumber;
  mChild->SendGamepadTestEvent(id, e);
}

void
GamepadServiceTest::NewButtonEvent(uint32_t aIndex,
                                   uint32_t aButton,
                                   bool aTouched,
                                   bool aPressed)
{
  if (mShuttingDown) {
    return;
  }

  GamepadButtonInformation a(aButton, aPressed ? 1.0 : 0, aPressed, aTouched);
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  uint32_t id = ++mEventNumber;
  mChild->SendGamepadTestEvent(id, e);
}

void
GamepadServiceTest::NewButtonValueEvent(uint32_t aIndex,
                                        uint32_t aButton,
                                        bool aPressed,
                                        bool aTouched,
                                        double aValue)
{
  if (mShuttingDown) {
    return;
  }

  GamepadButtonInformation a(aButton, aValue, aPressed, aTouched);
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  uint32_t id = ++mEventNumber;
  mChild->SendGamepadTestEvent(id, e);
}

void
GamepadServiceTest::NewAxisMoveEvent(uint32_t aIndex,
                                     uint32_t aAxis,
                                     double aValue)
{
  if (mShuttingDown) {
    return;
  }

  GamepadAxisInformation a(aAxis, aValue);
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  uint32_t id = ++mEventNumber;
  mChild->SendGamepadTestEvent(id, e);
}

void
GamepadServiceTest::NewPoseMove(uint32_t aIndex,
                                const Nullable<Float32Array>& aOrient,
                                const Nullable<Float32Array>& aPos,
                                const Nullable<Float32Array>& aAngVelocity,
                                const Nullable<Float32Array>& aAngAcceleration,
                                const Nullable<Float32Array>& aLinVelocity,
                                const Nullable<Float32Array>& aLinAcceleration)
{
  if (mShuttingDown) {
    return;
  }

  GamepadPoseState poseState;
  poseState.flags = GamepadCapabilityFlags::Cap_Orientation |
                    GamepadCapabilityFlags::Cap_Position |
                    GamepadCapabilityFlags::Cap_AngularAcceleration |
                    GamepadCapabilityFlags::Cap_LinearAcceleration;
  if (!aOrient.IsNull()) {
    const Float32Array& value = aOrient.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 4);
    poseState.orientation[0] = value.Data()[0];
    poseState.orientation[1] = value.Data()[1];
    poseState.orientation[2] = value.Data()[2];
    poseState.orientation[3] = value.Data()[3];
    poseState.isOrientationValid = true;
  }
  if (!aPos.IsNull()) {
    const Float32Array& value = aPos.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.position[0] = value.Data()[0];
    poseState.position[1] = value.Data()[1];
    poseState.position[2] = value.Data()[2];
    poseState.isPositionValid = true;
  }
  if (!aAngVelocity.IsNull()) {
    const Float32Array& value = aAngVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.angularVelocity[0] = value.Data()[0];
    poseState.angularVelocity[1] = value.Data()[1];
    poseState.angularVelocity[2] = value.Data()[2];
  }
  if (!aAngAcceleration.IsNull()) {
    const Float32Array& value = aAngAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.angularAcceleration[0] = value.Data()[0];
    poseState.angularAcceleration[1] = value.Data()[1];
    poseState.angularAcceleration[2] = value.Data()[2];
  }
  if (!aLinVelocity.IsNull()) {
    const Float32Array& value = aLinVelocity.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.linearVelocity[0] = value.Data()[0];
    poseState.linearVelocity[1] = value.Data()[1];
    poseState.linearVelocity[2] = value.Data()[2];
  }
  if (!aLinAcceleration.IsNull()) {
    const Float32Array& value = aLinAcceleration.Value();
    value.ComputeLengthAndData();
    MOZ_ASSERT(value.Length() == 3);
    poseState.linearAcceleration[0] = value.Data()[0];
    poseState.linearAcceleration[1] = value.Data()[1];
    poseState.linearAcceleration[2] = value.Data()[2];
  }

  GamepadPoseInformation a(poseState);
  GamepadChangeEventBody body(a);
  GamepadChangeEvent e(aIndex, GamepadServiceType::Standard, body);

  uint32_t id = ++mEventNumber;
  mChild->SendGamepadTestEvent(id, e);
}

JSObject*
GamepadServiceTest::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return GamepadServiceTest_Binding::Wrap(aCx, this, aGivenProto);
}

} // dom
} // mozilla

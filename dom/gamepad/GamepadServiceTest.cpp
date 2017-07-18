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

NS_IMPL_CYCLE_COLLECTION_CLASS(GamepadServiceTest)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(GamepadServiceTest,
                                                  DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mWindow)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(GamepadServiceTest,
                                                DOMEventTargetHelper)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mWindow)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(GamepadServiceTest)
  NS_INTERFACE_MAP_ENTRY(nsIIPCBackgroundChildCreateCallback)
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
  PBackgroundChild *actor = BackgroundChild::GetForCurrentThread();
  //Try to get the PBackground Child actor
  if (actor) {
    ActorCreated(actor);
  } else {
    Unused << BackgroundChild::GetOrCreateForCurrentThread(this);
  }
}

void
GamepadServiceTest::DestroyPBackgroundActor()
{
  if (mChild) {
    // If mChild exists, which means that IPDL channel
    // has been created, our pending operations should
    // be empty.
    MOZ_ASSERT(mPendingOperations.IsEmpty());
    mChild->SendShutdownChannel();
    mChild = nullptr;
  } else {
    // If the IPDL channel has not been created and we
    // want to destroy it now, just cancel all pending
    // operations.
    mPendingOperations.Clear();
  }
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

  GamepadAdded a(nsString(aID), 0,
                 aMapping, aHand,
                 GamepadServiceType::Standard,
                 aNumButtons, aNumAxes, aNumHaptics);
  GamepadChangeEvent e(a);
  nsCOMPtr<nsIGlobalObject> go = do_QueryInterface(mWindow);

  RefPtr<Promise> p = Promise::Create(go, aRv);
  if (aRv.Failed()) {
    return nullptr;
  }

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->AddPromise(id, p);
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e, p);
    mPendingOperations.AppendElement(op);
  }
  return p.forget();
}

void
GamepadServiceTest::RemoveGamepad(uint32_t aIndex)
{
  if (mShuttingDown) {
    return;
  }

  GamepadRemoved a(aIndex, GamepadServiceType::Standard);
  GamepadChangeEvent e(a);

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e);
    mPendingOperations.AppendElement(op);
  }
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

  GamepadButtonInformation a(aIndex, GamepadServiceType::Standard,
                             aButton, aPressed ? 1.0 : 0, aPressed, aTouched);
  GamepadChangeEvent e(a);

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e);
    mPendingOperations.AppendElement(op);
  }
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

  GamepadButtonInformation a(aIndex, GamepadServiceType::Standard,
                             aButton, aValue, aPressed, aTouched);
  GamepadChangeEvent e(a);

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e);
    mPendingOperations.AppendElement(op);
  }
}

void
GamepadServiceTest::NewAxisMoveEvent(uint32_t aIndex,
                                     uint32_t aAxis,
                                     double aValue)
{
  if (mShuttingDown) {
    return;
  }

  GamepadAxisInformation a(aIndex, GamepadServiceType::Standard,
                           aAxis, aValue);
  GamepadChangeEvent e(a);

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e);
    mPendingOperations.AppendElement(op);
  }
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

  GamepadPoseInformation a(aIndex, GamepadServiceType::Standard,
                           poseState);
  GamepadChangeEvent e(a);

  uint32_t id = ++mEventNumber;
  if (mChild) {
    mChild->SendGamepadTestEvent(id, e);
  } else {
    PendingOperation op(id, e);
    mPendingOperations.AppendElement(op);
  }
}

void
GamepadServiceTest::FlushPendingOperations()
{
  for (uint32_t i=0; i < mPendingOperations.Length(); ++i) {
    PendingOperation op = mPendingOperations[i];
    if (op.mPromise) {
      mChild->AddPromise(op.mID, op.mPromise);
    }
    mChild->SendGamepadTestEvent(op.mID, op.mEvent);
  }
  mPendingOperations.Clear();
}

void
GamepadServiceTest::ActorCreated(PBackgroundChild* aActor)
{
  MOZ_ASSERT(aActor);
  // If we are shutting down, we don't need to create the
  // IPDL child/parent pair anymore.
  if (mShuttingDown) {
    // mPendingOperations should be cleared in
    // DestroyPBackgroundActor()
    MOZ_ASSERT(mPendingOperations.IsEmpty());
    return;
  }

  mChild = new GamepadTestChannelChild();
  PGamepadTestChannelChild* initedChild =
    aActor->SendPGamepadTestChannelConstructor(mChild);
  if (NS_WARN_IF(!initedChild)) {
    ActorFailed();
    return;
  }
  FlushPendingOperations();
}

void
GamepadServiceTest::ActorFailed()
{
  MOZ_CRASH("Failed to create background child actor!");
}

JSObject*
GamepadServiceTest::WrapObject(JSContext* aCx, JS::HandleObject aGivenProto)
{
  return GamepadServiceTestBinding::Wrap(aCx, this, aGivenProto);
}

} // dom
} // mozilla

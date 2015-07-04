/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/GamepadFunctions.h"
#include "mozilla/dom/GamepadService.h"
#include "nsHashKeys.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/unused.h"

namespace mozilla {
namespace dom {
namespace GamepadFunctions {

namespace {
// Increments as gamepads are added
uint32_t gGamepadIndex = 0;
}

template<class T>
void
NotifyGamepadChange(const T& aInfo)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  GamepadChangeEvent e(aInfo);
  nsTArray<ContentParent*> t;
  ContentParent::GetAll(t);
  for(uint32_t i = 0; i < t.Length(); ++i) {
    unused << t[i]->SendGamepadUpdate(e);
  }
  // If we have a GamepadService in the main process, send directly to it.
  if (GamepadService::IsServiceRunning()) {
    nsRefPtr<GamepadService> svc = GamepadService::GetService();
    svc->Update(e);
  }
}

uint32_t
AddGamepad(const char* aID,
           GamepadMappingType aMapping,
           uint32_t aNumButtons, uint32_t aNumAxes)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());

  int index = gGamepadIndex;
  gGamepadIndex++;
  GamepadAdded a(NS_ConvertUTF8toUTF16(nsDependentCString(aID)), index,
                 (uint32_t)aMapping, aNumButtons, aNumAxes);
  gGamepadIndex++;
  NotifyGamepadChange<GamepadAdded>(a);
  return index;
}

void
RemoveGamepad(uint32_t aIndex)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  GamepadRemoved a(aIndex);
  NotifyGamepadChange<GamepadRemoved>(a);
}

void
NewButtonEvent(uint32_t aIndex, uint32_t aButton,
               bool aPressed, double aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  GamepadButtonInformation a(aIndex, aButton, aPressed, aValue);
  NotifyGamepadChange<GamepadButtonInformation>(a);
}

void
NewButtonEvent(uint32_t aIndex, uint32_t aButton,
               bool aPressed)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  // When only a digital button is available the value will be synthesized.
  NewButtonEvent(aIndex, aButton, aPressed, aPressed ? 1.0L : 0.0L);
}

void
NewAxisMoveEvent(uint32_t aIndex, uint32_t aAxis,
                 double aValue)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  GamepadAxisInformation a(aIndex, aAxis, aValue);
  NotifyGamepadChange<GamepadAxisInformation>(a);
}

void
ResetGamepadIndexes()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(XRE_IsParentProcess());
  gGamepadIndex = 0;
}

} // namespace GamepadFunctions
} // namespace dom
} // namespace mozilla

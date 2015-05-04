/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GamepadServiceTest.h"
#include "mozilla/dom/GamepadService.h"
#include "mozilla/dom/GamepadFunctions.h"

using namespace mozilla::dom;

/*
 * Implementation of the test service. This is just to provide a simple binding
 * of the GamepadService to JavaScript via XPCOM so that we can write Mochitests
 * that add and remove fake gamepads, avoiding the platform-specific backends.
 */
NS_IMPL_ISUPPORTS(GamepadServiceTest, nsIGamepadServiceTest)

GamepadServiceTest* GamepadServiceTest::sSingleton = nullptr;

// static
already_AddRefed<GamepadServiceTest>
GamepadServiceTest::CreateService()
{
  if (sSingleton == nullptr) {
    sSingleton = new GamepadServiceTest();
  }
  nsRefPtr<GamepadServiceTest> service = sSingleton;
  return service.forget();
}

GamepadServiceTest::GamepadServiceTest() :
  mService(GamepadService::GetService())
{
}

GamepadServiceTest::~GamepadServiceTest()
{
}

/* uint32_t addGamepad(in unsigned long index, in string id, in unsigned long mapping, in unsigned long numButtons, in unsigned long numAxes); */
NS_IMETHODIMP
GamepadServiceTest::AddGamepad(const char* aID,
                               uint32_t aMapping,
                               uint32_t aNumButtons,
                               uint32_t aNumAxes,
                               uint32_t* aGamepadIndex)
{
  *aGamepadIndex = GamepadFunctions::AddGamepad(aID,
                                                static_cast<GamepadMappingType>(aMapping),
                                                aNumButtons,
                                                aNumAxes);
  return NS_OK;
}

/* void removeGamepad (in uint32_t index); */
NS_IMETHODIMP GamepadServiceTest::RemoveGamepad(uint32_t aIndex)
{
  GamepadFunctions::RemoveGamepad(aIndex);
  return NS_OK;
}

/* void newButtonEvent (in uint32_t index, in uint32_t button,
   in boolean pressed); */
NS_IMETHODIMP GamepadServiceTest::NewButtonEvent(uint32_t aIndex,
                                                 uint32_t aButton,
                                                 bool aPressed)
{
  GamepadFunctions::NewButtonEvent(aIndex, aButton, aPressed);
  return NS_OK;
}

/* void newButtonEvent (in uint32_t index, in uint32_t button,
   in boolean pressed, double value); */
NS_IMETHODIMP GamepadServiceTest::NewButtonValueEvent(uint32_t aIndex,
                                                      uint32_t aButton,
                                                      bool aPressed,
                                                      double aValue)
{
  GamepadFunctions::NewButtonEvent(aIndex, aButton, aPressed, aValue);
  return NS_OK;
}

/* void newAxisMoveEvent (in uint32_t index, in uint32_t axis,
   in double value); */
NS_IMETHODIMP GamepadServiceTest::NewAxisMoveEvent(uint32_t aIndex,
                                                   uint32_t aAxis,
                                                   double aValue)
{
  GamepadFunctions::NewAxisMoveEvent(aIndex, aAxis, aValue);
  return NS_OK;
}


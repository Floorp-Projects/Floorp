/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputAPZContext.h"

namespace mozilla {
namespace layers {

ScrollableLayerGuid InputAPZContext::sGuid;
uint64_t InputAPZContext::sBlockId = 0;
nsEventStatus InputAPZContext::sApzResponse = nsEventStatus_eIgnore;
bool InputAPZContext::sRoutedToChildProcess = false;

/*static*/ ScrollableLayerGuid
InputAPZContext::GetTargetLayerGuid()
{
  return sGuid;
}

/*static*/ uint64_t
InputAPZContext::GetInputBlockId()
{
  return sBlockId;
}

/*static*/ nsEventStatus
InputAPZContext::GetApzResponse()
{
  return sApzResponse;
}

/*static*/ void
InputAPZContext::SetRoutedToChildProcess()
{
  sRoutedToChildProcess = true;
}

InputAPZContext::InputAPZContext(const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aBlockId,
                                 const nsEventStatus& aApzResponse)
  : mOldGuid(sGuid)
  , mOldBlockId(sBlockId)
  , mOldApzResponse(sApzResponse)
  , mOldRoutedToChildProcess(sRoutedToChildProcess)
{
  sGuid = aGuid;
  sBlockId = aBlockId;
  sApzResponse = aApzResponse;
  sRoutedToChildProcess = false;
}

InputAPZContext::~InputAPZContext()
{
  sGuid = mOldGuid;
  sBlockId = mOldBlockId;
  sApzResponse = mOldApzResponse;
  sRoutedToChildProcess = mOldRoutedToChildProcess;
}

bool
InputAPZContext::WasRoutedToChildProcess()
{
  return sRoutedToChildProcess;
}

}
}

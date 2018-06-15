/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "InputAPZContext.h"

namespace mozilla {
namespace layers {

ScrollableLayerGuid InputAPZContext::sGuid;
uint64_t InputAPZContext::sBlockId = 0;
nsEventStatus InputAPZContext::sApzResponse = nsEventStatus_eIgnore;
bool InputAPZContext::sPendingLayerization = false;
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

/*static*/ bool
InputAPZContext::HavePendingLayerization()
{
  return sPendingLayerization;
}

/*static*/ bool
InputAPZContext::WasRoutedToChildProcess()
{
  return sRoutedToChildProcess;
}

InputAPZContext::InputAPZContext(const ScrollableLayerGuid& aGuid,
                                 const uint64_t& aBlockId,
                                 const nsEventStatus& aApzResponse,
                                 bool aPendingLayerization)
  : mOldGuid(sGuid)
  , mOldBlockId(sBlockId)
  , mOldApzResponse(sApzResponse)
  , mOldPendingLayerization(sPendingLayerization)
  , mOldRoutedToChildProcess(sRoutedToChildProcess)
{
  sGuid = aGuid;
  sBlockId = aBlockId;
  sApzResponse = aApzResponse;
  sPendingLayerization = aPendingLayerization;
  sRoutedToChildProcess = false;
}

InputAPZContext::~InputAPZContext()
{
  sGuid = mOldGuid;
  sBlockId = mOldBlockId;
  sApzResponse = mOldApzResponse;
  sPendingLayerization = mOldPendingLayerization;
  sRoutedToChildProcess = mOldRoutedToChildProcess;
}

/*static*/ void
InputAPZContext::SetRoutedToChildProcess()
{
  sRoutedToChildProcess = true;
}

} // namespace layers
} // namespace mozilla

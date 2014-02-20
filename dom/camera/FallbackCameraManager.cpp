/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ICameraControl.h"

using namespace mozilla;

// From ICameraControl.
nsresult
ICameraControl::GetNumberOfCameras(int32_t& aDeviceCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
};

nsresult
ICameraControl::GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
ICameraControl::GetListOfCameras(nsTArray<nsString>& aList)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

already_AddRefed<ICameraControl>
ICameraControl::Create(uint32_t aCameraId)
{
  return nullptr;
}

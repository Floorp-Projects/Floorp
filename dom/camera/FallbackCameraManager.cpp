/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraManager.h"

#include "mozilla/ErrorResult.h"

using namespace mozilla;

// From nsDOMCameraManager.
nsresult
nsDOMCameraManager::GetNumberOfCameras(int32_t& aDeviceCount)
{
  return NS_ERROR_NOT_IMPLEMENTED;
};

nsresult
nsDOMCameraManager::GetCameraName(uint32_t aDeviceNum, nsCString& aDeviceName)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void
nsDOMCameraManager::GetListOfCameras(nsTArray<nsString>& aList, ErrorResult& aRv)
{
  aRv.Throw(NS_ERROR_NOT_IMPLEMENTED);
}

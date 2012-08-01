/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DOMCameraManager.h"

// From nsDOMCameraManager.

/* [implicit_jscontext] jsval getListOfCameras (); */
NS_IMETHODIMP
nsDOMCameraManager::GetListOfCameras(JSContext* cx, JS::Value* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

using namespace mozilla;

NS_IMETHODIMP
GetCameraTask::Run()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_DOMCAMERAMANAGER_H
#define DOM_CAMERA_DOMCAMERAMANAGER_H

#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"
#include "nsIDOMCameraManager.h"
#include "mozilla/Attributes.h"

class nsDOMCameraManager MOZ_FINAL : public nsIDOMCameraManager
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMCAMERAMANAGER

  static already_AddRefed<nsDOMCameraManager> Create(uint64_t aWindowId);

  void OnNavigation(uint64_t aWindowId);

private:
  nsDOMCameraManager();
  nsDOMCameraManager(uint64_t aWindowId);
  nsDOMCameraManager(const nsDOMCameraManager&) MOZ_DELETE;
  nsDOMCameraManager& operator=(const nsDOMCameraManager&) MOZ_DELETE;
  ~nsDOMCameraManager();

protected:
  uint64_t mWindowId;
  nsCOMPtr<nsIThread> mCameraThread;
};

class GetCameraTask : public nsRunnable
{
public:
  GetCameraTask(uint32_t aCameraId, nsICameraGetCameraCallback* onSuccess, nsICameraErrorCallback* onError, nsIThread* aCameraThread)
    : mCameraId(aCameraId)
    , mOnSuccessCb(onSuccess)
    , mOnErrorCb(onError)
    , mCameraThread(aCameraThread)
  { }

  NS_IMETHOD Run();

protected:
  uint32_t mCameraId;
  nsCOMPtr<nsICameraGetCameraCallback> mOnSuccessCb;
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
  nsCOMPtr<nsIThread> mCameraThread;
};

#endif // DOM_CAMERA_DOMCAMERAMANAGER_H

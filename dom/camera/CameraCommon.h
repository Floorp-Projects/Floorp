/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_CAMERA_CAMERACOMMON_H
#define DOM_CAMERA_CAMERACOMMON_H

#ifndef __func__
#ifdef __FUNCTION__
#define __func__ __FUNCTION__
#else
#define __func__ __FILE__
#endif
#endif

#ifndef NAN
#define NAN std::numeric_limits<double>::quiet_NaN()
#endif

#include "nsThreadUtils.h"
#include "nsIDOMCameraManager.h"

#define DOM_CAMERA_LOG( l, ... )          \
  do {                                    \
    if ( DOM_CAMERA_LOG_LEVEL >= (l) ) {  \
      printf_stderr (__VA_ARGS__);        \
    }                                     \
  } while (0)

#define DOM_CAMERA_LOGA( ... )        DOM_CAMERA_LOG( 0, __VA_ARGS__ )

enum {
  DOM_CAMERA_LOG_NOTHING,
  DOM_CAMERA_LOG_ERROR,
  DOM_CAMERA_LOG_WARNING,
  DOM_CAMERA_LOG_INFO
};

#define DOM_CAMERA_LOGI( ... )        DOM_CAMERA_LOG( DOM_CAMERA_LOG_INFO,  __VA_ARGS__ )
#define DOM_CAMERA_LOGW( ... )        DOM_CAMERA_LOG( DOM_CAMERA_LOG_WARNING, __VA_ARGS__ )
#define DOM_CAMERA_LOGE( ... )        DOM_CAMERA_LOG( DOM_CAMERA_LOG_ERROR, __VA_ARGS__ )

class CameraErrorResult : public nsRunnable
{
public:
  CameraErrorResult(nsICameraErrorCallback* onError, const nsString& aErrorMsg)
    : mOnErrorCb(onError)
    , mErrorMsg(aErrorMsg)
  { }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mOnErrorCb) {
      mOnErrorCb->HandleEvent(mErrorMsg);
    }
    return NS_OK;
  }

protected:
  nsCOMPtr<nsICameraErrorCallback> mOnErrorCb;
  const nsString mErrorMsg;
};

#endif // DOM_CAMERA_CAMERACOMMON_H

/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_mediaplaystatus_h__
#define mozilla_dom_bluetooth_mediaplaystatus_h__

#include "jsapi.h"
#include "nsString.h"

BEGIN_BLUETOOTH_NAMESPACE

class MediaPlayStatus
{
public:
  MediaPlayStatus();

  nsresult Init(JSContext* aCx, const jsval* aVal);

  int64_t mDuration;
  nsString mPlayStatus;
  int64_t mPosition;
};

END_BLUETOOTH_NAMESPACE

#endif

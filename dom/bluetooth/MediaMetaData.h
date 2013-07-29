/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_mediametadata_h__
#define mozilla_dom_bluetooth_mediametadata_h__

#include "jsapi.h"
#include "nsString.h"

BEGIN_BLUETOOTH_NAMESPACE

class MediaMetaData
{
public:
  MediaMetaData();

  nsresult Init(JSContext* aCx, const jsval* aVal);

  nsString mAlbum;
  nsString mArtist;
  int64_t mDuration;
  int64_t mMediaNumber;
  nsString mTitle;
  int64_t mTotalMediaCount;
};

END_BLUETOOTH_NAMESPACE

#endif

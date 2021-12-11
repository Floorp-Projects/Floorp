/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDOMDataChannelDeclarations_h
#define nsDOMDataChannelDeclarations_h

// This defines only what's necessary to create nsDOMDataChannels, since this
// gets used with MOZ_INTERNAL_API not set for media/webrtc/signaling/testing

#include "nsCOMPtr.h"

namespace mozilla {
class DataChannel;
}

class nsDOMDataChannel;
class nsPIDOMWindowInner;

nsresult NS_NewDOMDataChannel(
    already_AddRefed<mozilla::DataChannel>&& dataChannel,
    nsPIDOMWindowInner* aWindow, nsDOMDataChannel** domDataChannel);

#endif  // nsDOMDataChannelDeclarations_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MIDIPortInterface.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDITypes.h"

mozilla::dom::MIDIPortInterface::MIDIPortInterface(
    const MIDIPortInfo& aPortInfo, bool aSysexEnabled)
    : mId(aPortInfo.id()),
      mName(aPortInfo.name()),
      mManufacturer(aPortInfo.manufacturer()),
      mVersion(aPortInfo.version()),
      mSysexEnabled(aSysexEnabled),
      mType((MIDIPortType)aPortInfo.type()),
      // We'll never initialize a port object that's not connected
      mDeviceState(MIDIPortDeviceState::Connected),
      // Open everything on connection
      mConnectionState(MIDIPortConnectionState::Open),
      mShuttingDown(false) {}

mozilla::dom::MIDIPortInterface::~MIDIPortInterface() { Shutdown(); }

void mozilla::dom::MIDIPortInterface::Shutdown() { mShuttingDown = true; }

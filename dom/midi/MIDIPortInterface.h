/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPortInterface_h
#define mozilla_dom_MIDIPortInterface_h

#include "mozilla/dom/MIDIPortBinding.h"

namespace mozilla::dom {
class MIDIPortInfo;
/**
 * Base class for MIDIPort Parent/Child Actors. Makes sure both sides of the
 * MIDIPort IPC connection need to a synchronized set of info/state.
 *
 */
class MIDIPortInterface {
 public:
  MIDIPortInterface(const MIDIPortInfo& aPortInfo, bool aSysexEnabled);
  const nsString& Id() const { return mId; }
  const nsString& Name() const { return mName; }
  const nsString& Manufacturer() const { return mManufacturer; }
  const nsString& Version() const { return mVersion; }
  bool SysexEnabled() const { return mSysexEnabled; }
  MIDIPortType Type() const { return mType; }
  MIDIPortDeviceState DeviceState() const { return mDeviceState; }
  MIDIPortConnectionState ConnectionState() const { return mConnectionState; }
  bool IsShutdown() const { return mShuttingDown; }
  virtual void Shutdown();

 protected:
  virtual ~MIDIPortInterface();
  nsString mId;
  nsString mName;
  nsString mManufacturer;
  nsString mVersion;
  bool mSysexEnabled;
  MIDIPortType mType;
  MIDIPortDeviceState mDeviceState;
  MIDIPortConnectionState mConnectionState;
  bool mShuttingDown;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIPortInterface_h

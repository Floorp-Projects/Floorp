/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPlatformRunnables_h
#define mozilla_dom_MIDIPlatformRunnables_h

#include "mozilla/dom/MIDITypes.h"

namespace mozilla {
namespace dom {

class MIDIPortParent;
class MIDIMessage;
class MIDIPortInfo;

/**
 * Base class for runnables to be fired to the platform-specific MIDI service
 * thread in PBackground.
 */
class MIDIBackgroundRunnable : public Runnable {
 public:
  MIDIBackgroundRunnable(const char* aName) : Runnable(aName) {}
  virtual ~MIDIBackgroundRunnable() = default;
  NS_IMETHOD Run() override;
  virtual void RunInternal() = 0;
};

/**
 * Runnable fired from platform-specific MIDI service thread to PBackground
 * Thread whenever messages need to be sent to a MIDI device.
 *
 */
class ReceiveRunnable final : public MIDIBackgroundRunnable {
 public:
  ReceiveRunnable(const nsAString& aPortId, const nsTArray<MIDIMessage>& aMsgs)
      : MIDIBackgroundRunnable("ReceiveRunnable"),
        mMsgs(aMsgs.Clone()),
        mPortId(aPortId) {}
  // Used in tests
  ReceiveRunnable(const nsAString& aPortId, const MIDIMessage& aMsgs)
      : MIDIBackgroundRunnable("ReceiveRunnable"), mPortId(aPortId) {
    mMsgs.AppendElement(aMsgs);
  }
  ~ReceiveRunnable() = default;
  void RunInternal() override;

 private:
  nsTArray<MIDIMessage> mMsgs;
  nsString mPortId;
};

/**
 * Runnable fired from platform-specific MIDI service thread to PBackground
 * Thread whenever a device is connected.
 *
 */
class AddPortRunnable final : public MIDIBackgroundRunnable {
 public:
  explicit AddPortRunnable(const MIDIPortInfo& aPortInfo)
      : MIDIBackgroundRunnable("AddPortRunnable"), mPortInfo(aPortInfo) {}
  ~AddPortRunnable() = default;
  void RunInternal() override;

 private:
  MIDIPortInfo mPortInfo;
};

/**
 * Runnable fired from platform-specific MIDI service thread to PBackground
 * Thread whenever a device is disconnected.
 *
 */
class RemovePortRunnable final : public MIDIBackgroundRunnable {
 public:
  explicit RemovePortRunnable(const MIDIPortInfo& aPortInfo)
      : MIDIBackgroundRunnable("RemovePortRunnable"), mPortInfo(aPortInfo) {}
  ~RemovePortRunnable() = default;
  void RunInternal() override;

 private:
  MIDIPortInfo mPortInfo;
};

/**
 * Runnable used to delay calls to SendPortList, which is requires to make sure
 * MIDIManager actor initialization happens correctly. Also used for testing.
 *
 */
class SendPortListRunnable final : public MIDIBackgroundRunnable {
 public:
  SendPortListRunnable() : MIDIBackgroundRunnable("SendPortListRunnable") {}
  ~SendPortListRunnable() = default;
  void RunInternal() override;
};

/**
 * Runnable fired from platform-specific MIDI service thread to PBackground
 * Thread whenever a device is disconnected.
 *
 */
class SetStatusRunnable final : public MIDIBackgroundRunnable {
 public:
  SetStatusRunnable(const nsAString& aPortId, MIDIPortDeviceState aState,
                    MIDIPortConnectionState aConnection)
      : MIDIBackgroundRunnable("SetStatusRunnable"),
        mPortId(aPortId),
        mState(aState),
        mConnection(aConnection) {}
  ~SetStatusRunnable() = default;
  void RunInternal() override;

 private:
  nsString mPortId;
  MIDIPortDeviceState mState;
  MIDIPortConnectionState mConnection;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIPlatformRunnables_h

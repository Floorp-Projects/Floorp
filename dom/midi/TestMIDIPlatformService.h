/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TestMIDIPlatformService_h
#define mozilla_dom_TestMIDIPlatformService_h

#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDITypes.h"

class nsIThread;

namespace mozilla::dom {

class MIDIPortInterface;

/**
 * Platform service implementation used for mochitests. Emulates what a real
 * platform service should look like, including using an internal IO thread for
 * message IO.
 *
 */
class TestMIDIPlatformService : public MIDIPlatformService {
 public:
  TestMIDIPlatformService();
  virtual void Init() override;
  virtual void Refresh() override;
  virtual void Open(MIDIPortParent* aPort) override;
  virtual void Stop() override;
  virtual void ScheduleSend(const nsAString& aPort) override;
  virtual void ScheduleClose(MIDIPortParent* aPort) override;
  // MIDI Service simulation function. Can take specially formed sysex messages
  // in order to trigger device connection events and state changes,
  // interrupting messages for high priority sysex sends, etc...
  void ProcessMessages(const nsAString& aPort);

 private:
  virtual ~TestMIDIPlatformService();
  // Port that takes test control messages
  MIDIPortInfo mControlInputPort;
  // Port that returns test status messages
  MIDIPortInfo mControlOutputPort;
  // Used for testing input connection/disconnection
  MIDIPortInfo mStateTestInputPort;
  // Used for testing output connection/disconnection
  MIDIPortInfo mStateTestOutputPort;
  // Used for testing open() call failures
  MIDIPortInfo mAlwaysClosedTestOutputPort;
  // IO Simulation thread. Runs all instances of ProcessMessages().
  nsCOMPtr<nsIThread> mClientThread;
  // When true calling Refresh() will add new ports.
  bool mDoRefresh;
  // True if server has been brought up already.
  bool mIsInitialized;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_TestMIDIPlatformService_h

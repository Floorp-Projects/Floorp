/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_midirMIDIPlatformService_h
#define mozilla_dom_midirMIDIPlatformService_h

#include "mozilla/StaticMutex.h"
#include "mozilla/dom/MIDIPlatformService.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/dom/midi/midir_impl_ffi_generated.h"

class nsIThread;
struct MidirWrapper;

namespace mozilla::dom {

class MIDIPortInterface;

/**
 * Platform service implementation using the midir crate.
 */
class midirMIDIPlatformService : public MIDIPlatformService {
 public:
  midirMIDIPlatformService();
  virtual void Init() override;
  virtual void Open(MIDIPortParent* aPort) override;
  virtual void Stop() override;
  virtual void ScheduleSend(const nsAString& aPort) override;
  virtual void ScheduleClose(MIDIPortParent* aPort) override;

  void SendMessages(const nsAString& aPort);

 private:
  virtual ~midirMIDIPlatformService();

  static void AddPort(const nsString* aId, const nsString* aName, bool aInput);
  static void CheckAndReceive(const nsString* aId, const uint8_t* aData,
                              size_t aLength, const GeckoTimeStamp* aTimeStamp,
                              uint64_t aMicros);

  // Wrapper around the midir Rust implementation.
  MidirWrapper* mImplementation;

  // midir has its own internal threads and we can't execute jobs directly on
  // them, instead we forward them to the background thread the service was
  // created in.
  static StaticMutex gBackgroundThreadMutex;
  static nsCOMPtr<nsIThread> gBackgroundThread;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_midirMIDIPlatformService_h

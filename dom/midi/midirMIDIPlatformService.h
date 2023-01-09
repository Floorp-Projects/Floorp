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
  virtual void Refresh() override;
  virtual void Open(MIDIPortParent* aPort) override;
  virtual void Stop() override;
  virtual void ScheduleSend(const nsAString& aPort) override;
  virtual void ScheduleClose(MIDIPortParent* aPort) override;

  void SendMessage(const nsAString& aPort, const MIDIMessage& aMessage);

 private:
  virtual ~midirMIDIPlatformService();

  static void AddPort(const nsString* aId, const nsString* aName, bool aInput);
  static void RemovePort(const nsString* aId, const nsString* aName,
                         bool aInput);
  static void CheckAndReceive(const nsString* aId, const uint8_t* aData,
                              size_t aLength, const GeckoTimeStamp* aTimeStamp,
                              uint64_t aMicros);

  // Wrapper around the midir Rust implementation.
  MidirWrapper* mImplementation;

  // The midir backends can invoke CheckAndReceive on arbitrary background
  // threads, and so we dispatch events from there to the owner task queue.
  // It's a little ambiguous whether midir can ever invoke CheckAndReceive
  // on one of its platform-specific background threads after we've dropped
  // the main instance. Just in case, we use a static mutex to avoid a potential
  // race with dropping the primary reference to the task queue via
  // ClearOnShutdown.
  static StaticMutex gOwnerThreadMutex;
  static nsCOMPtr<nsISerialEventTarget> gOwnerThread
      MOZ_GUARDED_BY(gOwnerThreadMutex);
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_midirMIDIPlatformService_h

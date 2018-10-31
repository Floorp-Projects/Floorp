/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIAccessManager_h
#define mozilla_dom_MIDIAccessManager_h

#include "nsPIDOMWindow.h"
#include "nsIObserver.h"
#include "mozilla/dom/MIDITypes.h"
#include "mozilla/Observer.h"

namespace mozilla {
namespace dom {

class MIDIAccess;
class MIDIManagerChild;
struct MIDIOptions;
class MIDIPortChangeEvent;
class MIDIPortInfo;
class Promise;

/**
 * MIDIAccessManager manages creation and lifetime of MIDIAccess objects for the
 * process it lives in. It is in charge of dealing with permission requests,
 * creating new MIDIAccess objects, and updating live MIDIAccess objects with
 * new device listings.
 *
 * While a process/window can have many MIDIAccess objects, there is only one
 * MIDIAccessManager for any one process.
 */
class MIDIAccessManager final
{
public:
  NS_INLINE_DECL_REFCOUNTING(MIDIAccessManager);
  // Handles requests from Navigator for MIDI permissions and MIDIAccess
  // creation.
  already_AddRefed<Promise>
  RequestMIDIAccess(nsPIDOMWindowInner* aWindow,
                    const MIDIOptions& aOptions,
                    ErrorResult& aRv);
  // Creates a new MIDIAccess object
  void CreateMIDIAccess(nsPIDOMWindowInner* aWindow,
                        bool aNeedsSysex,
                        Promise* aPromise);
  // Getter for manager singleton
  static MIDIAccessManager* Get();
  // True if manager singleton has been created
  static bool IsRunning();
  // Send device connection updates to all known MIDIAccess objects.
  void Update(const MIDIPortList& aEvent);
  // Adds a device update observer (usually a MIDIAccess object)
  bool AddObserver(Observer<MIDIPortList>* aObserver);
  // Removes a device update observer (usually a MIDIAccess object)
  void RemoveObserver(Observer<MIDIPortList>* aObserver);

private:
  MIDIAccessManager();
  ~MIDIAccessManager();
  // True if object has received a device list from the MIDI platform service.
  bool mHasPortList;
  // List of known ports for the system.
  MIDIPortList mPortList;
  // Holds MIDIAccess objects until we've received the first list of devices
  // from the MIDI Service.
  nsTArray<RefPtr<MIDIAccess>> mAccessHolder;
  // Device state update observers (usually MIDIAccess objects)
  ObserverList<MIDIPortList> mChangeObservers;
  // IPC Object for MIDIManager. Created on first MIDIAccess object creation,
  // destroyed on last MIDIAccess object destruction.
  RefPtr<MIDIManagerChild> mChild;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MIDIAccessManager_h

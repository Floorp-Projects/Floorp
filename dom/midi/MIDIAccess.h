/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIAccess_h
#define mozilla_dom_MIDIAccess_h

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/Observer.h"
#include "mozilla/WeakPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsWrapperCache.h"

struct JSContext;

namespace mozilla {
class ErrorResult;

namespace dom {

class MIDIAccessManager;
class MIDIInputMap;
struct MIDIOptions;
class MIDIOutputMap;
class MIDIPermissionRequest;
class MIDIPort;
class MIDIPortChangeEvent;
class MIDIPortInfo;
class MIDIPortList;
class Promise;

/**
 * MIDIAccess is the DOM object that is handed to the user upon MIDI permissions
 * being successfully granted. It manages access to MIDI ports, and fires events
 * for device connection and disconnection.
 *
 * New MIDIAccess objects are created every time RequestMIDIAccess is called.
 * MIDIAccess objects are managed via MIDIAccessManager.
 */
class MIDIAccess final : public DOMEventTargetHelper,
                         public Observer<MIDIPortList>,
                         public SupportsWeakPtr {
  // Use the Permission Request class in MIDIAccessManager for creating
  // MIDIAccess objects.
  friend class MIDIPermissionRequest;
  friend class MIDIAccessManager;

 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MIDIAccess,
                                                         DOMEventTargetHelper)
 public:
  virtual JSObject* WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto) override;

  // Return map of MIDI Input Ports
  MIDIInputMap* Inputs() const { return mInputMap; }

  // Return map of MIDI Output Ports
  MIDIOutputMap* Outputs() const { return mOutputMap; }

  // Returns true if sysex permissions were given
  bool SysexEnabled() const { return mSysexEnabled; }

  // Observer implementation for receiving port connection updates
  void Notify(const MIDIPortList& aEvent) override;

  // Fires DOM event on port connection/disconnection
  void FireConnectionEvent(MIDIPort* aPort);

  // Notify all MIDIPorts that were created by this MIDIAccess and are still
  // alive, and detach from the MIDIAccessManager.
  void Shutdown();
  IMPL_EVENT_HANDLER(statechange);

  void DisconnectFromOwner() override;

 private:
  MIDIAccess(nsPIDOMWindowInner* aWindow, bool aSysexEnabled,
             Promise* aAccessPromise);
  ~MIDIAccess();

  // On receiving a connection event from MIDIAccessManager, create a
  // corresponding MIDIPort object if we don't already have one.
  void MaybeCreateMIDIPort(const MIDIPortInfo& aInfo, ErrorResult& aRv);

  // Stores all known MIDIInput Ports
  RefPtr<MIDIInputMap> mInputMap;
  // Stores all known MIDIOutput Ports
  RefPtr<MIDIOutputMap> mOutputMap;
  // True if user gave permissions for sysex usage to this object.
  bool mSysexEnabled;
  // Promise created by RequestMIDIAccess call, to be resolved after port
  // populating is finished.
  RefPtr<Promise> mAccessPromise;
  // True if shutdown process has started, so we don't try to add more ports.
  bool mHasShutdown;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIAccess_h

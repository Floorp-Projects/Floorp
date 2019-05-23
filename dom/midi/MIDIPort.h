/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPort_h
#define mozilla_dom_MIDIPort_h

#include "nsWrapperCache.h"
#include "mozilla/Attributes.h"
#include "mozilla/Observer.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/MIDIAccess.h"
#include "mozilla/dom/MIDIPortInterface.h"

struct JSContext;

namespace mozilla {
namespace dom {

class Promise;
class MIDIPortInfo;
class MIDIAccess;
class MIDIPortChangeEvent;
class MIDIPortChild;
class MIDIMessage;

/**
 * Implementation of WebIDL DOM MIDIPort class. Handles all port representation
 * and communication.
 *
 */
class MIDIPort : public DOMEventTargetHelper,
                 public MIDIAccessDestructionObserver {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MIDIPort,
                                                         DOMEventTargetHelper)
 protected:
  MIDIPort(nsPIDOMWindowInner* aWindow, MIDIAccess* aMIDIAccessParent);
  bool Initialize(const MIDIPortInfo& aPortInfo, bool aSysexEnabled);
  virtual ~MIDIPort();

 public:
  nsPIDOMWindowInner* GetParentObject() const { return GetOwner(); }

  // Getters
  void GetId(nsString& aRetVal) const;
  void GetManufacturer(nsString& aRetVal) const;
  void GetName(nsString& aRetVal) const;
  void GetVersion(nsString& aRetVal) const;
  MIDIPortType Type() const;
  MIDIPortConnectionState Connection() const;
  MIDIPortDeviceState State() const;
  bool SysexEnabled() const;

  already_AddRefed<Promise> Open();
  already_AddRefed<Promise> Close();

  // MIDIPorts observe the death of their parent MIDIAccess object, and delete
  // their reference accordingly.
  virtual void Notify(const void_t& aVoid) override;

  void FireStateChangeEvent();

  virtual void Receive(const nsTArray<MIDIMessage>& aMsg);

  // This object holds a pointer to its corresponding IPC MIDIPortChild actor.
  // If the IPC actor is deleted, it cleans itself up via this method.
  void UnsetIPCPort();

  IMPL_EVENT_HANDLER(statechange)
 protected:
  // IPC Actor corresponding to this class
  RefPtr<MIDIPortChild> mPort;

 private:
  // MIDIAccess object that created this MIDIPort object, which we need for
  // firing port connection events. There is a chance this MIDIPort object can
  // outlive its parent MIDIAccess object, so this is a weak reference that must
  // be handled properly. It is set on construction of the MIDIPort object, and
  // set to null when the parent MIDIAccess object is destroyed, which fires an
  // notification we observe.
  MIDIAccess* mMIDIAccessParent;
  // Promise object generated on Open() call, that needs to be resolved once the
  // platform specific Open() function has completed.
  RefPtr<Promise> mOpeningPromise;
  // Promise object generated on Close() call, that needs to be resolved once
  // the platform specific Close() function has completed.
  RefPtr<Promise> mClosingPromise;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIPort_h

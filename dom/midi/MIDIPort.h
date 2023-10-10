/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPort_h
#define mozilla_dom_MIDIPort_h

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/MIDIAccess.h"
#include "mozilla/dom/MIDIPortChild.h"
#include "mozilla/dom/MIDIPortInterface.h"

struct JSContext;

namespace mozilla::dom {

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
  explicit MIDIPort(nsPIDOMWindowInner* aWindow);
  bool Initialize(const MIDIPortInfo& aPortInfo, bool aSysexEnabled,
                  MIDIAccess* aMIDIAccessParent);
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

  already_AddRefed<Promise> Open(ErrorResult& aError);
  already_AddRefed<Promise> Close(ErrorResult& aError);

  // MIDIPorts observe the death of their parent MIDIAccess object, and delete
  // their reference accordingly.
  virtual void Notify(const void_t& aVoid) override;

  void FireStateChangeEvent();

  virtual void StateChange();
  virtual void Receive(const nsTArray<MIDIMessage>& aMsg);

  // This object holds a pointer to its corresponding IPC MIDIPortChild actor.
  // If the IPC actor is deleted, it cleans itself up via this method.
  void UnsetIPCPort();

  IMPL_EVENT_HANDLER(statechange)

  void DisconnectFromOwner() override;
  const nsString& StableId();

 protected:
  // Helper class to ensure we always call DetachOwner when we drop the
  // reference to the the port.
  class PortHolder {
   public:
    void Init(already_AddRefed<MIDIPortChild> aArg) {
      MOZ_ASSERT(!mInner);
      mInner = aArg;
    }
    void Clear() {
      if (mInner) {
        mInner->DetachOwner();
        mInner = nullptr;
      }
    }
    ~PortHolder() { Clear(); }
    MIDIPortChild* Get() const { return mInner; }

   private:
    RefPtr<MIDIPortChild> mInner;
  };

  // IPC Actor corresponding to this class.
  PortHolder mPortHolder;
  MIDIPortChild* Port() const { return mPortHolder.Get(); }

 private:
  void KeepAliveOnStatechange();
  void DontKeepAliveOnStatechange();

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
  // If true this object will be kept alive even without direct JS references
  bool mKeepAlive;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIPort_h

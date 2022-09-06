/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIInputMap_h
#define mozilla_dom_MIDIInputMap_h

#include "mozilla/dom/MIDIPort.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla::dom {

/**
 * Maplike DOM object that holds a list of all MIDI input ports available for
 * access. Almost all functions are implemented automatically by WebIDL.
 */
class MIDIInputMap final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(MIDIInputMap)
  nsPIDOMWindowInner* GetParentObject() const { return mParent; }

  explicit MIDIInputMap(nsPIDOMWindowInner* aParent);
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  bool Has(nsAString& aId) { return mPorts.Get(aId) != nullptr; }
  void Insert(nsAString& aId, RefPtr<MIDIPort> aPort) {
    mPorts.InsertOrUpdate(aId, aPort);
  }
  void Remove(nsAString& aId) { mPorts.Remove(aId); }

 private:
  ~MIDIInputMap() = default;
  nsTHashMap<nsString, RefPtr<MIDIPort>> mPorts;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_MIDIInputMap_h

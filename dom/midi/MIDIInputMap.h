/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIInputMap_h
#define mozilla_dom_MIDIInputMap_h

#include "nsCOMPtr.h"
#include "nsWrapperCache.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

/**
 * Maplike DOM object that holds a list of all MIDI input ports available for
 * access. Almost all functions are implemented automatically by WebIDL.
 */
class MIDIInputMap final : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(MIDIInputMap)
  nsPIDOMWindowInner* GetParentObject() const { return mParent; }

  explicit MIDIInputMap(nsPIDOMWindowInner* aParent);
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

 private:
  ~MIDIInputMap() = default;
  nsCOMPtr<nsPIDOMWindowInner> mParent;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIInputMap_h

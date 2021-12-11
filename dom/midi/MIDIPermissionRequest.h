/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPermissionRequest_h
#define mozilla_dom_MIDIPermissionRequest_h

#include "mozilla/dom/Promise.h"
#include "nsContentPermissionHelper.h"

namespace mozilla {
namespace dom {

struct MIDIOptions;

/**
 * Handles permission dialog management when requesting MIDI permissions.
 */
class MIDIPermissionRequest final : public ContentPermissionRequestBase,
                                    public nsIRunnable {
 public:
  MIDIPermissionRequest(nsPIDOMWindowInner* aWindow, Promise* aPromise,
                        const MIDIOptions& aOptions);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(MIDIPermissionRequest,
                                           ContentPermissionRequestBase)
  // nsIContentPermissionRequest
  NS_IMETHOD Cancel(void) override;
  NS_IMETHOD Allow(JS::HandleValue choices) override;
  NS_IMETHOD GetTypes(nsIArray** aTypes) override;

 private:
  ~MIDIPermissionRequest() = default;

  // Promise for returning MIDIAccess on request success
  RefPtr<Promise> mPromise;
  // True if sysex permissions should be requested
  bool mNeedsSysex;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_MIDIPermissionRequest_h

/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MIDIPermissionRequest_h
#define mozilla_dom_MIDIPermissionRequest_h

#include "mozilla/dom/Promise.h"

namespace mozilla {
namespace dom {

struct MIDIOptions;

/**
 * Handles permission dialog management when requesting MIDI permissions.
 */
class MIDIPermissionRequest final
  : public nsIContentPermissionRequest,
    public nsIRunnable
{
public:
  MIDIPermissionRequest(nsPIDOMWindowInner* aWindow,
                        Promise* aPromise,
                        const MIDIOptions& aOptions);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSICONTENTPERMISSIONREQUEST
  NS_DECL_NSIRUNNABLE
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(MIDIPermissionRequest,
                                           nsIContentPermissionRequest)

private:
  ~MIDIPermissionRequest();

  // Owning window for the permissions requests
  nsCOMPtr<nsPIDOMWindowInner> mWindow;
  // Principal for request
  nsCOMPtr<nsIPrincipal> mPrincipal;
  // Promise for returning MIDIAccess on request success
  RefPtr<Promise> mPromise;
  // True if sysex permissions should be requested
  bool mNeedsSysex;
  // Requester object
  nsCOMPtr<nsIContentPermissionRequester> mRequester;
};

} // namespace dom
} // namespace mozilla

#endif //mozilla_dom_MIDIPermissionRequest_h

/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_GeolocationSystem_h
#define mozilla_dom_GeolocationSystem_h

#include "mozilla/dom/PContentParent.h"
#include "GeolocationIPCUtils.h"

namespace mozilla::dom {

class BrowsingContext;

namespace geolocation {

/**
 * Get the behavior that Gecko should perform when the user asks for
 * geolocation.  The result isn't guaranteed to be accurate on all platforms
 * (for example, some may prompt the user for permission without Gecko's
 * knowledge).  It is, however, guaranteed to be sensible.  For example, this
 * will never return "SystemWillPromptUser" if that is not true, nor will it
 * return "GeckoWillPromptUser" if Gecko doesn't know how to open OS settings.
 */
SystemGeolocationPermissionBehavior GetGeolocationPermissionBehavior();

class SystemGeolocationPermissionRequest {
 public:
  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Stop watching for permission
  virtual void Stop() = 0;

 protected:
  virtual ~SystemGeolocationPermissionRequest() = default;
};

using ParentRequestResolver =
    PContentParent::RequestGeolocationPermissionFromUserResolver;

/**
 * Opens the relevant system dialog to request permission from the user.
 * Resolves aResolver when permission is granted, an error occurs, or Stop has
 * been called on the SystemGeolocationPermissionRequest.
 */
already_AddRefed<SystemGeolocationPermissionRequest>
RequestLocationPermissionFromUser(BrowsingContext* aBrowsingContext,
                                  ParentRequestResolver&& aResolver);

}  // namespace geolocation
}  // namespace mozilla::dom

#endif /* mozilla_dom_GeolocationSystem_h */

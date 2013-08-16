/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Session Storage and Restoration
 *
 * Overview
 * This service keeps track of a user's session, storing the various bits
 * required to return the browser to its current state. The relevant data is
 * stored in memory, and is periodically saved to disk in a file in the
 * profile directory. The service is started at first window load, in
 * delayedStartup, and will restore the session from the data received from
 * the nsSessionStartup service.
 */

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource:///modules/sessionstore/SessionStore.jsm");

function SessionStoreService() {}

// The SessionStore module's object is frozen. We need to modify our prototype
// and add some properties so let's just copy the SessionStore object.
Object.keys(SessionStore).forEach(function (aName) {
  let desc = Object.getOwnPropertyDescriptor(SessionStore, aName);
  Object.defineProperty(SessionStoreService.prototype, aName, desc);
});

SessionStoreService.prototype.classID =
  Components.ID("{5280606b-2510-4fe0-97ef-9b5a22eafe6b}");
SessionStoreService.prototype.QueryInterface =
  XPCOMUtils.generateQI([Ci.nsISessionStore]);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([SessionStoreService]);

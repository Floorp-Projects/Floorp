/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["LoopRooms"];

/**
 * Public Loop Rooms API
 */
this.LoopRooms = Object.freeze({
// Channel ids that will be registered with the PushServer for notifications
  channelIDs: {
    FxA: "6add272a-d316-477c-8335-f00f73dfde71",
    Guest: "19d3f799-a8f3-4328-9822-b7cd02765832",
  },

  onNotification: function(version, channelID) {
    return;
  },
});

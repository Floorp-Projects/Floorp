/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/ObjectWrapper.jsm");

function debug(aMsg) {
  //dump("-- ActivityWrapper.js " + Date.now() + " : " + aMsg + "\n");
}

/**
  * nsISystemMessagesWrapper implementation. Will return a
  * nsIDOMMozActivityRequestHandler
  */
function ActivityWrapper() {
  debug("ActivityWrapper");
}

ActivityWrapper.prototype = {
  wrapMessage: function wrapMessage(aMessage, aWindow) {
    debug("Wrapping " + JSON.stringify(aMessage));
    let handler = Cc["@mozilla.org/dom/activities/request-handler;1"]
                    .createInstance(Ci.nsIDOMMozActivityRequestHandler);
    handler.wrappedJSObject._id = aMessage.id;

    // options is an nsIDOMActivityOptions object.
    var options = handler.wrappedJSObject._options;
    options.wrappedJSObject._name = aMessage.payload.name;
    options.wrappedJSObject._data = ObjectWrapper.wrap(aMessage.payload.data, aWindow);

    return handler;
  },

  classID: Components.ID("{5430d6f9-32d6-4924-ba39-6b6d1b093cd6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivityWrapper]);


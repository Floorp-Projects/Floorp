/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function debug(aMsg) {
  // dump("-- ActivityMessageConfigurator.js " + Date.now() + " : " + aMsg + "\n");
}

/**
  * nsISystemMessagesConfigurator implementation.
  */
function ActivityMessageConfigurator() {
  debug("ActivityMessageConfigurator");
}

ActivityMessageConfigurator.prototype = {
  get mustShowRunningApp() {
    debug("mustShowRunningApp returning true");
    return true;
  },

  classID: Components.ID("{d2296daa-c406-4c5e-b698-e5f2c1715798}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesConfigurator])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivityMessageConfigurator]);

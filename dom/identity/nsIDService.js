/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

this.IDService = function IDService() {
  this.wrappedJSObject = this;
};

this.IDService.prototype = {
  classID: Components.ID("{4e0a0e98-b1d3-4745-a1eb-f815199dd06b}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        Services.obs.addObserver(this, "final-ui-startup", true);
        break;
      case "final-ui-startup":
        // Startup DOMIdentity.jsm
        Cu.import("resource://gre/modules/DOMIdentity.jsm");
        DOMIdentity._init();
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([IDService]);

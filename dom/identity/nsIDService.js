/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function IDService() {
  this.wrappedJSObject = this;
}
IDService.prototype = {
  classID: Components.ID("{baa581e5-8e72-406c-8c9f-dcd4b23a6f82}"),

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

const NSGetFactory = XPCOMUtils.generateNSGetFactory([IDService]);

// An XPCOM service that's registered with the category manager in the parent
// process for handling push notifications with scope "chrome://test-scope"
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

let pushService = Cc["@mozilla.org/push/Service;1"].getService(Ci.nsIPushService);

function PushServiceHandler() {
  // So JS code can reach into us.
  this.wrappedJSObject = this;
  // Register a push observer.
  this.observed = [];
  Services.obs.addObserver(this, pushService.pushTopic, false);
  Services.obs.addObserver(this, pushService.subscriptionChangeTopic, false);
  Services.obs.addObserver(this, pushService.subscriptionModifiedTopic, false);
}

PushServiceHandler.prototype = {
  classID: Components.ID("{bb7c5199-c0f7-4976-9f6d-1306e32c5591}"),
  QueryInterface: XPCOMUtils.generateQI([]),

  observe(subject, topic, data) {
    this.observed.push({ subject, topic, data });
  },
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PushServiceHandler]);

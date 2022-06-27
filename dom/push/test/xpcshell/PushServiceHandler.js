// An XPCOM service that's registered with the category manager in the parent
// process for handling push notifications with scope "chrome://test-scope"
"use strict";

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

let pushService = Cc["@mozilla.org/push/Service;1"].getService(
  Ci.nsIPushService
);

function PushServiceHandler() {
  // So JS code can reach into us.
  this.wrappedJSObject = this;
  // Register a push observer.
  this.observed = [];
  Services.obs.addObserver(this, pushService.pushTopic);
  Services.obs.addObserver(this, pushService.subscriptionChangeTopic);
  Services.obs.addObserver(this, pushService.subscriptionModifiedTopic);
}

PushServiceHandler.prototype = {
  classID: Components.ID("{bb7c5199-c0f7-4976-9f6d-1306e32c5591}"),
  QueryInterface: ChromeUtils.generateQI([]),

  observe(subject, topic, data) {
    this.observed.push({ subject, topic, data });
  },
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([PushServiceHandler]);

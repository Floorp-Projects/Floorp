/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [];

const Cc = Components.classes;
const Ci = Components.interfaces;

const Cu = Components.utils;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this,
                                   "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

var processType = Cc["@mozilla.org/xre/app-info;1"]
                    .getService(Ci.nsIXULRuntime).processType;
var isParent = processType === Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

Services.cpmm.addMessageListener("push", function (aMessage) {
  let {originAttributes, scope, payload} = aMessage.data;
  if (payload) {
    swm.sendPushEvent(originAttributes, scope, payload.length, payload);
  } else {
    swm.sendPushEvent(originAttributes, scope);
  }
});

Services.cpmm.addMessageListener("pushsubscriptionchange", function (aMessage) {
  swm.sendPushSubscriptionChangeEvent(aMessage.data.originAttributes,
                                      aMessage.data.scope);
});

if (!isParent) {
  Services.cpmm.sendAsyncMessage("Push:RegisterEventNotificationListener", null, null, null);
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(this,
                                   "swm",
                                   "@mozilla.org/serviceworkers/manager;1",
                                   "nsIServiceWorkerManager");

addMessageListener("push", function (aMessage) {
  swm.sendPushEvent(aMessage.data.scope, aMessage.data.payload);
});

addMessageListener("pushsubscriptionchanged", function (aMessage) {
  swm.sendPushSubscriptionChangedEvent(aMessage.data);
});
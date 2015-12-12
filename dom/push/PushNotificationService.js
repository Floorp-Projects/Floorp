/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

var isParent = Cc["@mozilla.org/xre/runtime;1"]
                 .getService(Ci.nsIXULRuntime)
                 .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

XPCOMUtils.defineLazyGetter(this, "PushService", function() {
  // Lazily initialize the PushService on
  // `sessionstore-windows-restored` or first use.
  const {PushService} = Cu.import("resource://gre/modules/PushService.jsm", {});
  if (isParent) {
    PushService.init();
  }
  return PushService;
});

this.PushNotificationService = function PushNotificationService() {};

PushNotificationService.prototype = {
  classID: Components.ID("{32028e38-903b-4a64-a180-5857eb4cb3dd}"),

  contractID: "@mozilla.org/push/NotificationService;1",

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PushNotificationService),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference,
                                         Ci.nsIPushNotificationService,
                                         Ci.nsIPushQuotaManager,]),

  register: function register(scope, originAttributes) {
    return PushService.register({
      scope: scope,
      originAttributes: originAttributes,
      maxQuota: Infinity,
    });
  },

  unregister: function unregister(scope, originAttributes) {
    return PushService.unregister({scope, originAttributes});
  },

  registration: function registration(scope, originAttributes) {
    return PushService.registration({scope, originAttributes});
  },

  clearAll: function clearAll() {
    return PushService._clearAll();
  },

  clearForDomain: function(domain) {
    return PushService._clearForDomain(domain);
  },

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        Services.obs.addObserver(this, "sessionstore-windows-restored", true);
        break;
      case "sessionstore-windows-restored":
        Services.obs.removeObserver(this, "sessionstore-windows-restored");
        if (isParent) {
          PushService.init();
        }
        break;
    }
  },

  // nsIPushQuotaManager methods

  notificationForOriginShown: function(origin) {
    if (!isParent) {
      Services.cpmm.sendAsyncMessage("Push:NotificationForOriginShown", origin);
      return;
    }

    PushService._notificationForOriginShown(origin);
  },

  notificationForOriginClosed: function(origin) {
    if (!isParent) {
      Services.cpmm.sendAsyncMessage("Push:NotificationForOriginClosed", origin);
      return;
    }

    PushService._notificationForOriginClosed(origin);
  }
};

this.PushObserverNotification = function PushObserverNotification() {};

PushObserverNotification.prototype = {
  classID: Components.ID("{66a87970-6dc9-46e0-ac61-adb4a13791de}"),

  contractID: "@mozilla.org/push/ObserverNotification;1",

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPushObserverNotification])
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  PushNotificationService,
  PushObserverNotification
]);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsISyncMessageSender");

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

    // This message is useful to communicate that the activity message has been
    // properly received by the app. If the app will be killed, the
    // ActivitiesService will be able to fire an error and complete the
    // Activity workflow.
    cpmm.sendAsyncMessage("Activity:Ready", { id: aMessage.id });

    let handler = Cc["@mozilla.org/dom/activities/request-handler;1"]
                    .createInstance(Ci.nsIDOMMozActivityRequestHandler);
    handler.wrappedJSObject._id = aMessage.id;

    // options is an ActivityOptions dictionary.
   handler.wrappedJSObject._options = Cu.cloneInto({
     name: aMessage.payload.name,
     data: Cu.cloneInto(aMessage.payload.data, aWindow),
   }, aWindow);

    // When the activity window is closed, fire an error to notify the activity
    // caller of the situation.
    // We don't need to check whether the activity itself already sent
    // back something since ActivitiesService.jsm takes care of that.
    let util = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDOMWindowUtils);
    let innerWindowID = util.currentInnerWindowID;

    let observer = {
      observe: function(aSubject, aTopic, aData) {

        switch (aTopic) {
          case 'inner-window-destroyed':
            let wId = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
            if (wId == innerWindowID) {
              debug("Closing activity window " + innerWindowID);
              Services.obs.removeObserver(observer, "inner-window-destroyed");
              cpmm.sendAsyncMessage("Activity:PostError",
                                    { id: aMessage.id,
                                      error: "ActivityCanceled"
                                    });
            }
            break;
          case 'activity-error':
          case 'activity-success':
            if (aData !== aMessage.id) {
              return;
            }
            Services.obs.removeObserver(observer, "activity-error");
            Services.obs.removeObserver(observer, "activity-success");
            let docshell = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIWebNavigation);
            Services.obs.notifyObservers(docshell, "activity-done", aTopic);
            break;
        }
      }
    }

    Services.obs.addObserver(observer, "activity-error", false);
    Services.obs.addObserver(observer, "activity-success", false);
    Services.obs.addObserver(observer, "inner-window-destroyed", false);
    return handler;
  },

  classID: Components.ID("{5430d6f9-32d6-4924-ba39-6b6d1b093cd6}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISystemMessagesWrapper])
}

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ActivityWrapper]);


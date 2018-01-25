"use strict";

var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "cpmm",
                                   "@mozilla.org/childprocessmessagemanager;1",
                                   "nsIMessageSender");

function getNotificationObject(app, id, tag) {
  return {
    origin: "https://" + app + ".gaiamobile.org/",
    id: id,
    title: app + "Notification:" + Date.now(),
    dir: "auto",
    lang: "",
    body: app + " notification body",
    tag: tag || "",
    icon: "icon.png"
  };
}

var systemNotification =
  getNotificationObject("system", "{2bc883bf-2809-4432-b0f4-f54e10372764}");

var calendarNotification =
  getNotificationObject("calendar", "{d8d11299-a58e-429b-9a9a-57c562982fbf}");

// Helper to start the NotificationDB
function startNotificationDB() {
  Cu.import("resource://gre/modules/NotificationDB.jsm");
}

// Helper function to add a listener, send message and treat the reply
function addAndSend(msg, reply, callback, payload, runNext = true) {
  let handler = {
    receiveMessage: function(message) {
      if (message.name === reply) {
        cpmm.removeMessageListener(reply, handler);
        callback(message);
        if (runNext) {
          run_next_test();
        }
      }
    }
  };
  cpmm.addMessageListener(reply, handler);
  cpmm.sendAsyncMessage(msg, payload);
}

// helper fonction, comparing two notifications
function compareNotification(notif1, notif2) {
  // retrieved notification should be the second one sent
  for (let prop in notif1) {
    // compare each property
    Assert.equal(notif1[prop], notif2[prop]);
  }
}

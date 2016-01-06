/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "uuidGenerator",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

XPCOMUtils.defineLazyServiceGetter(this, "notificationStorage",
                                   "@mozilla.org/notificationStorage;1",
                                   "nsINotificationStorage");

XPCOMUtils.defineLazyGetter(this, "cpmm", function() {
  return Cc["@mozilla.org/childprocessmessagemanager;1"]
           .getService(Ci.nsIMessageSender);
});

function debug(str) {
  dump("=*= AlertsService.js : " + str + "\n");
}

// -----------------------------------------------------------------------
// Alerts Service
// -----------------------------------------------------------------------

const kNotificationSystemMessageName = "notification";

const kMessageAppNotificationSend    = "app-notification-send";
const kMessageAppNotificationReturn  = "app-notification-return";
const kMessageAlertNotificationSend  = "alert-notification-send";
const kMessageAlertNotificationClose = "alert-notification-close";

const kTopicAlertShow          = "alertshow";
const kTopicAlertFinished      = "alertfinished";
const kTopicAlertClickCallback = "alertclickcallback";

function AlertsService() {
  Services.obs.addObserver(this, "xpcom-shutdown", false);
  cpmm.addMessageListener(kMessageAppNotificationReturn, this);
}

AlertsService.prototype = {
  classID: Components.ID("{fe33c107-82a4-41d6-8c64-5353267e04c9}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIAlertsService,
                                         Ci.nsIAppNotificationService,
                                         Ci.nsIObserver]),

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        cpmm.removeMessageListener(kMessageAppNotificationReturn, this);
        break;
    }
  },

  // nsIAlertsService
  showAlert: function(aAlert, aAlertListener) {
    if (!aAlert) {
      return;
    }
    cpmm.sendAsyncMessage(kMessageAlertNotificationSend, {
      imageURL: aAlert.imageURL,
      title: aAlert.title,
      text: aAlert.text,
      clickable: aAlert.textClickable,
      cookie: aAlert.cookie,
      listener: aAlertListener,
      id: aAlert.name,
      dir: aAlert.dir,
      lang: aAlert.lang,
      dataStr: aAlert.data,
      inPrivateBrowsing: aAlert.inPrivateBrowsing
    });
  },

  showAlertNotification: function(aImageUrl, aTitle, aText, aTextClickable,
                                  aCookie, aAlertListener, aName, aBidi,
                                  aLang, aDataStr, aPrincipal,
                                  aInPrivateBrowsing) {
    let alert = Cc["@mozilla.org/alert-notification;1"].
      createInstance(Ci.nsIAlertNotification);

    alert.init(aName, aImageUrl, aTitle, aText, aTextClickable, aCookie,
               aBidi, aLang, aDataStr, aPrincipal, aInPrivateBrowsing);

    this.showAlert(alert, aAlertListener);
  },

  closeAlert: function(aName) {
    cpmm.sendAsyncMessage(kMessageAlertNotificationClose, {
      name: aName
    });
  },

  // nsIAppNotificationService
  showAppNotification: function(aImageURL, aTitle, aText, aAlertListener,
                                aDetails) {
    let uid = (aDetails.id == "") ?
          "app-notif-" + uuidGenerator.generateUUID() : aDetails.id;

    let dataObj = this.deserializeStructuredClone(aDetails.data);
    this._listeners[uid] = {
      observer: aAlertListener,
      title: aTitle,
      text: aText,
      manifestURL: aDetails.manifestURL,
      imageURL: aImageURL,
      lang: aDetails.lang || undefined,
      id: aDetails.id || undefined,
      dbId: aDetails.dbId || undefined,
      dir: aDetails.dir || undefined,
      tag: aDetails.tag || undefined,
      timestamp: aDetails.timestamp || undefined,
      dataObj: dataObj || undefined
    };

    cpmm.sendAsyncMessage(kMessageAppNotificationSend, {
      imageURL: aImageURL,
      title: aTitle,
      text: aText,
      uid: uid,
      details: aDetails
    });
  },

  // AlertsService.js custom implementation
  _listeners: [],

  receiveMessage: function(aMessage) {
    let data = aMessage.data;
    let listener = this._listeners[data.uid];
    if (aMessage.name !== kMessageAppNotificationReturn || !listener) {
      return;
    }

    let topic = data.topic;

    try {
      listener.observer.observe(null, topic, null);
    } catch (e) {
      // It seems like there is no callbacks anymore, forward the click on
      // notification via a system message containing the title/text/icon of
      // the notification so the app get a change to react.
      if (data.target) {
        if (topic !== kTopicAlertShow) {
          // excluding the 'show' event: there is no reason a unlaunched app
          // would want to be notified that a notification is shown. This
          // happens when a notification is still displayed at reboot time.
          gSystemMessenger.sendMessage(kNotificationSystemMessageName, {
              clicked: (topic === kTopicAlertClickCallback),
              title: listener.title,
              body: listener.text,
              imageURL: listener.imageURL,
              lang: listener.lang,
              dir: listener.dir,
              id: listener.id,
              tag: listener.tag,
              timestamp: listener.timestamp,
              data: listener.dataObj || undefined,
            },
            Services.io.newURI(data.target, null, null),
            Services.io.newURI(listener.manifestURL, null, null)
          );
        }
      }
      if (topic === kTopicAlertFinished && listener.dbId) {
        notificationStorage.delete(listener.manifestURL, listener.dbId);
      }
    }

    // we're done with this notification
    if (topic === kTopicAlertFinished) {
      delete this._listeners[data.uid];
    }
  },

  deserializeStructuredClone: function(dataString) {
    if (!dataString) {
      return null;
    }
    let scContainer = Cc["@mozilla.org/docshell/structured-clone-container;1"].
      createInstance(Ci.nsIStructuredCloneContainer);

    // The maximum supported structured-clone serialization format version
    // as defined in "js/public/StructuredClone.h"
    let JS_STRUCTURED_CLONE_VERSION = 4;
    scContainer.initFromBase64(dataString, JS_STRUCTURED_CLONE_VERSION);
    let dataObj = scContainer.deserializeToVariant();

    // We have to check whether dataObj contains DOM objects (supported by
    // nsIStructuredCloneContainer, but not by Cu.cloneInto), e.g. ImageData.
    // After the structured clone callback systems will be unified, we'll not
    // have to perform this check anymore.
    try {
      let data = Cu.cloneInto(dataObj, {});
    } catch(e) { dataObj = null; }

    return dataObj;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AlertsService]);

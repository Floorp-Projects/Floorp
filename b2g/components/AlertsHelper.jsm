/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [];

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AppsUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gSystemMessenger",
                                   "@mozilla.org/system-message-internal;1",
                                   "nsISystemMessagesInternal");

XPCOMUtils.defineLazyServiceGetter(this, "appsService",
                                   "@mozilla.org/AppsService;1",
                                   "nsIAppsService");

XPCOMUtils.defineLazyModuleGetter(this, "SystemAppProxy",
                                  "resource://gre/modules/SystemAppProxy.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "notificationStorage",
                                   "@mozilla.org/notificationStorage;1",
                                   "nsINotificationStorage");

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
         .getService(Ci.nsIMessageListenerManager);
});

function debug(str) {
  //dump("=*= AlertsHelper.jsm : " + str + "\n");
}

const kNotificationIconSize = 128;

const kDesktopNotificationPerm = "desktop-notification";

const kNotificationSystemMessageName = "notification";

const kDesktopNotification      = "desktop-notification";
const kDesktopNotificationShow  = "desktop-notification-show";
const kDesktopNotificationClick = "desktop-notification-click";
const kDesktopNotificationClose = "desktop-notification-close";

const kTopicAlertClickCallback = "alertclickcallback";
const kTopicAlertShow          = "alertshow";
const kTopicAlertFinished      = "alertfinished";

const kMozChromeNotificationEvent  = "mozChromeNotificationEvent";
const kMozContentNotificationEvent = "mozContentNotificationEvent";

const kMessageAppNotificationSend    = "app-notification-send";
const kMessageAppNotificationReturn  = "app-notification-return";
const kMessageAlertNotificationSend  = "alert-notification-send";
const kMessageAlertNotificationClose = "alert-notification-close";

const kMessages = [
  kMessageAppNotificationSend,
  kMessageAlertNotificationSend,
  kMessageAlertNotificationClose
];

var AlertsHelper = {

  _listeners: {},

  init: function() {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    for (let message of kMessages) {
      ppmm.addMessageListener(message, this);
    }
    SystemAppProxy.addEventListener(kMozContentNotificationEvent, this);
  },

  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        Services.obs.removeObserver(this, "xpcom-shutdown");
        for (let message of kMessages) {
          ppmm.removeMessageListener(message, this);
        }
        SystemAppProxy.removeEventListener(kMozContentNotificationEvent, this);
        break;
    }
  },

  handleEvent: function(evt) {
    let detail = evt.detail;

    switch(detail.type) {
      case kDesktopNotificationShow:
      case kDesktopNotificationClick:
      case kDesktopNotificationClose:
        this.handleNotificationEvent(detail);
        break;
      default:
        debug("FIXME: Unhandled notification event: " + detail.type);
        break;
    }
  },

  handleNotificationEvent: function(detail) {
    if (!detail || !detail.id) {
      return;
    }

    let uid = detail.id;
    let listener = this._listeners[uid];
    if (!listener) {
      return;
    }

    let topic;
    if (detail.type === kDesktopNotificationClick) {
      topic = kTopicAlertClickCallback;
    } else if (detail.type === kDesktopNotificationShow) {
      topic = kTopicAlertShow;
    } else {
      /* kDesktopNotificationClose */
      topic = kTopicAlertFinished;
    }

    if (listener.cookie) {
      try {
        listener.observer.observe(null, topic, listener.cookie);
      } catch (e) { }
    } else {
      try {
        listener.mm.sendAsyncMessage(kMessageAppNotificationReturn, {
          uid: uid,
          topic: topic,
          target: listener.target
        });
      } catch (e) {
        // we get an exception if the app is not launched yet
        if (detail.type !== kDesktopNotificationShow) {
          // excluding the 'show' event: there is no reason a unlaunched app
          // would want to be notified that a notification is shown. This
          // happens when a notification is still displayed at reboot time.
          gSystemMessenger.sendMessage(kNotificationSystemMessageName, {
              clicked: (detail.type === kDesktopNotificationClick),
              title: listener.title,
              body: listener.text,
              imageURL: listener.imageURL,
              lang: listener.lang,
              dir: listener.dir,
              id: listener.id,
              tag: listener.tag,
              timestamp: listener.timestamp,
              data: listener.dataObj
            },
            Services.io.newURI(listener.target, null, null),
            Services.io.newURI(listener.manifestURL, null, null)
          );
        }
      }
      if (detail.type === kDesktopNotificationClose && listener.dbId) {
        notificationStorage.delete(listener.manifestURL, listener.dbId);
      }
    }

    // we"re done with this notification
    if (detail.type === kDesktopNotificationClose) {
      delete this._listeners[uid];
    }
  },

  registerListener: function(alertId, cookie, alertListener) {
    this._listeners[alertId] = { observer: alertListener, cookie: cookie };
  },

  registerAppListener: function(uid, listener) {
    this._listeners[uid] = listener;

    appsService.getManifestFor(listener.manifestURL).then((manifest) => {
      let app = appsService.getAppByManifestURL(listener.manifestURL);
      let helper = new ManifestHelper(manifest, app.origin, app.manifestURL);
      let getNotificationURLFor = function(messages) {
        if (!messages) {
          return null;
        }

        for (let i = 0; i < messages.length; i++) {
          let message = messages[i];
          if (message === kNotificationSystemMessageName) {
            return helper.fullLaunchPath();
          } else if (typeof message === "object" &&
                     kNotificationSystemMessageName in message) {
            return helper.resolveURL(message[kNotificationSystemMessageName]);
          }
        }

        // No message found...
        return null;
      }

      listener.target = getNotificationURLFor(manifest.messages);

      // Bug 816944 - Support notification messages for entry_points.
    });
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
  },

  showNotification: function(imageURL, title, text, textClickable, cookie,
                             uid, dir, lang, dataObj, manifestURL, timestamp,
                             behavior) {
    function send(appName, appIcon) {
      SystemAppProxy._sendCustomEvent(kMozChromeNotificationEvent, {
        type: kDesktopNotification,
        id: uid,
        icon: imageURL,
        title: title,
        text: text,
        dir: dir,
        lang: lang,
        appName: appName,
        appIcon: appIcon,
        manifestURL: manifestURL,
        timestamp: timestamp,
        data: dataObj,
        mozbehavior: behavior
      });
    }

    if (!manifestURL || !manifestURL.length) {
      send(null, null);
      return;
    }

    // If we have a manifest URL, get the icon and title from the manifest
    // to prevent spoofing.
    appsService.getManifestFor(manifestURL).then((manifest) => {
      let app = appsService.getAppByManifestURL(manifestURL);
      let helper = new ManifestHelper(manifest, app.origin, manifestURL);
      send(helper.name, helper.iconURLForSize(kNotificationIconSize));
    });
  },

  showAlertNotification: function(aMessage) {
    let data = aMessage.data;
    let currentListener = this._listeners[data.name];
    if (currentListener && currentListener.observer) {
      currentListener.observer.observe(null, kTopicAlertFinished, currentListener.cookie);
    }

    let dataObj = this.deserializeStructuredClone(data.dataStr);
    this.registerListener(data.name, data.cookie, data.alertListener);
    this.showNotification(data.imageURL, data.title, data.text,
                          data.textClickable, data.cookie, data.name, data.dir,
                          data.lang, dataObj, null, data.inPrivateBrowsing);
  },

  showAppNotification: function(aMessage) {
    let data = aMessage.data;
    let details = data.details;
    let dataObject = this.deserializeStructuredClone(details.data);
    let listener = {
      mm: aMessage.target,
      title: data.title,
      text: data.text,
      manifestURL: details.manifestURL,
      imageURL: data.imageURL,
      lang: details.lang || undefined,
      id: details.id || undefined,
      dbId: details.dbId || undefined,
      dir: details.dir || undefined,
      tag: details.tag || undefined,
      timestamp: details.timestamp || undefined,
      dataObj: dataObject || undefined
    };
    this.registerAppListener(data.uid, listener);
    this.showNotification(data.imageURL, data.title, data.text,
                          details.textClickable, null, data.uid, details.dir,
                          details.lang, dataObject, details.manifestURL,
                          details.timestamp, details.mozbehavior);
  },

  closeAlert: function(name) {
    SystemAppProxy._sendCustomEvent(kMozChromeNotificationEvent, {
      type: kDesktopNotificationClose,
      id: name
    });
  },

  receiveMessage: function(aMessage) {
    if (!aMessage.target.assertAppHasPermission(kDesktopNotificationPerm)) {
      Cu.reportError("Desktop-notification message " + aMessage.name +
                     " from a content process with no " + kDesktopNotificationPerm +
                     " privileges.");
      return;
    }

    switch(aMessage.name) {
      case kMessageAppNotificationSend:
        this.showAppNotification(aMessage);
        break;

      case kMessageAlertNotificationSend:
        this.showAlertNotification(aMessage);
        break;

      case kMessageAlertNotificationClose:
        this.closeAlert(aMessage.data.name);
        break;
    }

  },
}

AlertsHelper.init();

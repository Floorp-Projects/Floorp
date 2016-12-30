/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This is the implementation of nsIPresentationDevicePrompt XPCOM.
 * It will be registered into a XPCOM component by Presentation.jsm.
 *
 * This component will prompt a device selection UI for users to choose which
 * devices they want to connect, when PresentationRequest is started.
 */

"use strict";

var EXPORTED_SYMBOLS = ["PresentationDevicePrompt"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// An string bundle for localization.
XPCOMUtils.defineLazyGetter(this, "Strings", function() {
  return Services.strings.createBundle("chrome://presentation/locale/presentation.properties");
});
// To generate a device selection prompt.
XPCOMUtils.defineLazyModuleGetter(this, "PermissionUI",
                                        "resource:///modules/PermissionUI.jsm");
/*
 * Utils
 */
function log(aMsg) {
  // Prefix is useful to grep log.
  // dump("@ PresentationDevicePrompt: " + aMsg + "\n");
}

function GetString(aName) {
  return Strings.GetStringFromName(aName);
}

/*
 * Device Selection UI
 */
const kNotificationId = "presentation-device-selection";
const kNotificationPopupIcon = "chrome://presentation-shared/skin/link.svg";

// There is no dependancy between kNotificationId and kNotificationAnchorId,
// so it's NOT necessary to name them by same prefix
// (e.g., presentation-device-selection-notification-icon).
const kNotificationAnchorId = "presentation-device-notification-icon";
const kNotificationAnchorIcon = "chrome://presentation-shared/skin/link.svg";

// This will insert our own popupnotification content with the device list
// into the displayed popupnotification element.
// PopupNotifications.jsm will automatically generate a popupnotification
// element whose id is <notification id> + "-notification" and show it,
// so kPopupNotificationId must be kNotificationId + "-notification".
// Read more detail in PopupNotifications._refreshPanel.
const kPopupNotificationId = kNotificationId + "-notification";

function PresentationPermissionPrompt(aRequest, aDevices) {
  this.request = aRequest;
  this._isResponded = false;
  this._devices = aDevices;
}

PresentationPermissionPrompt.prototype = {
  __proto__: PermissionUI.PermissionPromptForRequestPrototype,
  // PUBLIC APIs
  get browser() {
    return this.request.chromeEventHandler;
  },
  get principal() {
    return this.request.principal;
  },
  get popupOptions() {
    return {
      removeOnDismissal: true,
      popupIconURL: kNotificationPopupIcon, // Icon shown on prompt content
      eventCallback: (aTopic, aNewBrowser) => {
        log("eventCallback: " + aTopic);
        let handler = {
          // dismissed: () => { // Won't be fired if removeOnDismissal is true.
          //   log("Dismissed by user. Cancel the request.");
          // },
          removed: () => {
            log("Prompt is removed.");
            if (!this._isResponded) {
              log("Dismissed by user. Cancel the request.");
              this.request.cancel(Cr.NS_ERROR_NOT_AVAILABLE);
            }
          },
          showing: () => {
            log("Prompt is showing.");
            // We cannot insert the device list at "showing" phase because
            // the popupnotification content whose id is kPopupNotificationId
            // is not generated yet.
          },
          shown: () => {
            log("Prompt is shown.");
            // Insert device selection list into popupnotification element.
            this._createPopupContent();
          },
        };

        // Call the handler for Notification events.
        handler[aTopic]();
      },
    };
  },
  get notificationID() {
    return kNotificationId;
  },
  get anchorID() {
    let chromeDoc = this.browser.ownerDocument;
    let anchor = chromeDoc.getElementById(kNotificationAnchorId);
    if (!anchor) {
      let notificationPopupBox =
        chromeDoc.getElementById("notification-popup-box");
      // Icon shown on URL bar
      let notificationIcon = chromeDoc.createElement("image");
      notificationIcon.id = kNotificationAnchorId;
      notificationIcon.setAttribute("src", kNotificationAnchorIcon);
      notificationIcon.classList.add("notification-anchor-icon");
      notificationIcon.setAttribute("role", "button");
      notificationIcon.setAttribute("tooltiptext",
                                    GetString("presentation.urlbar.tooltiptext"));
      notificationIcon.style.filter = "url('chrome://global/skin/filters.svg#fill')";
      notificationIcon.style.fill = "currentcolor";
      notificationIcon.style.opacity = "0.4";
      notificationPopupBox.appendChild(notificationIcon);
    }

    return kNotificationAnchorId;
  },
  get message() {
    return GetString("presentation.message", this._domainName);
  },
  get promptActions() {
    return [{
      label: GetString("presentation.deviceprompt.select.label"),
      accessKey: GetString("presentation.deviceprompt.select.accessKey"),
      callback: () => {
        log("Select");
        this._isResponded = true;
        if (!this._listbox || !this._devices.length) {
          log("No device can be selected!");
          this.request.cancel(Cr.NS_ERROR_NOT_AVAILABLE);
          return;
        }
        let device = this._devices[this._listbox.selectedIndex];
        this.request.select(device);
        log("device: " + device.name + "(" + device.id + ") is selected!");
      },
    }, {
      label: GetString("presentation.deviceprompt.cancel.label"),
      accessKey: GetString("presentation.deviceprompt.cancel.accessKey"),
      callback: () => {
        log("Cancel selection.");
        this._isResponded = true;
        this.request.cancel(Cr.NS_ERROR_NOT_AVAILABLE);
      },
      dismiss: true,
    }];
  },
  // PRIVATE APIs
  get _domainName() {
    if (this.principal.URI instanceof Ci.nsIFileURL) {
      return this.principal.URI.path.split('/')[1];
    }
    return this.principal.URI.hostPort;
  },
  _createPopupContent() {
    log("_createPopupContent");

    if (!this._devices.length) {
      log("No available devices can be listed!");
      return;
    }

    let chromeDoc = this.browser.ownerDocument;

    let popupnotification = chromeDoc.getElementById(kPopupNotificationId);
    if (!popupnotification) {
      log("No available popupnotification element to be inserted!");
      return;
    }

    let popupnotificationcontent =
      chromeDoc.createElement("popupnotificationcontent");

    this._listbox = chromeDoc.createElement("richlistbox");
    this._listbox.setAttribute("flex", "1");
    this._devices.forEach((device) => {
      let listitem = chromeDoc.createElement("richlistitem");
      let label = chromeDoc.createElement("label");
      label.setAttribute("value", device.name);
      listitem.appendChild(label);
      this._listbox.appendChild(listitem);
    });

    popupnotificationcontent.appendChild(this._listbox);
    popupnotification.appendChild(popupnotificationcontent);
  },
};


/*
 * nsIPresentationDevicePrompt
 */
// For XPCOM registration
const PRESENTATIONDEVICEPROMPT_CONTRACTID = "@mozilla.org/presentation-device/prompt;1";
const PRESENTATIONDEVICEPROMPT_CID        = Components.ID("{388bd149-c919-4a43-b646-d7ec57877689}");

function PresentationDevicePrompt() {}

PresentationDevicePrompt.prototype = {
  // properties required for XPCOM registration:
  classID: PRESENTATIONDEVICEPROMPT_CID,
  classDescription: "Presentation API Device Prompt",
  contractID: PRESENTATIONDEVICEPROMPT_CONTRACTID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIPresentationDevicePrompt]),

  // This will be fired when window.PresentationRequest(URL).start() is called.
  promptDeviceSelection(aRequest) {
    log("promptDeviceSelection");

    // Cancel request if no available device.
    let devices = this._loadDevices();
    if (!devices.length) {
      log("No available device.");
      aRequest.cancel(Cr.NS_ERROR_NOT_AVAILABLE);
      return;
    }

    // Show the prompt to users.
    let promptUI = new PresentationPermissionPrompt(aRequest, devices);
    promptUI.prompt();
  },
  _loadDevices() {
    let deviceManager = Cc["@mozilla.org/presentation-device/manager;1"]
                        .getService(Ci.nsIPresentationDeviceManager);
    let devices = deviceManager.getAvailableDevices().QueryInterface(Ci.nsIArray);
    let list = [];
    for (let i = 0; i < devices.length; i++) {
      let device = devices.queryElementAt(i, Ci.nsIPresentationDevice);
      list.push(device);
    }

    return list;
  },
};

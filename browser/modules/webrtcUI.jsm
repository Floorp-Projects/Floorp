/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let EXPORTED_SYMBOLS = ["webrtcUI"];

const Cu = Components.utils;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");

let webrtcUI = {
  init: function () {
    Services.obs.addObserver(handleRequest, "getUserMedia:request", false);
  },
  uninit: function () {
    Services.obs.removeObserver(handleRequest, "getUserMedia:request");
  }
}

function handleRequest(aSubject, aTopic, aData) {
  let {windowID: windowID, callID: callID} = JSON.parse(aData);

  let someWindow = Services.wm.getMostRecentWindow(null);
  let contentWindow = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils)
                                .getOuterWindowWithId(windowID);
  let browser = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                             .getInterface(Ci.nsIWebNavigation)
                             .QueryInterface(Ci.nsIDocShell)
                             .chromeEventHandler;

  let params = aSubject.QueryInterface(Ci.nsIMediaStreamOptions);

  browser.ownerDocument.defaultView.navigator.mozGetUserMediaDevices(
    function (devices) {
      prompt(browser, callID, params.audio, params.video, devices);
    },
    function (error) {
      Cu.reportError(error);
    }
  );
}

function prompt(aBrowser, aCallID, aAudioRequested, aVideoRequested, aDevices) {
  let audioDevices = [];
  let videoDevices = [];
  for (let device of aDevices) {
    device = device.QueryInterface(Ci.nsIMediaDevice);
    switch (device.type) {
      case "audio":
        if (aAudioRequested)
          audioDevices.push(device);
        break;
      case "video":
        if (aVideoRequested)
          videoDevices.push(device);
        break;
    }
  }

  let requestType;
  if (audioDevices.length && videoDevices.length)
    requestType = "shareCameraAndMicrophone";
  else if (audioDevices.length)
    requestType = "shareMicrophone";
  else if (videoDevices.length)
    requestType = "shareCamera";
  else
    return;

  let host = aBrowser.contentDocument.documentURIObject.asciiHost;
  let chromeWin = aBrowser.ownerDocument.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;
  let message = stringBundle.getFormattedString("getUserMedia." + requestType + ".message",
                                                [ host ]);

  let responseSent = false;

  let mainAction = {
    label: stringBundle.getString("getUserMedia." + requestType + ".label"),
    accessKey: stringBundle.getString("getUserMedia." + requestType + ".accesskey"),
    callback: function () {
      Services.obs.notifyObservers(null, "getUserMedia:response:allow", aCallID);
      responseSent = true;
    }
  };

  let secondaryActions = [];
  let selectableDevices = videoDevices.length ? videoDevices : audioDevices;
  if (selectableDevices.length > 1) {
    let selectableDeviceNumber = 0;
    for (let device of selectableDevices) {
      selectableDeviceNumber++;
      secondaryActions.push({
        label: stringBundle.getFormattedString(
                 device.type == "audio" ?
                   "getUserMedia.shareSpecificMicrophone.label" :
                   "getUserMedia.shareSpecificCamera.label",
                 [ device.name ]),
        accessKey: selectableDeviceNumber,
        callback: function () {
          Services.obs.notifyObservers(device, "getUserMedia:response:allow", aCallID);
          responseSent = true;
        }
      });
    }
  }

  let options = {
    removeOnDismissal: true,
    eventCallback: function (aType) {
      if (!responseSent && aType == "removed")
        Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
    }
  };

  chromeWin.PopupNotifications.show(aBrowser, "webRTC-shareDevices", message,
                                    "webRTC-notification-icon", mainAction,
                                    secondaryActions, options);
}

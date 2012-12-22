/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["webrtcUI"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PluralForm.jsm");

this.webrtcUI = {
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
  let chromeDoc = aBrowser.ownerDocument;
  let chromeWin = chromeDoc.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;
  let message = stringBundle.getFormattedString("getUserMedia." + requestType + ".message",
                                                [ host ]);

  function listDevices(menupopup, devices) {
    while (menupopup.lastChild)
      menupopup.removeChild(menupopup.lastChild);

    let deviceIndex = 0;
    for (let device of devices) {
      let menuitem = chromeDoc.createElement("menuitem");
      menuitem.setAttribute("value", deviceIndex);
      menuitem.setAttribute("label", device.name);
      menuitem.setAttribute("tooltiptext", device.name);
      menupopup.appendChild(menuitem);
      deviceIndex++;
    }
  }

  chromeDoc.getElementById("webRTC-selectCamera").hidden = !videoDevices.length;
  chromeDoc.getElementById("webRTC-selectMicrophone").hidden = !audioDevices.length;
  listDevices(chromeDoc.getElementById("webRTC-selectCamera-menupopup"), videoDevices);
  listDevices(chromeDoc.getElementById("webRTC-selectMicrophone-menupopup"), audioDevices);

  let mainAction = {
    label: PluralForm.get(requestType == "shareCameraAndMicrophone" ? 2 : 1,
                          stringBundle.getString("getUserMedia.shareSelectedDevices.label")),
    accessKey: stringBundle.getString("getUserMedia.shareSelectedDevices.accesskey"),
    callback: function () {
      let allowedDevices = Cc["@mozilla.org/supports-array;1"]
                             .createInstance(Ci.nsISupportsArray);
      if (videoDevices.length) {
        let videoDeviceIndex = chromeDoc.getElementById("webRTC-selectCamera-menulist").value;
        allowedDevices.AppendElement(videoDevices[videoDeviceIndex]);
      }
      if (audioDevices.length) {
        let audioDeviceIndex = chromeDoc.getElementById("webRTC-selectMicrophone-menulist").value;
        allowedDevices.AppendElement(audioDevices[audioDeviceIndex]);
      }
      Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
    }
  };

  let secondaryActions = [{
    label: stringBundle.getString("getUserMedia.denyRequest.label"),
    accessKey: stringBundle.getString("getUserMedia.denyRequest.accesskey"),
    callback: function () {
      Services.obs.notifyObservers(null, "getUserMedia:response:deny", aCallID);
    }
  }];

  let options = {
  };

  chromeWin.PopupNotifications.show(aBrowser, "webRTC-shareDevices", message,
                                    "webRTC-notification-icon", mainAction,
                                    secondaryActions, options);
}

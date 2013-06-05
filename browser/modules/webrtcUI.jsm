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
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

this.webrtcUI = {
  init: function () {
    Services.obs.addObserver(handleRequest, "getUserMedia:request", false);
    Services.obs.addObserver(updateIndicators, "recording-device-events", false);
    Services.obs.addObserver(removeBrowserSpecificIndicator, "recording-window-ended", false);
  },

  uninit: function () {
    Services.obs.removeObserver(handleRequest, "getUserMedia:request");
    Services.obs.removeObserver(updateIndicators, "recording-device-events");
    Services.obs.removeObserver(removeBrowserSpecificIndicator, "recording-window-ended");
  },

  showGlobalIndicator: false,

  get activeStreams() {
    let contentWindowSupportsArray = MediaManagerService.activeMediaCaptureWindows;
    let count = contentWindowSupportsArray.Count();
    let activeStreams = [];
    for (let i = 0; i < count; i++) {
      let contentWindow = contentWindowSupportsArray.GetElementAt(i);
      let browser = getBrowserForWindow(contentWindow);
      let browserWindow = browser.ownerDocument.defaultView;
      let tab = browserWindow.gBrowser &&
                browserWindow.gBrowser._getTabForContentWindow(contentWindow.top);
      activeStreams.push({
        uri: contentWindow.location.href,
        tab: tab,
        browser: browser
      });
    }
    return activeStreams;
  }
}

function getBrowserForWindowId(aWindowID) {
  return getBrowserForWindow(Services.wm.getOuterWindowWithId(aWindowID));
}

function getBrowserForWindow(aContentWindow) {
  return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebNavigation)
                       .QueryInterface(Ci.nsIDocShell)
                       .chromeEventHandler;
}

function handleRequest(aSubject, aTopic, aData) {
  let {windowID: windowID, callID: callID} = JSON.parse(aData);

  let params = aSubject.QueryInterface(Ci.nsIMediaStreamOptions);

  Services.wm.getMostRecentWindow(null).navigator.mozGetUserMediaDevices(
    function (devices) {
      prompt(windowID, callID, params.audio, params.video || params.picture, devices);
    },
    function (error) {
      // bug 827146 -- In the future, the UI should catch NO_DEVICES_FOUND
      // and allow the user to plug in a device, instead of immediately failing.
      denyRequest(callID, error);
    }
  );
}

function denyRequest(aCallID, aError) {
  let msg = null;
  if (aError) {
    msg = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    msg.data = aError;
  }
  Services.obs.notifyObservers(msg, "getUserMedia:response:deny", aCallID);
}

function prompt(aWindowID, aCallID, aAudioRequested, aVideoRequested, aDevices) {
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
    requestType = "CameraAndMicrophone";
  else if (audioDevices.length)
    requestType = "Microphone";
  else if (videoDevices.length)
    requestType = "Camera";
  else {
    denyRequest(aCallID, "NO_DEVICES_FOUND");
    return;
  }

  let contentWindow = Services.wm.getOuterWindowWithId(aWindowID);
  let host = contentWindow.document.documentURIObject.asciiHost;
  let browser = getBrowserForWindow(contentWindow);
  let chromeDoc = browser.ownerDocument;
  let chromeWin = chromeDoc.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;
  let message = stringBundle.getFormattedString("getUserMedia.share" + requestType + ".message",
                                                [ host ]);

  function listDevices(menupopup, devices) {
    while (menupopup.lastChild)
      menupopup.removeChild(menupopup.lastChild);

    let deviceIndex = 0;
    for (let device of devices) {
      addDeviceToList(menupopup, device.name, deviceIndex);
      deviceIndex++;
    }
  }

  function addDeviceToList(menupopup, deviceName, deviceIndex) {
    let menuitem = chromeDoc.createElement("menuitem");
    menuitem.setAttribute("value", deviceIndex);
    menuitem.setAttribute("label", deviceName);
    menuitem.setAttribute("tooltiptext", deviceName);
    menupopup.appendChild(menuitem);
  }

  chromeDoc.getElementById("webRTC-selectCamera").hidden = !videoDevices.length;
  chromeDoc.getElementById("webRTC-selectMicrophone").hidden = !audioDevices.length;

  let camMenupopup = chromeDoc.getElementById("webRTC-selectCamera-menupopup");
  let micMenupopup = chromeDoc.getElementById("webRTC-selectMicrophone-menupopup");
  listDevices(camMenupopup, videoDevices);
  listDevices(micMenupopup, audioDevices);
  if (requestType == "CameraAndMicrophone") {
    addDeviceToList(camMenupopup, stringBundle.getString("getUserMedia.noVideo.label"), "-1");
    addDeviceToList(micMenupopup, stringBundle.getString("getUserMedia.noAudio.label"), "-1");
  }

  let mainAction = {
    label: PluralForm.get(requestType == "CameraAndMicrophone" ? 2 : 1,
                          stringBundle.getString("getUserMedia.shareSelectedDevices.label")),
    accessKey: stringBundle.getString("getUserMedia.shareSelectedDevices.accesskey"),
    callback: function () {
      let allowedDevices = Cc["@mozilla.org/supports-array;1"]
                             .createInstance(Ci.nsISupportsArray);
      if (videoDevices.length) {
        let videoDeviceIndex = chromeDoc.getElementById("webRTC-selectCamera-menulist").value;
        if (videoDeviceIndex != "-1")
          allowedDevices.AppendElement(videoDevices[videoDeviceIndex]);
      }
      if (audioDevices.length) {
        let audioDeviceIndex = chromeDoc.getElementById("webRTC-selectMicrophone-menulist").value;
        if (audioDeviceIndex != "-1")
          allowedDevices.AppendElement(audioDevices[audioDeviceIndex]);
      }

      if (allowedDevices.Count() == 0) {
        denyRequest(aCallID);
        return;
      }

      Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
    }
  };

  let secondaryActions = [{
    label: stringBundle.getString("getUserMedia.denyRequest.label"),
    accessKey: stringBundle.getString("getUserMedia.denyRequest.accesskey"),
    callback: function () {
      denyRequest(aCallID);
    }
  }];

  let options = null;

  chromeWin.PopupNotifications.show(browser, "webRTC-shareDevices", message,
                                    "webRTC-shareDevices-notification-icon", mainAction,
                                    secondaryActions, options);
}

function updateIndicators() {
  webrtcUI.showGlobalIndicator =
    MediaManagerService.activeMediaCaptureWindows.Count() > 0;

  let e = Services.wm.getEnumerator("navigator:browser");
  while (e.hasMoreElements())
    e.getNext().WebrtcIndicator.updateButton();

  for (let {browser: browser} of webrtcUI.activeStreams)
    showBrowserSpecificIndicator(browser);
}

function showBrowserSpecificIndicator(aBrowser) {
  let hasVideo = {};
  let hasAudio = {};
  MediaManagerService.mediaCaptureWindowState(aBrowser.contentWindow,
                                              hasVideo, hasAudio);
  let captureState;
  if (hasVideo.value && hasAudio.value) {
    captureState = "CameraAndMicrophone";
  } else if (hasVideo.value) {
    captureState = "Camera";
  } else if (hasAudio.value) {
    captureState = "Microphone";
  } else {
    Cu.reportError("showBrowserSpecificIndicator: got neither video nor audio access");
    return;
  }

  let chromeWin = aBrowser.ownerDocument.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;

  let message = stringBundle.getString("getUserMedia.sharing" + captureState + ".message2");
  let mainAction = null;
  let secondaryActions = null;
  let options = {
    dismissed: true
  };
  chromeWin.PopupNotifications.show(aBrowser, "webRTC-sharingDevices", message,
                                    "webRTC-sharingDevices-notification-icon", mainAction,
                                    secondaryActions, options);
}

function removeBrowserSpecificIndicator(aSubject, aTopic, aData) {
  let browser = getBrowserForWindowId(aData);
  let PopupNotifications = browser.ownerDocument.defaultView.PopupNotifications;
  let notification = PopupNotifications &&
                     PopupNotifications.getNotification("webRTC-sharingDevices",
                                                        browser);
  if (notification)
    PopupNotifications.remove(notification);
}

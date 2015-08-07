/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

this.EXPORTED_SYMBOLS = [ "ContentWebRTC" ];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

this.ContentWebRTC = {
  _initialized: false,

  init: function() {
    if (this._initialized)
      return;

    this._initialized = true;
    Services.obs.addObserver(handleGUMRequest, "getUserMedia:request", false);
    Services.obs.addObserver(handlePCRequest, "PeerConnection:request", false);
    Services.obs.addObserver(updateIndicators, "recording-device-events", false);
    Services.obs.addObserver(removeBrowserSpecificIndicator, "recording-window-ended", false);
  },

  // Called only for 'unload' to remove pending gUM prompts in reloaded frames.
  handleEvent: function(aEvent) {
    let contentWindow = aEvent.target.defaultView;
    let mm = getMessageManagerForWindow(contentWindow);
    for (let key of contentWindow.pendingGetUserMediaRequests.keys()) {
      mm.sendAsyncMessage("webrtc:CancelRequest", key);
    }
    for (let key of contentWindow.pendingPeerConnectionRequests.keys()) {
      mm.sendAsyncMessage("rtcpeer:CancelRequest", key);
    }
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "rtcpeer:Allow":
      case "rtcpeer:Deny": {
        let callID = aMessage.data.callID;
        let contentWindow = Services.wm.getOuterWindowWithId(aMessage.data.windowID);
        forgetPCRequest(contentWindow, callID);
        let topic = (aMessage.name == "rtcpeer:Allow") ? "PeerConnection:response:allow" :
                                                         "PeerConnection:response:deny";
        Services.obs.notifyObservers(null, topic, callID);
        break;
      }
      case "webrtc:Allow": {
        let callID = aMessage.data.callID;
        let contentWindow = Services.wm.getOuterWindowWithId(aMessage.data.windowID);
        let devices = contentWindow.pendingGetUserMediaRequests.get(callID);
        forgetGUMRequest(contentWindow, callID);

        let allowedDevices = Cc["@mozilla.org/supports-array;1"]
                               .createInstance(Ci.nsISupportsArray);
        for (let deviceIndex of aMessage.data.devices)
           allowedDevices.AppendElement(devices[deviceIndex]);

        Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", callID);
        break;
      }
      case "webrtc:Deny":
        denyGUMRequest(aMessage.data);
        break;
      case "webrtc:StopSharing":
        Services.obs.notifyObservers(null, "getUserMedia:revoke", aMessage.data);
        break;
    }
  }
};

function handlePCRequest(aSubject, aTopic, aData) {
  // Need to access JS object behind XPCOM wrapper added by observer API (using a
  // WebIDL interface didn't work here as object comes from JSImplemented code).
  aSubject = aSubject.wrappedJSObject;
  let { windowID, callID, isSecure } = aSubject;
  let contentWindow = Services.wm.getOuterWindowWithId(windowID);

  if (!contentWindow.pendingPeerConnectionRequests) {
    setupPendingListsInitially(contentWindow);
  }
  contentWindow.pendingPeerConnectionRequests.add(callID);

  let request = {
    callID: callID,
    windowID: windowID,
    documentURI: contentWindow.document.documentURI,
    secure: isSecure,
  };

  let mm = getMessageManagerForWindow(contentWindow);
  mm.sendAsyncMessage("rtcpeer:Request", request);
}

function handleGUMRequest(aSubject, aTopic, aData) {
  let constraints = aSubject.getConstraints();
  let secure = aSubject.isSecure;
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  contentWindow.navigator.mozGetUserMediaDevices(
    constraints,
    function (devices) {
      prompt(contentWindow, aSubject.windowID, aSubject.callID,
             constraints, devices, secure);
    },
    function (error) {
      // bug 827146 -- In the future, the UI should catch NotFoundError
      // and allow the user to plug in a device, instead of immediately failing.
      denyGUMRequest({callID: aSubject.callID}, error);
    },
    aSubject.innerWindowID);
}

function prompt(aContentWindow, aWindowID, aCallID, aConstraints, aDevices, aSecure) {
  let audioDevices = [];
  let videoDevices = [];
  let devices = [];

  // MediaStreamConstraints defines video as 'boolean or MediaTrackConstraints'.
  let video = aConstraints.video || aConstraints.picture;
  let audio = aConstraints.audio;
  let sharingScreen = video && typeof(video) != "boolean" &&
                      video.mediaSource != "camera";
  let sharingAudio = audio && typeof(audio) != "boolean" &&
                     audio.mediaSource != "microphone";
  for (let device of aDevices) {
    device = device.QueryInterface(Ci.nsIMediaDevice);
    switch (device.type) {
      case "audio":
        // Check that if we got a microphone, we have not requested an audio
        // capture, and if we have requested an audio capture, we are not
        // getting a microphone instead.
        if (audio && (device.mediaSource == "microphone") != sharingAudio) {
          audioDevices.push({name: device.name, deviceIndex: devices.length,
                             mediaSource: device.mediaSource});
          devices.push(device);
        }
        break;
      case "video":
        // Verify that if we got a camera, we haven't requested a screen share,
        // or that if we requested a screen share we aren't getting a camera.
        if (video && (device.mediaSource == "camera") != sharingScreen) {
          videoDevices.push({name: device.name, deviceIndex: devices.length,
                             mediaSource: device.mediaSource});
          devices.push(device);
        }
        break;
    }
  }

  let requestTypes = [];
  if (videoDevices.length)
    requestTypes.push(sharingScreen ? "Screen" : "Camera");
  if (audioDevices.length)
    requestTypes.push(sharingAudio ? "AudioCapture" : "Microphone");

  if (!requestTypes.length) {
    denyGUMRequest({callID: aCallID}, "NotFoundError");
    return;
  }

  if (!aContentWindow.pendingGetUserMediaRequests) {
    setupPendingListsInitially(aContentWindow);
  }
  aContentWindow.pendingGetUserMediaRequests.set(aCallID, devices);

  let request = {
    callID: aCallID,
    windowID: aWindowID,
    documentURI: aContentWindow.document.documentURI,
    secure: aSecure,
    requestTypes: requestTypes,
    sharingScreen: sharingScreen,
    sharingAudio: sharingAudio,
    audioDevices: audioDevices,
    videoDevices: videoDevices
  };

  let mm = getMessageManagerForWindow(aContentWindow);
  mm.sendAsyncMessage("webrtc:Request", request);
}

function denyGUMRequest(aData, aError) {
  let msg = null;
  if (aError) {
    msg = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    msg.data = aError;
  }
  Services.obs.notifyObservers(msg, "getUserMedia:response:deny", aData.callID);

  if (!aData.windowID)
    return;
  let contentWindow = Services.wm.getOuterWindowWithId(aData.windowID);
  if (contentWindow.pendingGetUserMediaRequests)
    forgetGUMRequest(contentWindow, aData.callID);
}

function forgetGUMRequest(aContentWindow, aCallID) {
  aContentWindow.pendingGetUserMediaRequests.delete(aCallID);
  forgetPendingListsEventually(aContentWindow);
}

function forgetPCRequest(aContentWindow, aCallID) {
  aContentWindow.pendingPeerConnectionRequests.delete(aCallID);
  forgetPendingListsEventually(aContentWindow);
}

function setupPendingListsInitially(aContentWindow) {
  if (aContentWindow.pendingGetUserMediaRequests) {
    return;
  }
  aContentWindow.pendingGetUserMediaRequests = new Map();
  aContentWindow.pendingPeerConnectionRequests = new Set();
  aContentWindow.addEventListener("unload", ContentWebRTC);
}

function forgetPendingListsEventually(aContentWindow) {
  if (aContentWindow.pendingGetUserMediaRequests.size ||
      aContentWindow.pendingPeerConnectionRequests.size) {
    return;
  }
  aContentWindow.pendingGetUserMediaRequests = null;
  aContentWindow.pendingPeerConnectionRequests = null;
  aContentWindow.removeEventListener("unload", ContentWebRTC);
}

function updateIndicators() {
  let contentWindowSupportsArray = MediaManagerService.activeMediaCaptureWindows;
  let count = contentWindowSupportsArray.Count();

  let state = {
    showGlobalIndicator: count > 0,
    showCameraIndicator: false,
    showMicrophoneIndicator: false,
    showScreenSharingIndicator: ""
  };

  let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsIMessageSender);
  cpmm.sendAsyncMessage("webrtc:UpdatingIndicators");

  // If several iframes in the same page use media streams, it's possible to
  // have the same top level window several times. We use a Set to avoid
  // sending duplicate notifications.
  let contentWindows = new Set();
  for (let i = 0; i < count; ++i) {
    contentWindows.add(contentWindowSupportsArray.GetElementAt(i).top);
  }

  for (let contentWindow of contentWindows) {
    let camera = {}, microphone = {}, screen = {}, window = {}, app = {}, browser = {};
    MediaManagerService.mediaCaptureWindowState(contentWindow, camera, microphone,
                                                screen, window, app, browser);
    let tabState = {camera: camera.value, microphone: microphone.value};
    if (camera.value)
      state.showCameraIndicator = true;
    if (microphone.value)
      state.showMicrophoneIndicator = true;
    if (screen.value) {
      state.showScreenSharingIndicator = "Screen";
      tabState.screen = "Screen";
    }
    else if (window.value) {
      if (state.showScreenSharingIndicator != "Screen")
        state.showScreenSharingIndicator = "Window";
      tabState.screen = "Window";
    }
    else if (app.value) {
      if (!state.showScreenSharingIndicator)
        state.showScreenSharingIndicator = "Application";
      tabState.screen = "Application";
    }
    else if (browser.value) {
      if (!state.showScreenSharingIndicator)
        state.showScreenSharingIndicator = "Browser";
      tabState.screen = "Browser";
    }

    tabState.windowId = getInnerWindowIDForWindow(contentWindow);
    tabState.documentURI = contentWindow.document.documentURI;
    let mm = getMessageManagerForWindow(contentWindow);
    mm.sendAsyncMessage("webrtc:UpdateBrowserIndicators", tabState);
  }

  cpmm.sendAsyncMessage("webrtc:UpdateGlobalIndicators", state);
}

function removeBrowserSpecificIndicator(aSubject, aTopic, aData) {
  let contentWindow = Services.wm.getOuterWindowWithId(aData);
  let mm = getMessageManagerForWindow(contentWindow);
  if (mm)
    mm.sendAsyncMessage("webrtc:UpdateBrowserIndicators",
                        {windowId: getInnerWindowIDForWindow(contentWindow)});
}

function getInnerWindowIDForWindow(aContentWindow) {
  return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .currentInnerWindowID;
}

function getMessageManagerForWindow(aContentWindow) {
  let ir = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDocShell)
                         .sameTypeRootTreeItem
                         .QueryInterface(Ci.nsIInterfaceRequestor);
  try {
    // If e10s is disabled, this throws NS_NOINTERFACE for closed tabs.
    return ir.getInterface(Ci.nsIContentFrameMessageManager);
  } catch(e if e.result == Cr.NS_NOINTERFACE) {
    return null;
  }
}

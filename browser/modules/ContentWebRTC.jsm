/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "ContentWebRTC" ];

ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

const kBrowserURL = "chrome://browser/content/browser.xul";

var ContentWebRTC = {
  // Called only for 'unload' to remove pending gUM prompts in reloaded frames.
  handleEvent(aEvent) {
    let contentWindow = aEvent.target.defaultView;
    let mm = getMessageManagerForWindow(contentWindow);
    for (let key of contentWindow.pendingGetUserMediaRequests.keys()) {
      mm.sendAsyncMessage("webrtc:CancelRequest", key);
    }
    for (let key of contentWindow.pendingPeerConnectionRequests.keys()) {
      mm.sendAsyncMessage("rtcpeer:CancelRequest", key);
    }
  },

  // This observer is registered in ContentObservers.js to avoid
  // loading this .jsm when WebRTC is not in use.
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "getUserMedia:request":
        handleGUMRequest(aSubject, aTopic, aData);
        break;
      case "recording-device-stopped":
        handleGUMStop(aSubject, aTopic, aData);
        break;
      case "PeerConnection:request":
        handlePCRequest(aSubject, aTopic, aData);
        break;
      case "recording-device-events":
        updateIndicators(aSubject, aTopic, aData);
        break;
      case "recording-window-ended":
        removeBrowserSpecificIndicator(aSubject, aTopic, aData);
        break;
    }
  },

  receiveMessage(aMessage) {
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

        let allowedDevices = Cc["@mozilla.org/array;1"]
                               .createInstance(Ci.nsIMutableArray);
        for (let deviceIndex of aMessage.data.devices)
           allowedDevices.appendElement(devices[deviceIndex]);

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
  let { windowID, innerWindowID, callID, isSecure } = aSubject;
  let contentWindow = Services.wm.getOuterWindowWithId(windowID);

  let mm = getMessageManagerForWindow(contentWindow);
  if (!mm) {
    // Workaround for Bug 1207784. To use WebRTC, add-ons right now use
    // hiddenWindow.mozRTCPeerConnection which is only privileged on OSX. Other
    // platforms end up here without a message manager.
    // TODO: Remove once there's a better way (1215591).

    // Skip permission check in the absence of a message manager.
    Services.obs.notifyObservers(null, "PeerConnection:response:allow", callID);
    return;
  }

  if (!contentWindow.pendingPeerConnectionRequests) {
    setupPendingListsInitially(contentWindow);
  }
  contentWindow.pendingPeerConnectionRequests.add(callID);

  let request = {
    windowID,
    innerWindowID,
    callID,
    documentURI: contentWindow.document.documentURI,
    secure: isSecure,
  };
  mm.sendAsyncMessage("rtcpeer:Request", request);
}

function handleGUMStop(aSubject, aTopic, aData) {
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  let request = {
    windowID: aSubject.windowID,
    rawID: aSubject.rawID,
    mediaSource: aSubject.mediaSource,
  };

  let mm = getMessageManagerForWindow(contentWindow);
  if (mm)
    mm.sendAsyncMessage("webrtc:StopRecording", request);
}

function handleGUMRequest(aSubject, aTopic, aData) {
  let constraints = aSubject.getConstraints();
  let secure = aSubject.isSecure;
  let isHandlingUserInput = aSubject.isHandlingUserInput;
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  contentWindow.navigator.mozGetUserMediaDevices(
    constraints,
    function(devices) {
      // If the window has been closed while we were waiting for the list of
      // devices, there's nothing to do in the callback anymore.
      if (contentWindow.closed)
        return;

      prompt(contentWindow, aSubject.windowID, aSubject.callID,
             constraints, devices, secure, isHandlingUserInput);
    },
    function(error) {
      // Device enumeration is done ahead of handleGUMRequest, so we're not
      // responsible for handling the NotFoundError spec case.
      denyGUMRequest({callID: aSubject.callID});
    },
    aSubject.innerWindowID,
    aSubject.callID);
}

function prompt(aContentWindow, aWindowID, aCallID, aConstraints, aDevices, aSecure, aIsHandlingUserInput) {
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
      case "audioinput":
        // Check that if we got a microphone, we have not requested an audio
        // capture, and if we have requested an audio capture, we are not
        // getting a microphone instead.
        if (audio && (device.mediaSource == "microphone") != sharingAudio) {
          audioDevices.push({name: device.name, deviceIndex: devices.length,
                             id: device.rawId, mediaSource: device.mediaSource});
          devices.push(device);
        }
        break;
      case "videoinput":
        // Verify that if we got a camera, we haven't requested a screen share,
        // or that if we requested a screen share we aren't getting a camera.
        if (video && (device.mediaSource == "camera") != sharingScreen) {
          let deviceObject = {name: device.name, deviceIndex: devices.length,
                              id: device.rawId, mediaSource: device.mediaSource};
          if (device.scary)
            deviceObject.scary = true;
          videoDevices.push(deviceObject);
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
    // Device enumeration is done ahead of handleGUMRequest, so we're not
    // responsible for handling the NotFoundError spec case.
    denyGUMRequest({callID: aCallID});
    return;
  }

  if (!aContentWindow.pendingGetUserMediaRequests) {
    setupPendingListsInitially(aContentWindow);
  }
  aContentWindow.pendingGetUserMediaRequests.set(aCallID, devices);

  // Record third party origins for telemetry.
  let isThirdPartyOrigin =
    aContentWindow.document.location.origin != aContentWindow.top.document.location.origin;

  let request = {
    callID: aCallID,
    windowID: aWindowID,
    origin: aContentWindow.origin,
    documentURI: aContentWindow.document.documentURI,
    secure: aSecure,
    isHandlingUserInput: aIsHandlingUserInput,
    isThirdPartyOrigin,
    requestTypes,
    sharingScreen,
    sharingAudio,
    audioDevices,
    videoDevices
  };

  let mm = getMessageManagerForWindow(aContentWindow);
  mm.sendAsyncMessage("webrtc:Request", request);
}

function denyGUMRequest(aData) {
  Services.obs.notifyObservers(null, "getUserMedia:response:deny", aData.callID);

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

function updateIndicators(aSubject, aTopic, aData) {
  if (aSubject instanceof Ci.nsIPropertyBag &&
      aSubject.getProperty("requestURL") == kBrowserURL) {
    // Ignore notifications caused by the browser UI showing previews.
    return;
  }

  let contentWindowArray = MediaManagerService.activeMediaCaptureWindows;
  let count = contentWindowArray.length;

  let state = {
    showGlobalIndicator: count > 0,
    showCameraIndicator: false,
    showMicrophoneIndicator: false,
    showScreenSharingIndicator: ""
  };

  Services.cpmm.sendAsyncMessage("webrtc:UpdatingIndicators");

  // If several iframes in the same page use media streams, it's possible to
  // have the same top level window several times. We use a Set to avoid
  // sending duplicate notifications.
  let contentWindows = new Set();
  for (let i = 0; i < count; ++i) {
    contentWindows.add(contentWindowArray.queryElementAt(i, Ci.nsISupports).top);
  }

  for (let contentWindow of contentWindows) {
    if (contentWindow.document.documentURI == kBrowserURL) {
      // There may be a preview shown at the same time as other streams.
      continue;
    }

    let tabState = getTabStateForContentWindow(contentWindow);
    if (tabState.camera == MediaManagerService.STATE_CAPTURE_ENABLED ||
        tabState.camera == MediaManagerService.STATE_CAPTURE_DISABLED) {
      state.showCameraIndicator = true;
    }
    if (tabState.microphone == MediaManagerService.STATE_CAPTURE_ENABLED ||
        tabState.microphone == MediaManagerService.STATE_CAPTURE_DISABLED) {
      state.showMicrophoneIndicator = true;
    }
    if (tabState.screen) {
      if (tabState.screen.startsWith("Screen")) {
        state.showScreenSharingIndicator = "Screen";
      } else if (tabState.screen.startsWith("Window")) {
        if (state.showScreenSharingIndicator != "Screen")
          state.showScreenSharingIndicator = "Window";
      } else if (tabState.screen.startsWith("Application")) {
        if (!state.showScreenSharingIndicator)
          state.showScreenSharingIndicator = "Application";
      } else if (tabState.screen.startsWith("Browser")) {
        if (!state.showScreenSharingIndicator)
          state.showScreenSharingIndicator = "Browser";
      }
    }

    let mm = getMessageManagerForWindow(contentWindow);
    mm.sendAsyncMessage("webrtc:UpdateBrowserIndicators", tabState);
  }

  Services.cpmm.sendAsyncMessage("webrtc:UpdateGlobalIndicators", state);
}

function removeBrowserSpecificIndicator(aSubject, aTopic, aData) {
  let contentWindow = Services.wm.getOuterWindowWithId(aData).top;
  if (contentWindow.document.documentURI == kBrowserURL) {
    // Ignore notifications caused by the browser UI showing previews.
    return;
  }

  let tabState = getTabStateForContentWindow(contentWindow);
  if (tabState.camera == MediaManagerService.STATE_NOCAPTURE &&
      tabState.microphone == MediaManagerService.STATE_NOCAPTURE &&
      !tabState.screen)
    tabState = {windowId: tabState.windowId};

  let mm = getMessageManagerForWindow(contentWindow);
  if (mm)
    mm.sendAsyncMessage("webrtc:UpdateBrowserIndicators", tabState);
}

function getTabStateForContentWindow(aContentWindow) {
  let camera = {}, microphone = {}, screen = {}, window = {}, app = {}, browser = {};
  MediaManagerService.mediaCaptureWindowState(aContentWindow,
                                              camera, microphone,
                                              screen, window, app, browser);
  let tabState = {camera: camera.value, microphone: microphone.value};
  if (screen.value == MediaManagerService.STATE_CAPTURE_ENABLED)
    tabState.screen = "Screen";
  else if (window.value == MediaManagerService.STATE_CAPTURE_ENABLED)
    tabState.screen = "Window";
  else if (app.value == MediaManagerService.STATE_CAPTURE_ENABLED)
    tabState.screen = "Application";
  else if (browser.value == MediaManagerService.STATE_CAPTURE_ENABLED)
    tabState.screen = "Browser";
  else if (screen.value == MediaManagerService.STATE_CAPTURE_DISABLED)
    tabState.screen = "ScreenPaused";
  else if (window.value == MediaManagerService.STATE_CAPTURE_DISABLED)
    tabState.screen = "WindowPaused";
  else if (app.value == MediaManagerService.STATE_CAPTURE_DISABLED)
    tabState.screen = "ApplicationPaused";
  else if (browser.value == MediaManagerService.STATE_CAPTURE_DISABLED)
    tabState.screen = "BrowserPaused";

  let screenEnabled = tabState.screen && !tabState.screen.includes("Paused");
  let cameraEnabled = tabState.camera == MediaManagerService.STATE_CAPTURE_ENABLED;
  let microphoneEnabled = tabState.microphone == MediaManagerService.STATE_CAPTURE_ENABLED;

  // tabState.sharing controls which global indicator should be shown
  // for the tab. It should always be set to the _enabled_ device which
  // we consider most intrusive (screen > camera > microphone).
  if (screenEnabled) {
    tabState.sharing = "screen";
  } else if (cameraEnabled) {
    tabState.sharing = "camera";
  } else if (microphoneEnabled) {
    tabState.sharing = "microphone";
  } else if (tabState.screen) {
    tabState.sharing = "screen";
  } else if (tabState.camera) {
    tabState.sharing = "camera";
  } else if (tabState.microphone) {
    tabState.sharing = "microphone";
  }

  // The stream is considered paused when we're sharing something
  // but all devices are off or set to disabled.
  tabState.paused = tabState.sharing &&
    !screenEnabled && !cameraEnabled && !microphoneEnabled;

  tabState.windowId = getInnerWindowIDForWindow(aContentWindow);
  tabState.documentURI = aContentWindow.document.documentURI;

  return tabState;
}

function getInnerWindowIDForWindow(aContentWindow) {
  return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIDOMWindowUtils)
                       .currentInnerWindowID;
}

function getMessageManagerForWindow(aContentWindow) {
  aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor);

  let docShell;
  try {
    // This throws NS_NOINTERFACE for closed tabs.
    docShell = aContentWindow.getInterface(Ci.nsIDocShell);
  } catch (e) {
    if (e.result == Cr.NS_NOINTERFACE) {
      return null;
    }
    throw e;
  }

  let ir = docShell.sameTypeRootTreeItem
                   .QueryInterface(Ci.nsIInterfaceRequestor);
  try {
    // This throws NS_NOINTERFACE for closed tabs (only with e10s enabled).
    return ir.getInterface(Ci.nsIContentFrameMessageManager);
  } catch (e) {
    if (e.result == Cr.NS_NOINTERFACE) {
      return null;
    }
    throw e;
  }
}

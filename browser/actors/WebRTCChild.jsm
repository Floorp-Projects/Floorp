/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebRTCChild"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "MediaManagerService",
  "@mozilla.org/mediaManagerService;1",
  "nsIMediaManagerService"
);

const kBrowserURL = AppConstants.BROWSER_CHROME_URL;

class WebRTCChild extends JSWindowActorChild {
  // Called only for 'unload' to remove pending gUM prompts in reloaded frames.
  static handleEvent(aEvent) {
    let contentWindow = aEvent.target.defaultView;
    let actor = getActorForWindow(contentWindow);
    if (actor) {
      for (let key of contentWindow.pendingGetUserMediaRequests.keys()) {
        actor.sendAsyncMessage("webrtc:CancelRequest", key);
      }
      for (let key of contentWindow.pendingPeerConnectionRequests.keys()) {
        actor.sendAsyncMessage("rtcpeer:CancelRequest", key);
      }
    }
  }

  // This observer is registered in ContentObservers.js to avoid
  // loading this .jsm when WebRTC is not in use.
  static observe(aSubject, aTopic, aData) {
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
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "rtcpeer:Allow":
      case "rtcpeer:Deny": {
        let callID = aMessage.data.callID;
        let contentWindow = Services.wm.getOuterWindowWithId(
          aMessage.data.windowID
        );
        forgetPCRequest(contentWindow, callID);
        let topic =
          aMessage.name == "rtcpeer:Allow"
            ? "PeerConnection:response:allow"
            : "PeerConnection:response:deny";
        Services.obs.notifyObservers(null, topic, callID);
        break;
      }
      case "webrtc:Allow": {
        let callID = aMessage.data.callID;
        let contentWindow = Services.wm.getOuterWindowWithId(
          aMessage.data.windowID
        );
        let devices = contentWindow.pendingGetUserMediaRequests.get(callID);
        forgetGUMRequest(contentWindow, callID);

        let allowedDevices = Cc["@mozilla.org/array;1"].createInstance(
          Ci.nsIMutableArray
        );
        for (let deviceIndex of aMessage.data.devices) {
          allowedDevices.appendElement(devices[deviceIndex]);
        }

        Services.obs.notifyObservers(
          allowedDevices,
          "getUserMedia:response:allow",
          callID
        );
        break;
      }
      case "webrtc:Deny":
        denyGUMRequest(aMessage.data);
        break;
      case "webrtc:StopSharing":
        Services.obs.notifyObservers(
          null,
          "getUserMedia:revoke",
          aMessage.data
        );
        break;
    }
  }
}

function getActorForWindow(window) {
  let windowGlobal = window.windowGlobalChild;
  try {
    if (windowGlobal) {
      return windowGlobal.getActor("WebRTC");
    }
  } catch (ex) {
    // There might not be an actor for a parent process chrome URL.
  }

  return null;
}

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

  let actor = getActorForWindow(contentWindow);
  if (actor) {
    actor.sendAsyncMessage("rtcpeer:Request", request);
  }
}

function handleGUMStop(aSubject, aTopic, aData) {
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  let request = {
    windowID: aSubject.windowID,
    rawID: aSubject.rawID,
    mediaSource: aSubject.mediaSource,
  };

  let actor = getActorForWindow(contentWindow);
  if (actor) {
    actor.sendAsyncMessage("webrtc:StopRecording", request);
  }
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
      if (contentWindow.closed) {
        return;
      }

      prompt(
        contentWindow,
        aSubject.windowID,
        aSubject.callID,
        constraints,
        devices,
        secure,
        isHandlingUserInput
      );
    },
    function(error) {
      // Device enumeration is done ahead of handleGUMRequest, so we're not
      // responsible for handling the NotFoundError spec case.
      denyGUMRequest({ callID: aSubject.callID });
    },
    aSubject.innerWindowID,
    aSubject.callID
  );
}

function prompt(
  aContentWindow,
  aWindowID,
  aCallID,
  aConstraints,
  aDevices,
  aSecure,
  aIsHandlingUserInput
) {
  let audioDevices = [];
  let videoDevices = [];
  let devices = [];

  // MediaStreamConstraints defines video as 'boolean or MediaTrackConstraints'.
  let video = aConstraints.video || aConstraints.picture;
  let audio = aConstraints.audio;
  let sharingScreen =
    video && typeof video != "boolean" && video.mediaSource != "camera";
  let sharingAudio =
    audio && typeof audio != "boolean" && audio.mediaSource != "microphone";
  for (let device of aDevices) {
    device = device.QueryInterface(Ci.nsIMediaDevice);
    switch (device.type) {
      case "audioinput":
        // Check that if we got a microphone, we have not requested an audio
        // capture, and if we have requested an audio capture, we are not
        // getting a microphone instead.
        if (audio && (device.mediaSource == "microphone") != sharingAudio) {
          audioDevices.push({
            name: device.rawName, // unfiltered device name to show to the user
            deviceIndex: devices.length,
            id: device.rawId,
            mediaSource: device.mediaSource,
          });
          devices.push(device);
        }
        break;
      case "videoinput":
        // Verify that if we got a camera, we haven't requested a screen share,
        // or that if we requested a screen share we aren't getting a camera.
        if (video && (device.mediaSource == "camera") != sharingScreen) {
          let deviceObject = {
            name: device.rawName, // unfiltered device name to show to the user
            deviceIndex: devices.length,
            id: device.rawId,
            mediaSource: device.mediaSource,
          };
          if (device.scary) {
            deviceObject.scary = true;
          }
          videoDevices.push(deviceObject);
          devices.push(device);
        }
        break;
    }
  }

  let requestTypes = [];
  if (videoDevices.length) {
    requestTypes.push(sharingScreen ? "Screen" : "Camera");
  }
  if (audioDevices.length) {
    requestTypes.push(sharingAudio ? "AudioCapture" : "Microphone");
  }

  if (!requestTypes.length) {
    // Device enumeration is done ahead of handleGUMRequest, so we're not
    // responsible for handling the NotFoundError spec case.
    denyGUMRequest({ callID: aCallID });
    return;
  }

  if (!aContentWindow.pendingGetUserMediaRequests) {
    setupPendingListsInitially(aContentWindow);
  }
  aContentWindow.pendingGetUserMediaRequests.set(aCallID, devices);

  // WebRTC prompts have a bunch of special requirements, such as being able to
  // grant two permissions (microphone and camera), selecting devices and showing
  // a screen sharing preview. All this could have probably been baked into
  // nsIContentPermissionRequest prompts, but the team that implemented this back
  // then chose to just build their own prompting mechanism instead.
  //
  // So, what you are looking at here is not a real nsIContentPermissionRequest, but
  // something that looks really similar and will be transmitted to webrtcUI.jsm
  // for showing the prompt.
  // Note that we basically do the permission delegate check in
  // nsIContentPermissionRequest, but because webrtc uses their own prompting
  // system, we should manually apply the delegate policy here. Permission
  // should be delegated using Feature Policy and top principal
  const permDelegateHandler = aContentWindow.document.permDelegateHandler.QueryInterface(
    Ci.nsIPermissionDelegateHandler
  );

  const shouldDelegatePermission =
    permDelegateHandler.permissionDelegateFPEnabled;

  let secondOrigin = undefined;
  if (
    shouldDelegatePermission &&
    permDelegateHandler.maybeUnsafePermissionDelegate(requestTypes)
  ) {
    // We are going to prompt both first party and third party origin.
    // SecondOrigin should be third party
    secondOrigin = aContentWindow.document.nodePrincipal.origin;
  }

  let request = {
    callID: aCallID,
    windowID: aWindowID,
    secondOrigin,
    documentURI: aContentWindow.document.documentURI,
    secure: aSecure,
    isHandlingUserInput: aIsHandlingUserInput,
    shouldDelegatePermission,
    requestTypes,
    sharingScreen,
    sharingAudio,
    audioDevices,
    videoDevices,
  };

  let actor = getActorForWindow(aContentWindow);
  if (actor) {
    actor.sendAsyncMessage("webrtc:Request", request);
  }
}

function denyGUMRequest(aData) {
  let subject;
  if (aData.noOSPermission) {
    subject = "getUserMedia:response:noOSPermission";
  } else {
    subject = "getUserMedia:response:deny";
  }
  Services.obs.notifyObservers(null, subject, aData.callID);

  if (!aData.windowID) {
    return;
  }
  let contentWindow = Services.wm.getOuterWindowWithId(aData.windowID);
  if (contentWindow.pendingGetUserMediaRequests) {
    forgetGUMRequest(contentWindow, aData.callID);
  }
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
  aContentWindow.addEventListener("unload", WebRTCChild.handleEvent);
}

function forgetPendingListsEventually(aContentWindow) {
  if (
    aContentWindow.pendingGetUserMediaRequests.size ||
    aContentWindow.pendingPeerConnectionRequests.size
  ) {
    return;
  }
  aContentWindow.pendingGetUserMediaRequests = null;
  aContentWindow.pendingPeerConnectionRequests = null;
  aContentWindow.removeEventListener("unload", WebRTCChild.handleEvent);
}

function updateIndicators(aSubject, aTopic, aData) {
  if (
    aSubject instanceof Ci.nsIPropertyBag &&
    aSubject.getProperty("requestURL") == kBrowserURL
  ) {
    // Ignore notifications caused by the browser UI showing previews.
    return;
  }

  let contentWindow = aSubject.getProperty("window");

  let actor = contentWindow ? getActorForWindow(contentWindow) : null;
  if (actor) {
    let tabState = getTabStateForContentWindow(contentWindow, false);
    tabState.windowId = getInnerWindowIDForWindow(contentWindow);

    actor.sendAsyncMessage("webrtc:UpdateIndicators", tabState);
  }
}

function removeBrowserSpecificIndicator(aSubject, aTopic, aData) {
  let contentWindow = Services.wm.getOuterWindowWithId(aData);
  if (contentWindow.document.documentURI == kBrowserURL) {
    // Ignore notifications caused by the browser UI showing previews.
    return;
  }

  let tabState = getTabStateForContentWindow(contentWindow, true);

  tabState.windowId = aData;

  let actor = getActorForWindow(contentWindow);
  if (actor) {
    actor.sendAsyncMessage("webrtc:UpdateIndicators", tabState);
  }
}

function getTabStateForContentWindow(aContentWindow, aForRemove = false) {
  let camera = {},
    microphone = {},
    screen = {},
    window = {},
    browser = {};
  MediaManagerService.mediaCaptureWindowState(
    aContentWindow,
    camera,
    microphone,
    screen,
    window,
    browser,
    false
  );

  if (
    camera.value == MediaManagerService.STATE_NOCAPTURE &&
    microphone.value == MediaManagerService.STATE_NOCAPTURE &&
    screen.value == MediaManagerService.STATE_NOCAPTURE &&
    window.value == MediaManagerService.STATE_NOCAPTURE &&
    browser.value == MediaManagerService.STATE_NOCAPTURE
  ) {
    return {};
  }

  if (aForRemove) {
    return {};
  }

  return {
    camera: camera.value,
    microphone: microphone.value,
    screen: screen.value,
    window: window.value,
    browser: browser.value,
    documentURI: aContentWindow.document.documentURI,
  };
}

function getInnerWindowIDForWindow(aContentWindow) {
  return aContentWindow.windowUtils.currentInnerWindowID;
}

function getMessageManagerForWindow(aContentWindow) {
  let docShell = aContentWindow.docShell;
  if (!docShell) {
    // Closed tab.
    return null;
  }

  return docShell.messageManager;
}

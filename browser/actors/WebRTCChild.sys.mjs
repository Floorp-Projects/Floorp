/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "MediaManagerService",
  "@mozilla.org/mediaManagerService;1",
  "nsIMediaManagerService"
);

const kBrowserURL = AppConstants.BROWSER_CHROME_URL;

/**
 * GlobalMuteListener is a process-global object that listens for changes to
 * the global mute state of the camera and microphone. When it notices a
 * change in that state, it tells the underlying platform code to mute or
 * unmute those devices.
 */
const GlobalMuteListener = {
  _initted: false,

  /**
   * Initializes the listener if it hasn't been already. This will also
   * ensure that the microphone and camera are initially in the right
   * muting state.
   */
  init() {
    if (!this._initted) {
      Services.cpmm.sharedData.addEventListener("change", this);
      this._updateCameraMuteState();
      this._updateMicrophoneMuteState();
      this._initted = true;
    }
  },

  handleEvent(event) {
    if (event.changedKeys.includes("WebRTC:GlobalCameraMute")) {
      this._updateCameraMuteState();
    }
    if (event.changedKeys.includes("WebRTC:GlobalMicrophoneMute")) {
      this._updateMicrophoneMuteState();
    }
  },

  _updateCameraMuteState() {
    let shouldMute = Services.cpmm.sharedData.get("WebRTC:GlobalCameraMute");
    let topic = shouldMute
      ? "getUserMedia:muteVideo"
      : "getUserMedia:unmuteVideo";
    Services.obs.notifyObservers(null, topic);
  },

  _updateMicrophoneMuteState() {
    let shouldMute = Services.cpmm.sharedData.get(
      "WebRTC:GlobalMicrophoneMute"
    );
    let topic = shouldMute
      ? "getUserMedia:muteAudio"
      : "getUserMedia:unmuteAudio";

    Services.obs.notifyObservers(null, topic);
  },
};

export class WebRTCChild extends JSWindowActorChild {
  actorCreated() {
    // The user might request that DOM notifications be silenced
    // when sharing the screen. There doesn't seem to be a great
    // way of storing that state in any of the objects going into
    // the WebRTC API or coming out via the observer notification
    // service, so we store it here on the actor.
    //
    // If the user chooses to silence notifications during screen
    // share, this will get set to true.
    this.suppressNotifications = false;
  }

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

  // This observer is called from BrowserProcessChild to avoid
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

        this.suppressNotifications = !!aMessage.data.suppressNotifications;

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
      case "webrtc:MuteCamera":
        Services.obs.notifyObservers(
          null,
          "getUserMedia:muteVideo",
          aMessage.data
        );
        break;
      case "webrtc:UnmuteCamera":
        Services.obs.notifyObservers(
          null,
          "getUserMedia:unmuteVideo",
          aMessage.data
        );
        break;
      case "webrtc:MuteMicrophone":
        Services.obs.notifyObservers(
          null,
          "getUserMedia:muteAudio",
          aMessage.data
        );
        break;
      case "webrtc:UnmuteMicrophone":
        Services.obs.notifyObservers(
          null,
          "getUserMedia:unmuteAudio",
          aMessage.data
        );
        break;
    }
  }
}

function getActorForWindow(window) {
  try {
    let windowGlobal = window.windowGlobalChild;
    if (windowGlobal) {
      return windowGlobal.getActor("WebRTC");
    }
  } catch (ex) {
    // There might not be an actor for a parent process chrome URL,
    // and we may not even be allowed to access its windowGlobalChild.
  }

  return null;
}

function handlePCRequest(aSubject, aTopic, aData) {
  let { windowID, innerWindowID, callID, isSecure } = aSubject;
  let contentWindow = Services.wm.getOuterWindowWithId(windowID);
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
  // Now that a getUserMedia request has been created, we should check
  // to see if we're supposed to have any devices muted. This needs
  // to occur after the getUserMedia request is made, since the global
  // mute state is associated with the GetUserMediaWindowListener, which
  // is only created after a getUserMedia request.
  GlobalMuteListener.init();

  let constraints = aSubject.getConstraints();
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  prompt(
    aSubject.type,
    contentWindow,
    aSubject.windowID,
    aSubject.callID,
    constraints,
    aSubject.getAudioOutputOptions(),
    aSubject.devices,
    aSubject.isSecure,
    aSubject.isHandlingUserInput
  );
}

function prompt(
  aRequestType,
  aContentWindow,
  aWindowID,
  aCallID,
  aConstraints,
  aAudioOutputOptions,
  aDevices,
  aSecure,
  aIsHandlingUserInput
) {
  let audioInputDevices = [];
  let videoInputDevices = [];
  let audioOutputDevices = [];
  let devices = [];

  // MediaStreamConstraints defines video as 'boolean or MediaTrackConstraints'.
  let video = aConstraints.video || aConstraints.picture;
  let audio = aConstraints.audio;
  let sharingScreen =
    video && typeof video != "boolean" && video.mediaSource != "camera";
  let sharingAudio =
    audio && typeof audio != "boolean" && audio.mediaSource != "microphone";

  const hasInherentConstraints = ({ facingMode, groupId, deviceId }) => {
    const id = [deviceId].flat()[0];
    return facingMode || groupId || (id && id != "default"); // flock workaround
  };
  let hasInherentAudioConstraints =
    audio &&
    !sharingAudio &&
    [audio, ...(audio.advanced || [])].some(hasInherentConstraints);
  let hasInherentVideoConstraints =
    video &&
    !sharingScreen &&
    [video, ...(video.advanced || [])].some(hasInherentConstraints);

  for (let device of aDevices) {
    device = device.QueryInterface(Ci.nsIMediaDevice);
    let deviceObject = {
      name: device.rawName, // unfiltered device name to show to the user
      deviceIndex: devices.length,
      rawId: device.rawId,
      id: device.id,
      mediaSource: device.mediaSource,
      canRequestOsLevelPrompt: device.canRequestOsLevelPrompt,
    };
    switch (device.type) {
      case "audioinput":
        // Check that if we got a microphone, we have not requested an audio
        // capture, and if we have requested an audio capture, we are not
        // getting a microphone instead.
        if (audio && (device.mediaSource == "microphone") != sharingAudio) {
          audioInputDevices.push(deviceObject);
          devices.push(device);
        }
        break;
      case "videoinput":
        // Verify that if we got a camera, we haven't requested a screen share,
        // or that if we requested a screen share we aren't getting a camera.
        if (video && (device.mediaSource == "camera") != sharingScreen) {
          if (device.scary) {
            deviceObject.scary = true;
          }
          videoInputDevices.push(deviceObject);
          devices.push(device);
        }
        break;
      case "audiooutput":
        if (aRequestType == "selectaudiooutput") {
          audioOutputDevices.push(deviceObject);
          devices.push(device);
        }
        break;
    }
  }

  let requestTypes = [];
  if (videoInputDevices.length) {
    requestTypes.push(sharingScreen ? "Screen" : "Camera");
  }
  if (audioInputDevices.length) {
    requestTypes.push(sharingAudio ? "AudioCapture" : "Microphone");
  }
  if (audioOutputDevices.length) {
    requestTypes.push("Speaker");
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
  // something that looks really similar and will be transmitted to webrtcUI.sys.mjs
  // for showing the prompt.
  // Note that we basically do the permission delegate check in
  // nsIContentPermissionRequest, but because webrtc uses their own prompting
  // system, we should manually apply the delegate policy here. Permission
  // should be delegated using Feature Policy and top principal
  const permDelegateHandler =
    aContentWindow.document.permDelegateHandler.QueryInterface(
      Ci.nsIPermissionDelegateHandler
    );

  let secondOrigin = undefined;
  if (permDelegateHandler.maybeUnsafePermissionDelegate(requestTypes)) {
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
    requestTypes,
    sharingScreen,
    sharingAudio,
    audioInputDevices,
    videoInputDevices,
    audioOutputDevices,
    hasInherentAudioConstraints,
    hasInherentVideoConstraints,
    audioOutputId: aAudioOutputOptions.deviceId,
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

    // If we were silencing DOM notifications before, but we've updated
    // state such that we're no longer sharing one of our displays, then
    // reset the silencing state.
    if (actor.suppressNotifications) {
      if (!tabState.screen && !tabState.window && !tabState.browser) {
        actor.suppressNotifications = false;
      }
    }

    tabState.suppressNotifications = actor.suppressNotifications;

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
    browser = {},
    devices = {};
  lazy.MediaManagerService.mediaCaptureWindowState(
    aContentWindow,
    camera,
    microphone,
    screen,
    window,
    browser,
    devices
  );

  if (
    camera.value == lazy.MediaManagerService.STATE_NOCAPTURE &&
    microphone.value == lazy.MediaManagerService.STATE_NOCAPTURE &&
    screen.value == lazy.MediaManagerService.STATE_NOCAPTURE &&
    window.value == lazy.MediaManagerService.STATE_NOCAPTURE &&
    browser.value == lazy.MediaManagerService.STATE_NOCAPTURE
  ) {
    return { remove: true };
  }

  if (aForRemove) {
    return { remove: true };
  }

  let serializedDevices = [];
  if (Array.isArray(devices.value)) {
    serializedDevices = devices.value.map(device => {
      return {
        type: device.type,
        mediaSource: device.mediaSource,
        rawId: device.rawId,
        scary: device.scary,
      };
    });
  }

  return {
    camera: camera.value,
    microphone: microphone.value,
    screen: screen.value,
    window: window.value,
    browser: browser.value,
    devices: serializedDevices,
  };
}

function getInnerWindowIDForWindow(aContentWindow) {
  return aContentWindow.windowGlobalChild.innerWindowId;
}

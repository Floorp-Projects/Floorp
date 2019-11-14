/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from fxrui.js */

/**
 * Code to manage Permissions UI
 *
 * FxR on Desktop only supports granting permission for
 * - Location
 * - Camera
 * - Microphone
 * Any other permissions are automatically denied.
 *
 */

// Base class for managing permissions in FxR on PC
class FxrPermissionPromptPrototype {
  constructor(aRequest, aBrowser, aCallback) {
    this.request = aRequest;
    this.targetBrowser = aBrowser;
    this.responseCallback = aCallback;
  }

  showPrompt() {
    // For now, all permissions default to denied. Testing for allow must be
    // done manually until UI is finished:
    // Bug 1594840 - Add UI for Web Permissions in FxR for Desktop
    this.defaultDeny();
  }

  defaultDeny() {
    this.handleResponse(false);
  }

  handleResponse(allowed) {
    if (allowed) {
      this.allow();
    } else {
      this.deny();
    }

    this.responseCallback();
  }
}

// WebRTC-specific class implementation
class FxrWebRTCPrompt extends FxrPermissionPromptPrototype {
  showPrompt() {
    for (let typeName of this.request.requestTypes) {
      if (typeName !== "Microphone" && typeName !== "Camera") {
        // Only Microphone and Camera requests are allowed. Automatically deny
        // any other request.
        this.defaultDeny();
        return;
      }
    }

    super.showPrompt();
  }

  allow() {
    let { audioDevices, videoDevices } = this.request;

    let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
      this.request.origin
    );

    // For now, collect the first audio and video device by default. User
    // selection will be enabled later:
    // Bug 1594841 - Add UI to select device for WebRTC in FxR for Desktop
    let allowedDevices = [];

    if (audioDevices.length) {
      allowedDevices.push(audioDevices[0].deviceIndex);
    }

    if (videoDevices.length) {
      Services.perms.addFromPrincipal(
        principal,
        "MediaManagerVideo",
        Services.perms.ALLOW_ACTION,
        Services.perms.EXPIRE_SESSION
      );
      allowedDevices.push(videoDevices[0].deviceIndex);
    }

    this.targetBrowser.messageManager.sendAsyncMessage("webrtc:Allow", {
      callID: this.request.callID,
      windowID: this.request.windowID,
      devices: allowedDevices,
    });
  }

  deny() {
    this.targetBrowser.messageManager.sendAsyncMessage("webrtc:Deny", {
      callID: this.request.callID,
      windowID: this.request.windowID,
    });
  }
}

// Implementation for other, non-WebRTC web permission prompts
class FxrContentPrompt extends FxrPermissionPromptPrototype {
  showPrompt() {
    // Only allow exactly one permission request here.
    let types = this.request.types.QueryInterface(Ci.nsIArray);
    if (types.length != 1) {
      this.defaultDeny();
      return;
    }

    // Only Location is supported from this type of request
    let type = types.queryElementAt(0, Ci.nsIContentPermissionType).type;
    if (type !== "geolocation") {
      this.defaultDeny();
      return;
    }

    // Override type so that it can be more easily interpreted by the code
    // for the prompt.
    type = "Location";
    super.showPrompt();
  }

  allow() {
    this.request.allow();
  }

  deny() {
    this.request.cancel();
  }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["WebRTCParent"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "SitePermissions",
  "resource:///modules/SitePermissions.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "webrtcUI",
  "resource:///modules/webrtcUI.jsm"
);

XPCOMUtils.defineLazyGetter(lazy, "gBrandBundle", function() {
  return Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "OSPermissions",
  "@mozilla.org/ospermissionrequest;1",
  "nsIOSPermissionRequest"
);

// Keep in sync with defines at base_capturer_pipewire.cc
// With PipeWire we can't select which system resource is shared so
// we don't create a window/screen list. Instead we place these constants
// as window name/id so frontend code can identify PipeWire backend
// and does not try to create screen/window preview.
const PIPEWIRE_PORTAL_NAME = "####_PIPEWIRE_PORTAL_####";
const PIPEWIRE_ID = 0xaffffff;

class WebRTCParent extends JSWindowActorParent {
  didDestroy() {
    // Media stream tracks end on unload, so call stopRecording() on them early
    // *before* we go away, to ensure we're working with the right principal.
    this.stopRecording(this.manager.outerWindowId);
    lazy.webrtcUI.forgetStreamsFromBrowserContext(this.browsingContext);
    // Must clear activePerms here to prevent them from being read by laggard
    // stopRecording() calls, which due to IPC, may come in *after* navigation.
    // This is to prevent granting temporary grace periods to the wrong page.
    lazy.webrtcUI.activePerms.delete(this.manager.outerWindowId);
  }

  getBrowser() {
    return this.browsingContext.top.embedderElement;
  }

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "rtcpeer:Request": {
        let params = Object.freeze(
          Object.assign(
            {
              origin: this.manager.documentPrincipal.origin,
            },
            aMessage.data
          )
        );

        let blockers = Array.from(lazy.webrtcUI.peerConnectionBlockers);

        (async function() {
          for (let blocker of blockers) {
            try {
              let result = await blocker(params);
              if (result == "deny") {
                return false;
              }
            } catch (err) {
              Cu.reportError(`error in PeerConnection blocker: ${err.message}`);
            }
          }
          return true;
        })().then(decision => {
          let message;
          if (decision) {
            lazy.webrtcUI.emitter.emit("peer-request-allowed", params);
            message = "rtcpeer:Allow";
          } else {
            lazy.webrtcUI.emitter.emit("peer-request-blocked", params);
            message = "rtcpeer:Deny";
          }

          this.sendAsyncMessage(message, {
            callID: params.callID,
            windowID: params.windowID,
          });
        });
        break;
      }
      case "rtcpeer:CancelRequest": {
        let params = Object.freeze({
          origin: this.manager.documentPrincipal.origin,
          callID: aMessage.data,
        });
        lazy.webrtcUI.emitter.emit("peer-request-cancel", params);
        break;
      }
      case "webrtc:Request": {
        let data = aMessage.data;

        // Record third party origins for telemetry.
        let isThirdPartyOrigin =
          this.manager.documentPrincipal.origin !=
          this.manager.topWindowContext.documentPrincipal.origin;
        data.isThirdPartyOrigin = isThirdPartyOrigin;

        data.origin = data.shouldDelegatePermission
          ? this.manager.topWindowContext.documentPrincipal.origin
          : this.manager.documentPrincipal.origin;

        let browser = this.getBrowser();
        if (browser.fxrPermissionPrompt) {
          // For Firefox Reality on Desktop, switch to a different mechanism to
          // prompt the user since fewer permissions are available and since many
          // UI dependencies are not available.
          browser.fxrPermissionPrompt(data);
        } else {
          prompt(this, this.getBrowser(), data);
        }
        break;
      }
      case "webrtc:StopRecording":
        this.stopRecording(
          aMessage.data.windowID,
          aMessage.data.mediaSource,
          aMessage.data.rawID
        );
        break;
      case "webrtc:CancelRequest": {
        let browser = this.getBrowser();
        // browser can be null when closing the window
        if (browser) {
          removePrompt(browser, aMessage.data);
        }
        break;
      }
      case "webrtc:UpdateIndicators": {
        let { data } = aMessage;
        data.documentURI = this.manager.documentURI?.spec;
        if (data.windowId) {
          if (!data.remove) {
            data.principal = this.manager.topWindowContext.documentPrincipal;
          }
          lazy.webrtcUI.streamAddedOrRemoved(this.browsingContext, data);
        }
        this.updateIndicators(data);
        break;
      }
    }
  }

  updateIndicators(aData) {
    let browsingContext = this.browsingContext;
    let state = lazy.webrtcUI.updateIndicators(browsingContext.top);

    let browser = this.getBrowser();
    if (!browser) {
      return;
    }

    state.browsingContext = browsingContext;
    state.windowId = aData.windowId;

    let tabbrowser = browser.ownerGlobal.gBrowser;
    if (tabbrowser) {
      tabbrowser.updateBrowserSharing(browser, {
        webRTC: state,
      });
    }
  }

  denyRequest(aRequest) {
    this.sendAsyncMessage("webrtc:Deny", {
      callID: aRequest.callID,
      windowID: aRequest.windowID,
    });
  }

  //
  // Deny the request because the browser does not have access to the
  // camera or microphone due to OS security restrictions. The user may
  // have granted camera/microphone access to the site, but not have
  // allowed the browser access in OS settings.
  //
  denyRequestNoPermission(aRequest) {
    this.sendAsyncMessage("webrtc:Deny", {
      callID: aRequest.callID,
      windowID: aRequest.windowID,
      noOSPermission: true,
    });
  }

  //
  // Check if we have permission to access the camera or screen-sharing and/or
  // microphone at the OS level. Triggers a request to access the device if access
  // is needed and the permission state has not yet been determined.
  //
  async checkOSPermission(camNeeded, micNeeded, scrNeeded) {
    // Don't trigger OS permission requests for fake devices. Fake devices don't
    // require OS permission and the dialogs are problematic in automated testing
    // (where fake devices are used) because they require user interaction.
    if (
      !scrNeeded &&
      Services.prefs.getBoolPref("media.navigator.streams.fake", false)
    ) {
      return true;
    }
    let camStatus = {},
      micStatus = {};
    if (camNeeded || micNeeded) {
      lazy.OSPermissions.getMediaCapturePermissionState(camStatus, micStatus);
    }
    if (camNeeded) {
      let camPermission = camStatus.value;
      let camAccessible = await this.checkAndGetOSPermission(
        camPermission,
        lazy.OSPermissions.requestVideoCapturePermission
      );
      if (!camAccessible) {
        return false;
      }
    }
    if (micNeeded) {
      let micPermission = micStatus.value;
      let micAccessible = await this.checkAndGetOSPermission(
        micPermission,
        lazy.OSPermissions.requestAudioCapturePermission
      );
      if (!micAccessible) {
        return false;
      }
    }
    let scrStatus = {};
    if (scrNeeded) {
      lazy.OSPermissions.getScreenCapturePermissionState(scrStatus);
      if (scrStatus.value == lazy.OSPermissions.PERMISSION_STATE_DENIED) {
        lazy.OSPermissions.maybeRequestScreenCapturePermission();
        return false;
      }
    }
    return true;
  }

  //
  // Given a device's permission, return true if the device is accessible. If
  // the device's permission is not yet determined, request access to the device.
  // |requestPermissionFunc| must return a promise that resolves with true
  // if the device is accessible and false otherwise.
  //
  async checkAndGetOSPermission(devicePermission, requestPermissionFunc) {
    if (
      devicePermission == lazy.OSPermissions.PERMISSION_STATE_DENIED ||
      devicePermission == lazy.OSPermissions.PERMISSION_STATE_RESTRICTED
    ) {
      return false;
    }
    if (devicePermission == lazy.OSPermissions.PERMISSION_STATE_NOTDETERMINED) {
      let deviceAllowed = await requestPermissionFunc();
      if (!deviceAllowed) {
        return false;
      }
    }
    return true;
  }

  stopRecording(aOuterWindowId, aMediaSource, aRawId) {
    for (let { browsingContext, state } of lazy.webrtcUI._streams) {
      if (browsingContext == this.browsingContext) {
        let { principal } = state;
        for (let { mediaSource, rawId } of state.devices) {
          if (aRawId && (aRawId != rawId || aMediaSource != mediaSource)) {
            continue;
          }
          // Deactivate this device (no aRawId means all devices).
          this.deactivateDevicePerm(
            aOuterWindowId,
            mediaSource,
            rawId,
            principal
          );
        }
      }
    }
  }

  /**
   * Add a device record to webrtcUI.activePerms, denoting a device as in use.
   * Important to call for permission grace periods to work correctly.
   */
  activateDevicePerm(aOuterWindowId, aMediaSource, aId) {
    if (!lazy.webrtcUI.activePerms.has(this.manager.outerWindowId)) {
      lazy.webrtcUI.activePerms.set(this.manager.outerWindowId, new Map());
    }
    lazy.webrtcUI.activePerms
      .get(this.manager.outerWindowId)
      .set(aOuterWindowId + aMediaSource + aId, aMediaSource);
  }

  /**
   * Remove a device record from webrtcUI.activePerms, denoting a device as
   * no longer in use by the site. Meaning: gUM requests for this device will
   * no longer be implicitly granted through the webrtcUI.activePerms mechanism.
   *
   * However, if webrtcUI.deviceGracePeriodTimeoutMs is defined, the implicit
   * grant is extended for an additional period of time through SitePermissions.
   */
  deactivateDevicePerm(
    aOuterWindowId,
    aMediaSource,
    aId,
    aPermissionPrincipal
  ) {
    // If we don't have active permissions for the given window anymore don't
    // set a grace period. This happens if there has been a user revoke and
    // webrtcUI clears the permissions.
    if (!lazy.webrtcUI.activePerms.has(this.manager.outerWindowId)) {
      return;
    }
    let map = lazy.webrtcUI.activePerms.get(this.manager.outerWindowId);
    map.delete(aOuterWindowId + aMediaSource + aId);

    // Add a permission grace period for camera and microphone only
    if (
      (aMediaSource != "camera" && aMediaSource != "microphone") ||
      !this.browsingContext.top.embedderElement
    ) {
      return;
    }
    let gracePeriodMs = lazy.webrtcUI.deviceGracePeriodTimeoutMs;
    if (gracePeriodMs > 0) {
      // A grace period is extended (even past navigation) to this outer window
      // + origin + deviceId only. This avoids re-prompting without the user
      // having to persist permission to the site, in a common case of a web
      // conference asking them for the camera in a lobby page, before
      // navigating to the actual meeting room page. Does not survive tab close.
      //
      // Caution: since navigation causes deactivation, we may be in the middle
      // of one. We must pass in a principal & URI for SitePermissions to use
      // instead of browser.currentURI, because the latter may point to a new
      // page already, and we must not leak permission to unrelated pages.
      //
      let permissionName = [aMediaSource, aId].join("^");
      lazy.SitePermissions.setForPrincipal(
        aPermissionPrincipal,
        permissionName,
        lazy.SitePermissions.ALLOW,
        lazy.SitePermissions.SCOPE_TEMPORARY,
        this.browsingContext.top.embedderElement,
        gracePeriodMs,
        aPermissionPrincipal.URI
      );
    }
  }

  /**
   * Checks if the principal has sufficient permissions
   * to fulfill the given request. If the request can be
   * fulfilled, a message is sent to the child
   * signaling that WebRTC permissions were given and
   * this function will return true.
   */
  checkRequestAllowed(aRequest, aPrincipal) {
    if (!aRequest.secure) {
      return false;
    }
    // Always prompt for screen sharing
    if (aRequest.sharingScreen) {
      return false;
    }
    if (aRequest.audioOutputDevices?.length) {
      return false;
    }

    let { perms } = Services;
    if (
      perms.testExactPermissionFromPrincipal(aPrincipal, "MediaManagerVideo")
    ) {
      perms.removeFromPrincipal(aPrincipal, "MediaManagerVideo");
    }

    // Don't use persistent permissions from the top-level principal
    // if we're in a cross-origin iframe and permission delegation is not
    // allowed, or when we're handling a potentially insecure third party
    // through a wildcard ("*") allow attribute.
    let limited =
      (aRequest.isThirdPartyOrigin && !aRequest.shouldDelegatePermission) ||
      aRequest.secondOrigin;

    let map = lazy.webrtcUI.activePerms.get(this.manager.outerWindowId);
    let {
      callID,
      windowID,
      audioInputDevices,
      videoInputDevices,
      hasInherentAudioConstraints,
      hasInherentVideoConstraints,
    } = aRequest;

    // We consider a camera or mic active if it is active or was active within a
    // grace period of milliseconds ago.
    const isAllowed = ({ mediaSource, id }, permissionID) =>
      map?.get(windowID + mediaSource + id) ||
      (!limited &&
        (lazy.SitePermissions.getForPrincipal(aPrincipal, permissionID).state ==
          lazy.SitePermissions.ALLOW ||
          lazy.SitePermissions.getForPrincipal(
            aPrincipal,
            [mediaSource, id].join("^"),
            this.getBrowser()
          ).state == lazy.SitePermissions.ALLOW));

    let microphone;
    if (audioInputDevices.length) {
      for (let device of audioInputDevices) {
        if (isAllowed(device, "microphone")) {
          microphone = device;
          break;
        }
        if (hasInherentAudioConstraints) {
          // Inherent constraints suggest site is looking for a specific mic
          break;
        }
        // Some sites don't look too hard at what they get, and spam gUM without
        // adjusting what they ask for to match what they got last time. To keep
        // users in charge and reduce prompts, ignore other constraints by
        // returning the most-fit microphone a site already has access to.
      }
      if (!microphone) {
        return false;
      }
    }
    let camera;
    if (videoInputDevices.length) {
      for (let device of videoInputDevices) {
        if (isAllowed(device, "camera")) {
          camera = device;
          break;
        }
        if (hasInherentVideoConstraints) {
          // Inherent constraints suggest site is looking for a specific camera
          break;
        }
        // Some sites don't look too hard at what they get, and spam gUM without
        // adjusting what they ask for to match what they got last time. To keep
        // users in charge and reduce prompts, ignore other constraints by
        // returning the most-fit camera a site already has access to.
      }
      if (!camera) {
        return false;
      }
    }
    let devices = [];
    if (camera) {
      perms.addFromPrincipal(
        aPrincipal,
        "MediaManagerVideo",
        perms.ALLOW_ACTION,
        perms.EXPIRE_SESSION
      );
      devices.push(camera.deviceIndex);
      this.activateDevicePerm(windowID, camera.mediaSource, camera.id);
    }
    if (microphone) {
      devices.push(microphone.deviceIndex);
      this.activateDevicePerm(windowID, microphone.mediaSource, microphone.id);
    }
    this.checkOSPermission(!!camera, !!microphone, false).then(
      havePermission => {
        if (havePermission) {
          this.sendAsyncMessage("webrtc:Allow", { callID, windowID, devices });
        } else {
          this.denyRequestNoPermission(aRequest);
        }
      }
    );
    return true;
  }
}

function prompt(aActor, aBrowser, aRequest) {
  let {
    audioInputDevices,
    videoInputDevices,
    audioOutputDevices,
    sharingScreen,
    sharingAudio,
    requestTypes,
  } = aRequest;

  let principal = Services.scriptSecurityManager.createContentPrincipalFromOrigin(
    aRequest.origin
  );

  // For add-on principals, we immediately check for permission instead
  // of waiting for the notification to focus. This allows for supporting
  // cases such as browserAction popups where no prompt is shown.
  if (principal.addonPolicy) {
    let isPopup = false;
    let isBackground = false;

    for (let view of principal.addonPolicy.extension.views) {
      if (view.viewType == "popup" && view.xulBrowser == aBrowser) {
        isPopup = true;
      }
      if (view.viewType == "background" && view.xulBrowser == aBrowser) {
        isBackground = true;
      }
    }

    // Recording from background pages is considered too sensitive and will
    // always be denied.
    if (isBackground) {
      aActor.denyRequest(aRequest);
      return;
    }

    // If the request comes from a popup, we don't want to show the prompt,
    // but we do want to allow the request if the user previously gave permission.
    if (isPopup) {
      if (!aActor.checkRequestAllowed(aRequest, principal, aBrowser)) {
        aActor.denyRequest(aRequest);
      }
      return;
    }
  }

  // If the user has already denied access once in this tab,
  // deny again without even showing the notification icon.
  for (const type of requestTypes) {
    const permissionID =
      type == "AudioCapture" ? "microphone" : type.toLowerCase();
    if (
      lazy.SitePermissions.getForPrincipal(principal, permissionID, aBrowser)
        .state == lazy.SitePermissions.BLOCK
    ) {
      aActor.denyRequest(aRequest);
      return;
    }
  }

  let chromeDoc = aBrowser.ownerDocument;
  let stringBundle = chromeDoc.defaultView.gNavigatorBundle;
  let localization = new Localization(
    ["branding/brand.ftl", "browser/browser.ftl"],
    true
  );

  // Mind the order, because for simplicity we're iterating over the list using
  // "includes()". This allows the rotation of string identifiers. We list the
  // full identifiers here so they can be cross-referenced more easily.
  let joinedRequestTypes = requestTypes.join("And");
  let requestMessages;
  if (aRequest.secondOrigin) {
    requestMessages = [
      // Individual request types first.
      "getUserMedia.shareCameraUnsafeDelegation2.message",
      "getUserMedia.shareMicrophoneUnsafeDelegations2.message",
      "getUserMedia.shareScreenUnsafeDelegation2.message",
      "getUserMedia.shareAudioCaptureUnsafeDelegation2.message",
      "selectAudioOutput.shareSpeakerUnsafeDelegation.message",
      // Combinations of the above request types last.
      "getUserMedia.shareCameraAndMicrophoneUnsafeDelegation2.message",
      "getUserMedia.shareCameraAndAudioCaptureUnsafeDelegation2.message",
      "getUserMedia.shareScreenAndMicrophoneUnsafeDelegation2.message",
      "getUserMedia.shareScreenAndAudioCaptureUnsafeDelegation2.message",
    ];
  } else {
    requestMessages = [
      // Individual request types first.
      "getUserMedia.shareCamera3.message",
      "getUserMedia.shareMicrophone3.message",
      "getUserMedia.shareScreen4.message",
      "getUserMedia.shareAudioCapture3.message",
      "selectAudioOutput.shareSpeaker.message",
      // Combinations of the above request types last.
      "getUserMedia.shareCameraAndMicrophone3.message",
      "getUserMedia.shareCameraAndAudioCapture3.message",
      "getUserMedia.shareScreenAndMicrophone4.message",
      "getUserMedia.shareScreenAndAudioCapture4.message",
    ];
  }

  let stringId = requestMessages.find(id => id.includes(joinedRequestTypes));
  let message = aRequest.secondOrigin
    ? stringBundle.getFormattedString(stringId, ["<>", "{}"])
    : stringBundle.getFormattedString(stringId, ["<>"]);

  let notification; // Used by action callbacks.
  let mainAction = {
    label: stringBundle.getString("getUserMedia.allow.label"),
    accessKey: stringBundle.getString("getUserMedia.allow.accesskey"),
    // The real callback will be set during the "showing" event. The
    // empty function here is so that PopupNotifications.show doesn't
    // reject the action.
    callback() {},
  };

  let notificationSilencingEnabled = Services.prefs.getBoolPref(
    "privacy.webrtc.allowSilencingNotifications"
  );

  let secondaryActions = [];
  if (notificationSilencingEnabled && sharingScreen) {
    // We want to free up the checkbox at the bottom of the permission
    // panel for the notification silencing option, so we use a
    // different configuration for the permissions panel when
    // notification silencing is enabled.

    // The formatMessagesSync method returns an array of results
    // for each message that was requested, and for the ones with
    // attributes, returns an attributes array with objects like:
    //
    // { name: "someName", value: "somevalue" }
    //
    // For these strings, which use .label and .accesskey attributes,
    // this convertAttributesToObjects function looks at the attributes
    // property of each message, and returns back an array of objects,
    // where each object property is one of the attribute names, and
    // the property value is the attribute value.
    //
    // So, the above example would be converted into:
    //
    // { someName: "someValue" }
    //
    // which is much easier to access and pass along to other things.
    let convertAttributesToObjects = messages => {
      return messages.map(msg => {
        return msg.attributes.reduce((acc, attribute) => {
          acc[attribute.name] = attribute.value;
          return acc;
        }, {});
      });
    };

    let [block, alwaysBlock] = convertAttributesToObjects(
      localization.formatMessagesSync([
        { id: "popup-screen-sharing-block" },
        { id: "popup-screen-sharing-always-block" },
      ])
    );

    secondaryActions = [
      {
        label: block.label,
        accessKey: block.accesskey,
        callback(aState) {
          aActor.denyRequest(aRequest);
          lazy.SitePermissions.setForPrincipal(
            principal,
            "screen",
            lazy.SitePermissions.BLOCK,
            lazy.SitePermissions.SCOPE_TEMPORARY,
            notification.browser
          );
        },
      },
      {
        label: alwaysBlock.label,
        accessKey: alwaysBlock.accesskey,
        callback(aState) {
          aActor.denyRequest(aRequest);
          lazy.SitePermissions.setForPrincipal(
            principal,
            "screen",
            lazy.SitePermissions.BLOCK,
            lazy.SitePermissions.SCOPE_PERSISTENT,
            notification.browser
          );
        },
      },
    ];
  } else {
    let label = "getUserMedia.block.label";
    let key = "getUserMedia.block.accesskey";
    let isNotNowLabelEnabled =
      allowedOrActiveCameraOrMicrophone(aBrowser) && !sharingScreen;
    // We have a (temporary) allow permission for some device
    // hence we offer a 'Not now' label instead of 'Block'.
    if (isNotNowLabelEnabled) {
      label = "getUserMedia.notNow.label";
      key = "getUserMedia.notNow.accesskey";
    }
    secondaryActions = [
      {
        label: stringBundle.getString(label),
        accessKey: stringBundle.getString(key),
        callback(aState) {
          aActor.denyRequest(aRequest);

          let scope = lazy.SitePermissions.SCOPE_TEMPORARY;
          if (aState && aState.checkboxChecked) {
            scope = lazy.SitePermissions.SCOPE_PERSISTENT;
          }

          // Choosing 'Not now' will not set a block permission
          // we just deny the request. This enables certain use cases
          // where sites want to switch devices, but users back out of the permission request
          // (See Bug 1609578).
          // Selecting 'Remember this decision' and clicking 'Not now' will set a persistent block
          if (
            scope != lazy.SitePermissions.SCOPE_PERSISTENT &&
            isNotNowLabelEnabled
          ) {
            return;
          }

          // Denying a camera / microphone prompt means we set a temporary or
          // persistent permission block. There may still be active grace period
          // permissions at this point. We need to remove them.
          clearTemporaryGrants(
            notification.browser,
            videoInputDevices.length && !sharingScreen,
            audioInputDevices.length
          );

          if (audioInputDevices.length) {
            lazy.SitePermissions.setForPrincipal(
              principal,
              "microphone",
              lazy.SitePermissions.BLOCK,
              scope,
              notification.browser
            );
          }
          if (videoInputDevices.length) {
            lazy.SitePermissions.setForPrincipal(
              principal,
              sharingScreen ? "screen" : "camera",
              lazy.SitePermissions.BLOCK,
              scope,
              notification.browser
            );
          }
        },
      },
    ];
  }

  let productName = lazy.gBrandBundle.GetStringFromName("brandShortName");

  let options = {
    name: lazy.webrtcUI.getHostOrExtensionName(principal.URI),
    persistent: true,
    hideClose: true,
    eventCallback(aTopic, aNewBrowser, isCancel) {
      if (aTopic == "swapping") {
        return true;
      }

      let doc = this.browser.ownerDocument;

      // Clean-up video streams of screensharing previews.
      if (
        ((aTopic == "dismissed" || aTopic == "removed") &&
          requestTypes.includes("Screen")) ||
        !requestTypes.includes("Screen")
      ) {
        let video = doc.getElementById("webRTC-previewVideo");
        if (video.stream) {
          video.stream.getTracks().forEach(t => t.stop());
          video.stream = null;
          video.src = null;
          doc.getElementById("webRTC-preview").hidden = true;
        }
        let menupopup = doc.getElementById("webRTC-selectWindow-menupopup");
        if (menupopup._commandEventListener) {
          menupopup.removeEventListener(
            "command",
            menupopup._commandEventListener
          );
          menupopup._commandEventListener = null;
        }
      }

      // If the notification has been cancelled (e.g. due to entering full-screen), also cancel the webRTC request
      if (aTopic == "removed" && notification && isCancel) {
        aActor.denyRequest(aRequest);
      }

      if (aTopic != "showing") {
        return false;
      }

      // If BLOCK has been set persistently in the permission manager or has
      // been set on the tab, then it is handled synchronously before we add
      // the notification.
      // Handling of ALLOW is delayed until the popupshowing event,
      // to avoid granting permissions automatically to background tabs.
      if (aActor.checkRequestAllowed(aRequest, principal, aBrowser)) {
        this.remove();
        return true;
      }

      function listDevices(menupopup, devices, labelID) {
        while (menupopup.lastChild) {
          menupopup.removeChild(menupopup.lastChild);
        }
        let menulist = menupopup.parentNode;
        // Removing the child nodes of the menupopup doesn't clear the value
        // attribute of the menulist. This can have unfortunate side effects
        // when the list is rebuilt with a different content, so we remove
        // the value attribute and unset the selectedItem explicitly.
        menulist.removeAttribute("value");
        menulist.selectedItem = null;

        for (let device of devices) {
          addDeviceToList(menupopup, device.name, device.deviceIndex);
        }

        let label = doc.getElementById(labelID);
        if (devices.length == 1) {
          label.value = devices[0].name;
          label.hidden = false;
          menulist.hidden = true;
        } else {
          label.hidden = true;
          menulist.hidden = false;
        }
      }

      let notificationElement = doc.getElementById(
        "webRTC-shareDevices-notification"
      );

      function checkDisabledWindowMenuItem() {
        let list = doc.getElementById("webRTC-selectWindow-menulist");
        let item = list.selectedItem;
        if (!item || item.hasAttribute("disabled")) {
          notificationElement.setAttribute("invalidselection", "true");
        } else {
          notificationElement.removeAttribute("invalidselection");
        }
      }

      function listScreenShareDevices(menupopup, devices) {
        while (menupopup.lastChild) {
          menupopup.removeChild(menupopup.lastChild);
        }

        // Removing the child nodes of the menupopup doesn't clear the value
        // attribute of the menulist. This can have unfortunate side effects
        // when the list is rebuilt with a different content, so we remove
        // the value attribute and unset the selectedItem explicitly.
        menupopup.parentNode.removeAttribute("value");
        menupopup.parentNode.selectedItem = null;

        let label = doc.getElementById("webRTC-selectWindow-label");
        const gumStringId = "getUserMedia.selectWindowOrScreen2";
        label.setAttribute(
          "value",
          stringBundle.getString(gumStringId + ".label")
        );
        label.setAttribute(
          "accesskey",
          stringBundle.getString(gumStringId + ".accesskey")
        );

        // "Select a Window or Screen" is the default because we can't and don't
        // want to pick a 'default' window to share (Full screen is "scary").
        addDeviceToList(
          menupopup,
          stringBundle.getString("getUserMedia.pickWindowOrScreen.label"),
          "-1"
        );
        menupopup.appendChild(doc.createXULElement("menuseparator"));

        let isPipeWire = false;

        // Build the list of 'devices'.
        let monitorIndex = 1;
        for (let i = 0; i < devices.length; ++i) {
          let device = devices[i];
          let type = device.mediaSource;
          let name;
          // Building screen list from available screens.
          if (type == "screen") {
            if (device.name == "Primary Monitor") {
              name = stringBundle.getString(
                "getUserMedia.shareEntireScreen.label"
              );
            } else {
              name = stringBundle.getFormattedString(
                "getUserMedia.shareMonitor.label",
                [monitorIndex]
              );
              ++monitorIndex;
            }
          } else {
            name = device.name;
            // When we share content by PipeWire add only one item to the device
            // list. When it's selected PipeWire portal dialog is opened and
            // user confirms actual window/screen sharing there.
            // Don't mark it as scary as there's an extra confirmation step by
            // PipeWire portal dialog.
            if (name == PIPEWIRE_PORTAL_NAME && device.id == PIPEWIRE_ID) {
              isPipeWire = true;
              let sawcStringId = "getUserMedia.sharePipeWirePortal.label";
              let item = addDeviceToList(
                menupopup,
                stringBundle.getString(sawcStringId),
                i,
                type
              );
              item.deviceId = device.id;
              item.mediaSource = type;

              // In this case the OS sharing dialog will be the only option and
              // can be safely pre-selected.
              menupopup.parentNode.selectedItem = item;
              menupopup.parentNode.disabled = true;
              break;
            }
            if (type == "application") {
              // The application names returned by the platform are of the form:
              // <window count>\x1e<application name>
              let sepIndex = name.indexOf("\x1e");
              let count = name.slice(0, sepIndex);
              let sawcStringId =
                "getUserMedia.shareApplicationWindowCount.label";
              name = lazy.PluralForm.get(
                parseInt(count),
                stringBundle.getString(sawcStringId)
              )
                .replace("#1", name.slice(sepIndex + 1))
                .replace("#2", count);
            }
          }
          let item = addDeviceToList(menupopup, name, i, type);
          item.deviceId = device.id;
          item.mediaSource = type;
          if (device.scary) {
            item.scary = true;
          }
        }

        // Always re-select the "No <type>" item.
        doc
          .getElementById("webRTC-selectWindow-menulist")
          .removeAttribute("value");
        doc.getElementById("webRTC-all-windows-shared").hidden = true;

        menupopup._commandEventListener = event => {
          checkDisabledWindowMenuItem();
          let video = doc.getElementById("webRTC-previewVideo");
          if (video.stream) {
            video.stream.getTracks().forEach(t => t.stop());
            video.stream = null;
          }

          let type = event.target.mediaSource;
          let deviceId = event.target.deviceId;
          if (deviceId == undefined) {
            doc.getElementById("webRTC-preview").hidden = true;
            video.src = null;
            return;
          }

          let scary = event.target.scary;
          let warning = doc.getElementById("webRTC-previewWarning");
          let warningBox = doc.getElementById("webRTC-previewWarningBox");
          warningBox.hidden = !scary;
          let chromeWin = doc.defaultView;
          if (scary) {
            warningBox.hidden = false;
            let string;
            let bundle = chromeWin.gNavigatorBundle;

            let learnMoreText = bundle.getString(
              "getUserMedia.shareScreen.learnMoreLabel"
            );
            let baseURL = Services.urlFormatter.formatURLPref(
              "app.support.baseURL"
            );

            if (type == "screen") {
              string = bundle.getString(
                "getUserMedia.shareScreenWarning2.message"
              );
            } else {
              let brand = doc
                .getElementById("bundle_brand")
                .getString("brandShortName");
              string = bundle.getFormattedString(
                "getUserMedia.shareFirefoxWarning2.message",
                [brand]
              );
            }

            warning.textContent = string;

            let learnMore = doc.getElementById(
              "webRTC-previewWarning-learnMore"
            );
            learnMore.setAttribute("href", baseURL + "screenshare-safety");
            learnMore.textContent = learnMoreText;

            // On Catalina, we don't want to blow our chance to show the
            // OS-level helper prompt to enable screen recording if the user
            // intends to reject anyway. OTOH showing it when they click Allow
            // is too late. A happy middle is to show it when the user makes a
            // choice in the picker. This already happens implicitly if the
            // user chooses "Entire desktop", as a side-effect of our preview,
            // we just need to also do it if they choose "Firefox". These are
            // the lone two options when permission is absent on Catalina.
            // Ironically, these are the two sources marked "scary" from a
            // web-sharing perspective, which is why this code resides here.
            // A restart doesn't appear to be necessary in spite of OS wording.
            let scrStatus = {};
            lazy.OSPermissions.getScreenCapturePermissionState(scrStatus);
            if (scrStatus.value == lazy.OSPermissions.PERMISSION_STATE_DENIED) {
              lazy.OSPermissions.maybeRequestScreenCapturePermission();
            }
          }

          let perms = Services.perms;
          let chromePrincipal = Services.scriptSecurityManager.getSystemPrincipal();
          perms.addFromPrincipal(
            chromePrincipal,
            "MediaManagerVideo",
            perms.ALLOW_ACTION,
            perms.EXPIRE_SESSION
          );

          if (!isPipeWire) {
            video.deviceId = deviceId;
            let constraints = {
              video: { mediaSource: type, deviceId: { exact: deviceId } },
            };
            chromeWin.navigator.mediaDevices.getUserMedia(constraints).then(
              stream => {
                if (video.deviceId != deviceId) {
                  // The user has selected a different device or closed the panel
                  // before getUserMedia finished.
                  stream.getTracks().forEach(t => t.stop());
                  return;
                }
                video.srcObject = stream;
                video.stream = stream;
                doc.getElementById("webRTC-preview").hidden = false;
                video.onloadedmetadata = function(e) {
                  video.play();
                };
              },
              err => {
                if (
                  err.name == "OverconstrainedError" &&
                  err.constraint == "deviceId"
                ) {
                  // Window has disappeared since enumeration, which can happen.
                  // No preview for you.
                  return;
                }
                Cu.reportError(
                  `error in preview: ${err.message} ${err.constraint}`
                );
              }
            );
          }
        };
        menupopup.addEventListener("command", menupopup._commandEventListener);
      }

      function addDeviceToList(menupopup, deviceName, deviceIndex, type) {
        let menuitem = doc.createXULElement("menuitem");
        menuitem.setAttribute("value", deviceIndex);
        menuitem.setAttribute("label", deviceName);
        menuitem.setAttribute("tooltiptext", deviceName);
        if (type) {
          menuitem.setAttribute("devicetype", type);
        }

        if (deviceIndex == "-1") {
          menuitem.setAttribute("disabled", true);
        }

        menupopup.appendChild(menuitem);
        return menuitem;
      }

      doc.getElementById("webRTC-selectCamera").hidden =
        !videoInputDevices.length || sharingScreen;
      doc.getElementById("webRTC-selectWindowOrScreen").hidden =
        !sharingScreen || !videoInputDevices.length;
      doc.getElementById("webRTC-selectMicrophone").hidden =
        !audioInputDevices.length || sharingAudio;
      doc.getElementById(
        "webRTC-selectSpeaker"
      ).hidden = !audioOutputDevices.length;

      let camMenupopup = doc.getElementById("webRTC-selectCamera-menupopup");
      let windowMenupopup = doc.getElementById("webRTC-selectWindow-menupopup");
      let micMenupopup = doc.getElementById(
        "webRTC-selectMicrophone-menupopup"
      );
      let speakerMenupopup = doc.getElementById(
        "webRTC-selectSpeaker-menupopup"
      );
      let describedByIDs = ["webRTC-shareDevices-notification-description"];
      let describedBySuffix = "icon";

      if (sharingScreen) {
        listScreenShareDevices(windowMenupopup, videoInputDevices);
        checkDisabledWindowMenuItem();
      } else {
        let labelID = "webRTC-selectCamera-single-device-label";
        listDevices(camMenupopup, videoInputDevices, labelID);
        notificationElement.removeAttribute("invalidselection");
        if (videoInputDevices.length == 1) {
          describedByIDs.push("webRTC-selectCamera-" + describedBySuffix);
          describedByIDs.push(labelID);
        }
      }

      if (!sharingAudio) {
        let labelID = "webRTC-selectMicrophone-single-device-label";
        listDevices(micMenupopup, audioInputDevices, labelID);
        if (audioInputDevices.length == 1) {
          describedByIDs.push("webRTC-selectMicrophone-" + describedBySuffix);
          describedByIDs.push(labelID);
        }
      }

      let labelID = "webRTC-selectSpeaker-single-device-label";
      listDevices(speakerMenupopup, audioOutputDevices, labelID);
      if (audioInputDevices.length == 1) {
        describedByIDs.push("webRTC-selectSpeaker-icon");
        describedByIDs.push(labelID);
      }

      // PopupNotifications knows to clear the aria-describedby attribute
      // when hiding, so we don't have to worry about cleaning it up ourselves.
      chromeDoc.defaultView.PopupNotifications.panel.setAttribute(
        "aria-describedby",
        describedByIDs.join(" ")
      );

      this.mainAction.callback = async function(aState) {
        let remember = false;
        let silenceNotifications = false;

        if (notificationSilencingEnabled && sharingScreen) {
          silenceNotifications = aState && aState.checkboxChecked;
        } else {
          remember = aState && aState.checkboxChecked;
        }

        let allowedDevices = [];
        let perms = Services.perms;
        if (videoInputDevices.length) {
          let listId =
            "webRTC-select" +
            (sharingScreen ? "Window" : "Camera") +
            "-menulist";
          let videoDeviceIndex = doc.getElementById(listId).value;
          let allowVideoDevice = videoDeviceIndex != "-1";
          if (allowVideoDevice) {
            allowedDevices.push(videoDeviceIndex);
            // Session permission will be removed after use
            // (it's really one-shot, not for the entire session)
            perms.addFromPrincipal(
              principal,
              "MediaManagerVideo",
              perms.ALLOW_ACTION,
              perms.EXPIRE_SESSION
            );
            let { mediaSource, id } = videoInputDevices.find(
              ({ deviceIndex }) => deviceIndex == videoDeviceIndex
            );
            aActor.activateDevicePerm(aRequest.windowID, mediaSource, id);
            if (remember) {
              lazy.SitePermissions.setForPrincipal(
                principal,
                "camera",
                lazy.SitePermissions.ALLOW
              );
            }
          }
        }
        if (audioInputDevices.length) {
          if (!sharingAudio) {
            let audioDeviceIndex = doc.getElementById(
              "webRTC-selectMicrophone-menulist"
            ).value;
            let allowMic = audioDeviceIndex != "-1";
            if (allowMic) {
              allowedDevices.push(audioDeviceIndex);
              let { mediaSource, id } = audioInputDevices.find(
                ({ deviceIndex }) => deviceIndex == audioDeviceIndex
              );
              aActor.activateDevicePerm(aRequest.windowID, mediaSource, id);
              if (remember) {
                lazy.SitePermissions.setForPrincipal(
                  principal,
                  "microphone",
                  lazy.SitePermissions.ALLOW
                );
              }
            }
          } else {
            // Only one device possible for audio capture.
            allowedDevices.push(0);
          }
        }
        if (audioOutputDevices.length) {
          let audioDeviceIndex = doc.getElementById(
            "webRTC-selectSpeaker-menulist"
          ).value;
          let allowSpeaker = audioDeviceIndex != "-1";
          if (allowSpeaker) {
            allowedDevices.push(audioDeviceIndex);
          }
        }

        if (!allowedDevices.length) {
          aActor.denyRequest(aRequest);
          return;
        }

        let camNeeded = !!videoInputDevices.length && !sharingScreen;
        let scrNeeded = !!videoInputDevices.length && sharingScreen;
        let micNeeded = !!audioInputDevices.length;
        let havePermission = await aActor.checkOSPermission(
          camNeeded,
          micNeeded,
          scrNeeded
        );
        if (!havePermission) {
          aActor.denyRequestNoPermission(aRequest);
          return;
        }

        aActor.sendAsyncMessage("webrtc:Allow", {
          callID: aRequest.callID,
          windowID: aRequest.windowID,
          devices: allowedDevices,
          suppressNotifications: silenceNotifications,
        });
      };

      // If we haven't handled the permission yet, we want to show the doorhanger.
      return false;
    },
  };

  function shouldShowAlwaysRemember() {
    // Don't offer "always remember" action in PB mode
    if (lazy.PrivateBrowsingUtils.isBrowserPrivate(aBrowser)) {
      return false;
    }

    // Don't offer "always remember" action in third party with no permission
    // delegation
    if (aRequest.isThirdPartyOrigin && !aRequest.shouldDelegatePermission) {
      return false;
    }

    // Don't offer "always remember" action in maybe unsafe permission
    // delegation
    if (aRequest.shouldDelegatePermission && aRequest.secondOrigin) {
      return false;
    }

    // "Always allow this speaker" not yet supported for
    // selectAudioOutput().  Bug 1712892
    if (audioOutputDevices.length) {
      return false;
    }

    return true;
  }

  if (shouldShowAlwaysRemember()) {
    // Disable the permanent 'Allow' action if the connection isn't secure, or for
    // screen/audio sharing (because we can't guess which window the user wants to
    // share without prompting). Note that we never enter this block for private
    // browsing windows.
    let reasonForNoPermanentAllow = "";
    if (sharingScreen) {
      reasonForNoPermanentAllow =
        "getUserMedia.reasonForNoPermanentAllow.screen3";
    } else if (sharingAudio) {
      reasonForNoPermanentAllow =
        "getUserMedia.reasonForNoPermanentAllow.audio";
    } else if (!aRequest.secure) {
      reasonForNoPermanentAllow =
        "getUserMedia.reasonForNoPermanentAllow.insecure";
    }

    options.checkbox = {
      label: stringBundle.getString("getUserMedia.remember"),
      checked: principal.isAddonOrExpandedAddonPrincipal,
      checkedState: reasonForNoPermanentAllow
        ? {
            disableMainAction: true,
            warningLabel: stringBundle.getFormattedString(
              reasonForNoPermanentAllow,
              [productName]
            ),
          }
        : undefined,
    };
  }

  // If the notification silencing feature is enabled and we're sharing a
  // screen, then the checkbox for the permission panel is what controls
  // notification silencing.
  if (notificationSilencingEnabled && sharingScreen) {
    let [silenceNotifications] = localization.formatMessagesSync([
      { id: "popup-mute-notifications-checkbox" },
    ]);

    options.checkbox = {
      label: silenceNotifications.value,
      checked: false,
      checkedState: {
        disableMainAction: false,
      },
    };
  }

  let iconType = "Devices";
  if (requestTypes.length == 1) {
    switch (requestTypes[0]) {
      case "AudioCapture":
        iconType = "Microphone";
        break;
      case "Microphone":
      case "Speaker":
        iconType = requestTypes[0];
        break;
    }
  }
  if (requestTypes.includes("Screen")) {
    iconType = "Screen";
  }
  let anchorId = "webRTC-share" + iconType + "-notification-icon";

  if (aRequest.secondOrigin) {
    options.secondName = lazy.webrtcUI.getHostOrExtensionName(
      null,
      aRequest.secondOrigin
    );
  }

  notification = chromeDoc.defaultView.PopupNotifications.show(
    aBrowser,
    "webRTC-shareDevices",
    message,
    anchorId,
    mainAction,
    secondaryActions,
    options
  );
  notification.callID = aRequest.callID;

  let schemeHistogram = Services.telemetry.getKeyedHistogramById(
    "PERMISSION_REQUEST_ORIGIN_SCHEME"
  );
  let userInputHistogram = Services.telemetry.getKeyedHistogramById(
    "PERMISSION_REQUEST_HANDLING_USER_INPUT"
  );

  let docURI = aRequest.documentURI;
  let scheme = 0;
  if (docURI.startsWith("https")) {
    scheme = 2;
  } else if (docURI.startsWith("http")) {
    scheme = 1;
  }

  for (let requestType of requestTypes) {
    if (requestType == "AudioCapture") {
      requestType = "Microphone";
    }
    requestType = requestType.toLowerCase();

    schemeHistogram.add(requestType, scheme);
    userInputHistogram.add(requestType, aRequest.isHandlingUserInput);
  }
}

/**
 * Checks whether we have a microphone/camera in use by checking the activePerms map
 * or if we have an allow permission for a microphone/camera in sitePermissions
 * @param {Browser} browser - Browser to find all active and allowed microphone and camera devices for
 * @return true if one of the above conditions is met
 */
function allowedOrActiveCameraOrMicrophone(browser) {
  // Do we have an allow permission for cam/mic in the permissions manager?
  if (
    lazy.SitePermissions.getAllForBrowser(browser).some(perm => {
      return (
        perm.state == lazy.SitePermissions.ALLOW &&
        (perm.id.startsWith("camera") || perm.id.startsWith("microphone"))
      );
    })
  ) {
    // Return early, no need to check for active devices
    return true;
  }

  // Do we have an active device?
  return (
    // Find all windowIDs that belong to our browsing contexts
    browser.browsingContext
      .getAllBrowsingContextsInSubtree()
      // Only keep the outerWindowIds
      .map(bc => bc.currentWindowGlobal?.outerWindowId)
      .filter(id => id != null)
      // We have an active device if one of our windowIds has a non empty map in the activePerms map
      // that includes one device of type "camera" or "microphone"
      .some(id => {
        let map = lazy.webrtcUI.activePerms.get(id);
        if (!map) {
          // This windowId has no active device
          return false;
        }
        // Let's see if one of the devices is a camera or a microphone
        let types = [...map.values()];
        return types.includes("microphone") || types.includes("camera");
      })
  );
}

function removePrompt(aBrowser, aCallId) {
  let chromeWin = aBrowser.ownerGlobal;
  let notification = chromeWin.PopupNotifications.getNotification(
    "webRTC-shareDevices",
    aBrowser
  );
  if (notification && notification.callID == aCallId) {
    notification.remove();
  }
}

/**
 * Clears temporary permission grants used for WebRTC device grace periods.
 * @param browser - Browser element to clear permissions for.
 * @param {boolean} clearCamera - Clear camera grants.
 * @param {boolean} clearMicrophone - Clear microphone grants.
 */
function clearTemporaryGrants(browser, clearCamera, clearMicrophone) {
  if (!clearCamera && !clearMicrophone) {
    // Nothing to clear.
    return;
  }
  let perms = lazy.SitePermissions.getAllForBrowser(browser);
  perms
    .filter(perm => {
      let [id, key] = perm.id.split(lazy.SitePermissions.PERM_KEY_DELIMITER);
      // We only want to clear WebRTC grace periods. These are temporary, device
      // specifc (double-keyed) microphone or camera permissions.
      return (
        key &&
        perm.state == lazy.SitePermissions.ALLOW &&
        perm.scope == lazy.SitePermissions.SCOPE_TEMPORARY &&
        ((clearCamera && id == "camera") ||
          (clearMicrophone && id == "microphone"))
      );
    })
    .forEach(perm =>
      lazy.SitePermissions.removeFromPrincipal(null, perm.id, browser)
    );
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["webrtcUI"];

ChromeUtils.import("resource:///modules/syncedtabs/EventEmitter.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "AppConstants",
                               "resource://gre/modules/AppConstants.jsm");
ChromeUtils.defineModuleGetter(this, "PluralForm",
                               "resource://gre/modules/PluralForm.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "SitePermissions",
                               "resource:///modules/SitePermissions.jsm");

XPCOMUtils.defineLazyGetter(this, "gBrandBundle", function() {
  return Services.strings.createBundle("chrome://branding/locale/brand.properties");
});

var webrtcUI = {
  peerConnectionBlockers: new Set(),
  emitter: new EventEmitter(),

  init() {
    Services.obs.addObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished");
    Services.ppmm.addMessageListener("child-process-shutdown", this);
  },

  uninit() {
    Services.obs.removeObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished");

    if (gIndicatorWindow) {
      gIndicatorWindow.close();
      gIndicatorWindow = null;
    }
  },

  processIndicators: new Map(),
  activePerms: new Map(),

  get showGlobalIndicator() {
    for (let [, indicators] of this.processIndicators) {
      if (indicators.showGlobalIndicator)
        return true;
    }
    return false;
  },

  get showCameraIndicator() {
    for (let [, indicators] of this.processIndicators) {
      if (indicators.showCameraIndicator)
        return true;
    }
    return false;
  },

  get showMicrophoneIndicator() {
    for (let [, indicators] of this.processIndicators) {
      if (indicators.showMicrophoneIndicator)
        return true;
    }
    return false;
  },

  get showScreenSharingIndicator() {
    let list = [""];
    for (let [, indicators] of this.processIndicators) {
      if (indicators.showScreenSharingIndicator)
        list.push(indicators.showScreenSharingIndicator);
    }

    let precedence =
      ["Screen", "Window", "Application", "Browser", ""];

    list.sort((a, b) => {
 return precedence.indexOf(a) -
                                 precedence.indexOf(b);
});

    return list[0];
  },

  _streams: [],
  // The boolean parameters indicate which streams should be included in the result.
  getActiveStreams(aCamera, aMicrophone, aScreen) {
    return webrtcUI._streams.filter(aStream => {
      let state = aStream.state;
      return aCamera && state.camera ||
             aMicrophone && state.microphone ||
             aScreen && state.screen;
    }).map(aStream => {
      let state = aStream.state;
      let types = {camera: state.camera, microphone: state.microphone,
                   screen: state.screen};
      let browser = aStream.browser;
      let browserWindow = browser.ownerGlobal;
      let tab = browserWindow.gBrowser &&
                browserWindow.gBrowser.getTabForBrowser(browser);
      return {uri: state.documentURI, tab, browser, types};
    });
  },

  swapBrowserForNotification(aOldBrowser, aNewBrowser) {
    for (let stream of this._streams) {
      if (stream.browser == aOldBrowser)
        stream.browser = aNewBrowser;
    }
  },

  forgetActivePermissionsFromBrowser(aBrowser) {
    webrtcUI.activePerms.delete(aBrowser.outerWindowID);
  },

  forgetStreamsFromBrowser(aBrowser) {
    this._streams = this._streams.filter(stream => stream.browser != aBrowser);
    webrtcUI.forgetActivePermissionsFromBrowser(aBrowser);
  },

  forgetStreamsFromProcess(aProcessMM) {
    // stream.processMM is null when e10s is disabled.
    this._streams =
      this._streams.filter(stream => stream.processMM &&
                                     stream.processMM != aProcessMM);
  },

  showSharingDoorhanger(aActiveStream) {
    let browserWindow = aActiveStream.browser.ownerGlobal;
    if (aActiveStream.tab) {
      browserWindow.gBrowser.selectedTab = aActiveStream.tab;
    } else {
      aActiveStream.browser.focus();
    }
    browserWindow.focus();
    let identityBox = browserWindow.document.getElementById("identity-box");
    if (AppConstants.platform == "macosx" && !Services.focus.activeWindow) {
      browserWindow.addEventListener("activate", function() {
        Services.tm.dispatchToMainThread(function() {
          identityBox.click();
        });
      }, {once: true});
      Cc["@mozilla.org/widget/macdocksupport;1"].getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
      return;
    }
    identityBox.click();
  },

  updateWarningLabel(aMenuList) {
    let type = aMenuList.selectedItem.getAttribute("devicetype");
    let document = aMenuList.ownerDocument;
    document.getElementById("webRTC-all-windows-shared").hidden = type != "Screen";
  },

  // Add-ons can override stock permission behavior by doing:
  //
  //   webrtcUI.addPeerConnectionBlocker(function(aParams) {
  //     // new permission checking logic
  //   }));
  //
  // The blocking function receives an object with origin, callID, and windowID
  // parameters.  If it returns the string "deny" or a Promise that resolves
  // to "deny", the connection is immediately blocked.  With any other return
  // value (though the string "allow" is suggested for consistency), control
  // is passed to other registered blockers.  If no registered blockers block
  // the connection (or of course if there are no registered blockers), then
  // the connection is allowed.
  //
  // Add-ons may also use webrtcUI.on/off to listen to events without
  // blocking anything:
  //   peer-request-allowed is emitted when a new peer connection is
  //                        established (and not blocked).
  //   peer-request-blocked is emitted when a peer connection request is
  //                        blocked by some blocking connection handler.
  //   peer-request-cancel is emitted when a peer-request connection request
  //                       is canceled.  (This would typically be used in
  //                       conjunction with a blocking handler to cancel
  //                       a user prompt or other work done by the handler)
  addPeerConnectionBlocker(aCallback) {
    this.peerConnectionBlockers.add(aCallback);
  },

  removePeerConnectionBlocker(aCallback) {
    this.peerConnectionBlockers.delete(aCallback);
  },

  on(...args) {
    return this.emitter.on(...args);
  },

  off(...args) {
    return this.emitter.off(...args);
  },

  // Listeners and observers are registered in nsBrowserGlue.js
  receiveMessage(aMessage) {
    switch (aMessage.name) {

      case "rtcpeer:Request": {
        let params = Object.freeze(Object.assign({
          origin: aMessage.target.contentPrincipal.origin
        }, aMessage.data));

        let blockers = Array.from(this.peerConnectionBlockers);

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
            this.emitter.emit("peer-request-allowed", params);
            message = "rtcpeer:Allow";
          } else {
            this.emitter.emit("peer-request-blocked", params);
            message = "rtcpeer:Deny";
          }

          aMessage.target.messageManager.sendAsyncMessage(message, {
            callID: params.callID,
            windowID: params.windowID,
          });
        });
        break;
      }
      case "rtcpeer:CancelRequest": {
        let params = Object.freeze({
          origin: aMessage.target.contentPrincipal.origin,
          callID: aMessage.data
        });
        this.emitter.emit("peer-request-cancel", params);
        break;
      }
      case "webrtc:Request":
        prompt(aMessage.target, aMessage.data);
        break;
      case "webrtc:StopRecording":
        stopRecording(aMessage.target, aMessage.data);
        break;
      case "webrtc:CancelRequest":
        removePrompt(aMessage.target, aMessage.data);
        break;
      case "webrtc:UpdatingIndicators":
        webrtcUI.forgetStreamsFromProcess(aMessage.target);
        break;
      case "webrtc:UpdateGlobalIndicators":
        updateIndicators(aMessage.data, aMessage.target);
        break;
      case "webrtc:UpdateBrowserIndicators":
        let id = aMessage.data.windowId;
        let processMM =
          aMessage.targetFrameLoader.messageManager.processMessageManager;
        let index;
        for (index = 0; index < webrtcUI._streams.length; ++index) {
          let stream = webrtcUI._streams[index];
          if (stream.state.windowId == id && stream.processMM == processMM)
            break;
        }
        // If there's no documentURI, the update is actually a removal of the
        // stream, triggered by the recording-window-ended notification.
        if (!aMessage.data.documentURI && index < webrtcUI._streams.length) {
          webrtcUI._streams.splice(index, 1);
        } else {
          webrtcUI._streams[index] = {
            browser: aMessage.target,
            processMM,
            state: aMessage.data
          };
        }
        let tabbrowser = aMessage.target.ownerGlobal.gBrowser;
        if (tabbrowser)
          tabbrowser.setBrowserSharing(aMessage.target, aMessage.data);
        break;
      case "child-process-shutdown":
        webrtcUI.processIndicators.delete(aMessage.target);
        webrtcUI.forgetStreamsFromProcess(aMessage.target);
        updateIndicators(null, null);
        break;
    }
  }
};

function denyRequest(aBrowser, aRequest) {
  aBrowser.messageManager.sendAsyncMessage("webrtc:Deny",
                                           {callID: aRequest.callID,
                                            windowID: aRequest.windowID});
}

function getHost(uri, href) {
  let host;
  try {
    if (!uri) {
      uri = Services.io.newURI(href);
    }
    host = uri.host;
  } catch (ex) {}
  if (!host) {
    if (uri && uri.scheme.toLowerCase() == "about") {
      // For about URIs, just use the full spec, without any #hash parts.
      host = uri.specIgnoringRef;
    } else {
      // This is unfortunate, but we should display *something*...
      const kBundleURI = "chrome://browser/locale/browser.properties";
      let bundle = Services.strings.createBundle(kBundleURI);
      host = bundle.GetStringFromName("getUserMedia.sharingMenuUnknownHost");
    }
  }
  return host;
}

function stopRecording(aBrowser, aRequest) {
  let outerWindowID = aBrowser.outerWindowID;

  if (!webrtcUI.activePerms.has(outerWindowID)) {
    return;
  }

  if (!aRequest.rawID) {
    webrtcUI.activePerms.delete(outerWindowID);
  } else {
    let set = webrtcUI.activePerms.get(outerWindowID);
    set.delete(aRequest.windowID + aRequest.mediaSource + aRequest.rawID);
  }
}

function prompt(aBrowser, aRequest) {
  let { audioDevices, videoDevices, sharingScreen, sharingAudio,
        requestTypes } = aRequest;

  let uri;
  try {
    // This fails for principals that serialize to "null", e.g. file URIs.
    uri = Services.io.newURI(aRequest.origin);
  } catch (e) {
    uri = Services.io.newURI(aRequest.documentURI);
  }

  // If the user has already denied access once in this tab,
  // deny again without even showing the notification icon.
  if ((audioDevices.length && SitePermissions
        .get(uri, "microphone", aBrowser).state == SitePermissions.BLOCK) ||
      (videoDevices.length && SitePermissions
        .get(uri, sharingScreen ? "screen" : "camera", aBrowser).state == SitePermissions.BLOCK)) {
    denyRequest(aBrowser, aRequest);
    return;
  }

  // Tell the browser to refresh the identity block display in case there
  // are expired permission states.
  aBrowser.dispatchEvent(new aBrowser.ownerGlobal
                                     .CustomEvent("PermissionStateChange"));

  let chromeDoc = aBrowser.ownerDocument;
  let stringBundle = chromeDoc.defaultView.gNavigatorBundle;

  // Mind the order, because for simplicity we're iterating over the list using
  // "includes()". This allows the rotation of string identifiers. We list the
  // full identifiers here so they can be cross-referenced more easily.
  let joinedRequestTypes = requestTypes.join("And");
  let stringId = [
    // Individual request types first.
    "getUserMedia.shareCamera2.message",
    "getUserMedia.shareMicrophone2.message",
    "getUserMedia.shareScreen3.message",
    "getUserMedia.shareAudioCapture2.message",
    // Combinations of the above request types last.
    "getUserMedia.shareCameraAndMicrophone2.message",
    "getUserMedia.shareCameraAndAudioCapture2.message",
    "getUserMedia.shareScreenAndMicrophone3.message",
    "getUserMedia.shareScreenAndAudioCapture3.message",
  ].find(id => id.includes(joinedRequestTypes));

  let message = stringBundle.getFormattedString(stringId, ["<>"], 1);

  let notification; // Used by action callbacks.
  let mainAction = {
    label: stringBundle.getString("getUserMedia.allow.label"),
    accessKey: stringBundle.getString("getUserMedia.allow.accesskey"),
    // The real callback will be set during the "showing" event. The
    // empty function here is so that PopupNotifications.show doesn't
    // reject the action.
    callback() {}
  };

  let secondaryActions = [
    {
      label: stringBundle.getString("getUserMedia.dontAllow.label"),
      accessKey: stringBundle.getString("getUserMedia.dontAllow.accesskey"),
      callback(aState) {
        denyRequest(notification.browser, aRequest);
        let scope = SitePermissions.SCOPE_TEMPORARY;
        if (aState && aState.checkboxChecked) {
          scope = SitePermissions.SCOPE_PERSISTENT;
        }
        if (audioDevices.length)
          SitePermissions.set(uri, "microphone",
                              SitePermissions.BLOCK, scope, notification.browser);
        if (videoDevices.length)
          SitePermissions.set(uri, sharingScreen ? "screen" : "camera",
                              SitePermissions.BLOCK, scope, notification.browser);
      }
    }
  ];

  let productName = gBrandBundle.GetStringFromName("brandShortName");

  let options = {
    name: getHost(uri),
    persistent: true,
    hideClose: !Services.prefs.getBoolPref("privacy.permissionPrompts.showCloseButton"),
    eventCallback(aTopic, aNewBrowser) {
      if (aTopic == "swapping")
        return true;

      let doc = this.browser.ownerDocument;

      // Clean-up video streams of screensharing previews.
      if ((aTopic == "dismissed" || aTopic == "removed") &&
          requestTypes.includes("Screen")) {
        let video = doc.getElementById("webRTC-previewVideo");
        video.deviceId = undefined;
        if (video.stream) {
          video.stream.getTracks().forEach(t => t.stop());
          video.stream = null;
          video.src = null;
          doc.getElementById("webRTC-preview").hidden = true;
        }
        let menupopup = doc.getElementById("webRTC-selectWindow-menupopup");
        if (menupopup._commandEventListener) {
          menupopup.removeEventListener("command", menupopup._commandEventListener);
          menupopup._commandEventListener = null;
        }
      }

      if (aTopic != "showing")
        return false;

      // BLOCK is handled immediately by MediaManager if it has been set
      // persistently in the permission manager. If it has been set on the tab,
      // it is handled synchronously before we add the notification.
      // Handling of ALLOW is delayed until the popupshowing event,
      // to avoid granting permissions automatically to background tabs.
      if (aRequest.secure) {
        let micAllowed =
          SitePermissions.get(uri, "microphone").state == SitePermissions.ALLOW;
        let camAllowed =
          SitePermissions.get(uri, "camera").state == SitePermissions.ALLOW;

        let perms = Services.perms;
        let mediaManagerPerm =
          perms.testExactPermission(uri, "MediaManagerVideo");
        if (mediaManagerPerm) {
          perms.remove(uri, "MediaManagerVideo");
        }

        // Screen sharing shouldn't follow the camera permissions.
        if (videoDevices.length && sharingScreen)
          camAllowed = false;

        let activeCamera;
        let activeMic;

        // Always prompt for screen sharing
        if (!sharingScreen) {
          for (let device of videoDevices) {
            let set = webrtcUI.activePerms.get(aBrowser.outerWindowID);
            if (set && set.has(aRequest.windowID + device.mediaSource + device.id)) {
              activeCamera = device;
              break;
            }
          }

          for (let device of audioDevices) {
            let set = webrtcUI.activePerms.get(aBrowser.outerWindowID);
            if (set && set.has(aRequest.windowID + device.mediaSource + device.id)) {
              activeMic = device;
              break;
            }
          }
        }

        if ((!audioDevices.length || micAllowed || activeMic) &&
            (!videoDevices.length || camAllowed || activeCamera)) {
          let allowedDevices = [];
          if (videoDevices.length) {
            allowedDevices.push((activeCamera || videoDevices[0]).deviceIndex);
            Services.perms.add(uri, "MediaManagerVideo",
                               Services.perms.ALLOW_ACTION,
                               Services.perms.EXPIRE_SESSION);
          }
          if (audioDevices.length) {
            allowedDevices.push((activeMic || audioDevices[0]).deviceIndex);
          }

          // Remember on which URIs we found persistent permissions so that we
          // can remove them if the user clicks 'Stop Sharing'. There's no
          // other way for the stop sharing code to know the hostnames of frames
          // using devices until bug 1066082 is fixed.
          let browser = this.browser;
          browser._devicePermissionURIs = browser._devicePermissionURIs || [];
          browser._devicePermissionURIs.push(uri);

          let mm = browser.messageManager;
          mm.sendAsyncMessage("webrtc:Allow", {callID: aRequest.callID,
                                               windowID: aRequest.windowID,
                                               devices: allowedDevices});
          this.remove();
          return true;
        }
      }

      function listDevices(menupopup, devices) {
        while (menupopup.lastChild)
          menupopup.removeChild(menupopup.lastChild);
        // Removing the child nodes of the menupopup doesn't clear the value
        // attribute of the menulist. This can have unfortunate side effects
        // when the list is rebuilt with a different content, so we remove
        // the value attribute explicitly.
        menupopup.parentNode.removeAttribute("value");

        for (let device of devices)
          addDeviceToList(menupopup, device.name, device.deviceIndex);
      }

      function checkDisabledWindowMenuItem() {
        let list = doc.getElementById("webRTC-selectWindow-menulist");
        let item = list.selectedItem;
        let notificationElement = doc.getElementById("webRTC-shareDevices-notification");
        if (!item || item.hasAttribute("disabled")) {
          notificationElement.setAttribute("invalidselection", "true");
        } else {
          notificationElement.removeAttribute("invalidselection");
        }
      }

      function listScreenShareDevices(menupopup, devices) {
        while (menupopup.lastChild)
          menupopup.removeChild(menupopup.lastChild);

        let type = devices[0].mediaSource;
        let typeName = type.charAt(0).toUpperCase() + type.substr(1);

        let label = doc.getElementById("webRTC-selectWindow-label");
        let gumStringId = "getUserMedia.select" + typeName;
        label.setAttribute("value",
                           stringBundle.getString(gumStringId + ".label"));
        label.setAttribute("accesskey",
                           stringBundle.getString(gumStringId + ".accesskey"));

        // "Select <type>" is the default because we can't pick a
        // 'default' window to share.
        addDeviceToList(menupopup,
                        stringBundle.getString("getUserMedia.pick" + typeName + ".label"),
                        "-1");
        menupopup.appendChild(doc.createElement("menuseparator"));

        // Build the list of 'devices'.
        let monitorIndex = 1;
        for (let i = 0; i < devices.length; ++i) {
          let device = devices[i];

          let name;
          // Building screen list from available screens.
          if (type == "screen") {
            if (device.name == "Primary Monitor") {
              name = stringBundle.getString("getUserMedia.shareEntireScreen.label");
            } else {
              name = stringBundle.getFormattedString("getUserMedia.shareMonitor.label",
                                                     [monitorIndex]);
              ++monitorIndex;
            }
          } else {
            name = device.name;
            if (type == "application") {
              // The application names returned by the platform are of the form:
              // <window count>\x1e<application name>
              let sepIndex = name.indexOf("\x1e");
              let count = name.slice(0, sepIndex);
              let sawcStringId = "getUserMedia.shareApplicationWindowCount.label";
              name = PluralForm.get(parseInt(count), stringBundle.getString(sawcStringId))
                               .replace("#1", name.slice(sepIndex + 1))
                               .replace("#2", count);
            }
          }
          let item = addDeviceToList(menupopup, name, i, typeName);
          item.deviceId = device.id;
          if (device.scary)
            item.scary = true;
        }

        // Always re-select the "No <type>" item.
        doc.getElementById("webRTC-selectWindow-menulist").removeAttribute("value");
        doc.getElementById("webRTC-all-windows-shared").hidden = true;

        menupopup._commandEventListener = event => {
          checkDisabledWindowMenuItem();
          let video = doc.getElementById("webRTC-previewVideo");
          if (video.stream) {
            video.stream.getTracks().forEach(t => t.stop());
            video.stream = null;
          }

          let deviceId = event.target.deviceId;
          if (deviceId == undefined) {
            doc.getElementById("webRTC-preview").hidden = true;
            video.src = null;
            return;
          }

          let scary = event.target.scary;
          let warning = doc.getElementById("webRTC-previewWarning");
          warning.hidden = !scary;
          let chromeWin = doc.defaultView;
          if (scary) {
            warning.hidden = false;
            let string;
            let bundle = chromeWin.gNavigatorBundle;

            let learnMoreText =
              bundle.getString("getUserMedia.shareScreen.learnMoreLabel");
            let baseURL =
              Services.urlFormatter.formatURLPref("app.support.baseURL");

            let learnMore = chromeWin.document.createElement("label");
            learnMore.className = "text-link";
            learnMore.setAttribute("href", baseURL + "screenshare-safety");
            learnMore.textContent = learnMoreText;

            if (type == "screen") {
              string = bundle.getFormattedString("getUserMedia.shareScreenWarning.message",
                                                 ["<>"]);
            } else {
              let brand =
                doc.getElementById("bundle_brand").getString("brandShortName");
              string = bundle.getFormattedString("getUserMedia.shareFirefoxWarning.message",
                                                 [brand, "<>"]);
            }

            let [pre, post] = string.split("<>");
            warning.textContent = pre;
            warning.appendChild(learnMore);
            warning.appendChild(chromeWin.document.createTextNode(post));
          }

          let perms = Services.perms;
          let chromeUri = Services.io.newURI(doc.documentURI);
          perms.add(chromeUri, "MediaManagerVideo", perms.ALLOW_ACTION,
                    perms.EXPIRE_SESSION);

          video.deviceId = deviceId;
          let constraints = { video: { mediaSource: type, deviceId: {exact: deviceId } } };
          chromeWin.navigator.mediaDevices.getUserMedia(constraints).then(stream => {
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
          });
        };
        menupopup.addEventListener("command", menupopup._commandEventListener);
      }

      function addDeviceToList(menupopup, deviceName, deviceIndex, type) {
        let menuitem = doc.createElement("menuitem");
        menuitem.setAttribute("value", deviceIndex);
        menuitem.setAttribute("label", deviceName);
        menuitem.setAttribute("tooltiptext", deviceName);
        if (type)
          menuitem.setAttribute("devicetype", type);

        if (deviceIndex == "-1")
          menuitem.setAttribute("disabled", true);

        menupopup.appendChild(menuitem);
        return menuitem;
      }

      doc.getElementById("webRTC-selectCamera").hidden = !videoDevices.length || sharingScreen;
      doc.getElementById("webRTC-selectWindowOrScreen").hidden = !sharingScreen || !videoDevices.length;
      doc.getElementById("webRTC-selectMicrophone").hidden = !audioDevices.length || sharingAudio;

      let camMenupopup = doc.getElementById("webRTC-selectCamera-menupopup");
      let windowMenupopup = doc.getElementById("webRTC-selectWindow-menupopup");
      let micMenupopup = doc.getElementById("webRTC-selectMicrophone-menupopup");
      if (sharingScreen) {
        listScreenShareDevices(windowMenupopup, videoDevices);
        checkDisabledWindowMenuItem();
      } else {
        listDevices(camMenupopup, videoDevices);
        doc.getElementById("webRTC-shareDevices-notification").removeAttribute("invalidselection");
      }

      if (!sharingAudio)
        listDevices(micMenupopup, audioDevices);

      this.mainAction.callback = function(aState) {
        let remember = aState && aState.checkboxChecked;
        let allowedDevices = [];
        let perms = Services.perms;
        if (videoDevices.length) {
          let listId = "webRTC-select" + (sharingScreen ? "Window" : "Camera") + "-menulist";
          let videoDeviceIndex = doc.getElementById(listId).value;
          let allowVideoDevice = videoDeviceIndex != "-1";
          if (allowVideoDevice) {
            allowedDevices.push(videoDeviceIndex);
            // Session permission will be removed after use
            // (it's really one-shot, not for the entire session)
            perms.add(uri, "MediaManagerVideo", perms.ALLOW_ACTION,
                      perms.EXPIRE_SESSION);
            if (!webrtcUI.activePerms.has(aBrowser.outerWindowID)) {
              webrtcUI.activePerms.set(aBrowser.outerWindowID, new Set());
            }

            for (let device of videoDevices) {
              if (device.deviceIndex == videoDeviceIndex) {
                webrtcUI.activePerms.get(aBrowser.outerWindowID)
                        .add(aRequest.windowID + device.mediaSource + device.id);
                break;
              }
            }
            if (remember)
              SitePermissions.set(uri, "camera", SitePermissions.ALLOW);
          }
        }
        if (audioDevices.length) {
          if (!sharingAudio) {
            let audioDeviceIndex = doc.getElementById("webRTC-selectMicrophone-menulist").value;
            let allowMic = audioDeviceIndex != "-1";
            if (allowMic) {
              allowedDevices.push(audioDeviceIndex);
              if (!webrtcUI.activePerms.has(aBrowser.outerWindowID)) {
                webrtcUI.activePerms.set(aBrowser.outerWindowID, new Set());
              }

              for (let device of audioDevices) {
                if (device.deviceIndex == audioDeviceIndex) {
                  webrtcUI.activePerms.get(aBrowser.outerWindowID)
                          .add(aRequest.windowID + device.mediaSource + device.id);
                  break;
                }
              }
              if (remember)
                SitePermissions.set(uri, "microphone", SitePermissions.ALLOW);
            }
          } else {
            // Only one device possible for audio capture.
            allowedDevices.push(0);
          }
        }

        if (!allowedDevices.length) {
          denyRequest(notification.browser, aRequest);
          return;
        }

        if (remember) {
          // Remember on which URIs we set persistent permissions so that we
          // can remove them if the user clicks 'Stop Sharing'.
          aBrowser._devicePermissionURIs = aBrowser._devicePermissionURIs || [];
          aBrowser._devicePermissionURIs.push(uri);
        }

        let mm = notification.browser.messageManager;
        mm.sendAsyncMessage("webrtc:Allow", {callID: aRequest.callID,
                                             windowID: aRequest.windowID,
                                             devices: allowedDevices});
      };
      return false;
    }
  };

  // Don't offer "always remember" action in PB mode.
  if (!PrivateBrowsingUtils.isBrowserPrivate(aBrowser)) {

    // Disable the permanent 'Allow' action if the connection isn't secure, or for
    // screen/audio sharing (because we can't guess which window the user wants to
    // share without prompting).
    let reasonForNoPermanentAllow = "";
    if (sharingScreen) {
      reasonForNoPermanentAllow = "getUserMedia.reasonForNoPermanentAllow.screen3";
    } else if (sharingAudio) {
      reasonForNoPermanentAllow = "getUserMedia.reasonForNoPermanentAllow.audio";
    } else if (!aRequest.secure) {
      reasonForNoPermanentAllow = "getUserMedia.reasonForNoPermanentAllow.insecure";
    }

    options.checkbox = {
      label: stringBundle.getString("getUserMedia.remember"),
      checkedState: reasonForNoPermanentAllow ? {
        disableMainAction: true,
        warningLabel: stringBundle.getFormattedString(reasonForNoPermanentAllow,
                                                      [productName])
      } : undefined,
    };
  }

  let iconType = "Devices";
  if (requestTypes.length == 1 && (requestTypes[0] == "Microphone" ||
                                   requestTypes[0] == "AudioCapture"))
    iconType = "Microphone";
  if (requestTypes.includes("Screen"))
    iconType = "Screen";
  let anchorId = "webRTC-share" + iconType + "-notification-icon";

  let iconClass = iconType.toLowerCase();
  if (iconClass == "devices")
    iconClass = "camera";
  options.popupIconClass = iconClass + "-icon";

  notification =
    chromeDoc.defaultView
             .PopupNotifications
             .show(aBrowser, "webRTC-shareDevices", message,
                   anchorId, mainAction, secondaryActions,
                   options);
  notification.callID = aRequest.callID;

  let schemeHistogram = Services.telemetry.getKeyedHistogramById("PERMISSION_REQUEST_ORIGIN_SCHEME");
  let thirdPartyHistogram = Services.telemetry.getKeyedHistogramById("PERMISSION_REQUEST_THIRD_PARTY_ORIGIN");
  let userInputHistogram = Services.telemetry.getKeyedHistogramById("PERMISSION_REQUEST_HANDLING_USER_INPUT");

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
    thirdPartyHistogram.add(requestType, aRequest.isThirdPartyOrigin);
    userInputHistogram.add(requestType, aRequest.isHandlingUserInput);
  }
}

function removePrompt(aBrowser, aCallId) {
  let chromeWin = aBrowser.ownerGlobal;
  let notification =
    chromeWin.PopupNotifications.getNotification("webRTC-shareDevices", aBrowser);
  if (notification && notification.callID == aCallId)
    notification.remove();
}

function getGlobalIndicator() {
  if (AppConstants.platform != "macosx") {
    const INDICATOR_CHROME_URI = "chrome://browser/content/webrtcIndicator.xul";
    const features = "chrome,dialog=yes,titlebar=no,popup=yes";

    return Services.ww.openWindow(null, INDICATOR_CHROME_URI, "_blank", features, []);
  }

  let indicator = {
    _camera: null,
    _microphone: null,
    _screen: null,

    _hiddenDoc: Services.appShell.hiddenDOMWindow.document,
    _statusBar: Cc["@mozilla.org/widget/macsystemstatusbar;1"]
                  .getService(Ci.nsISystemStatusBar),

    _command(aEvent) {
      webrtcUI.showSharingDoorhanger(aEvent.target.stream);
    },

    _popupShowing(aEvent) {
      let type = this.getAttribute("type");
      let activeStreams;
      if (type == "Camera") {
        activeStreams = webrtcUI.getActiveStreams(true, false, false);
      } else if (type == "Microphone") {
        activeStreams = webrtcUI.getActiveStreams(false, true, false);
      } else if (type == "Screen") {
        activeStreams = webrtcUI.getActiveStreams(false, false, true);
        type = webrtcUI.showScreenSharingIndicator;
      }

      let bundle =
        Services.strings.createBundle("chrome://browser/locale/webrtcIndicator.properties");

      if (activeStreams.length == 1) {
        let stream = activeStreams[0];

        let menuitem = this.ownerDocument.createElement("menuitem");
        let labelId = "webrtcIndicator.sharing" + type + "With.menuitem";
        let label = stream.browser.contentTitle || stream.uri;
        menuitem.setAttribute("label", bundle.formatStringFromName(labelId, [label], 1));
        menuitem.setAttribute("disabled", "true");
        this.appendChild(menuitem);

        menuitem = this.ownerDocument.createElement("menuitem");
        menuitem.setAttribute("label",
                              bundle.GetStringFromName("webrtcIndicator.controlSharing.menuitem"));
        menuitem.stream = stream;
        menuitem.addEventListener("command", indicator._command);

        this.appendChild(menuitem);
        return true;
      }

      // We show a different menu when there are several active streams.
      let menuitem = this.ownerDocument.createElement("menuitem");
      let labelId = "webrtcIndicator.sharing" + type + "WithNTabs.menuitem";
      let count = activeStreams.length;
      let label = PluralForm.get(count, bundle.GetStringFromName(labelId)).replace("#1", count);
      menuitem.setAttribute("label", label);
      menuitem.setAttribute("disabled", "true");
      this.appendChild(menuitem);

      for (let stream of activeStreams) {
        let item = this.ownerDocument.createElement("menuitem");
        labelId = "webrtcIndicator.controlSharingOn.menuitem";
        label = stream.browser.contentTitle || stream.uri;
        item.setAttribute("label", bundle.formatStringFromName(labelId, [label], 1));
        item.stream = stream;
        item.addEventListener("command", indicator._command);
        this.appendChild(item);
      }

      return true;
    },

    _popupHiding(aEvent) {
      while (this.firstChild)
        this.firstChild.remove();
    },

    _setIndicatorState(aName, aState) {
      let field = "_" + aName.toLowerCase();
      if (aState && !this[field]) {
        let menu = this._hiddenDoc.createElement("menu");
        menu.setAttribute("id", "webRTC-sharing" + aName + "-menu");

        // The CSS will only be applied if the menu is actually inserted in the DOM.
        this._hiddenDoc.documentElement.appendChild(menu);

        this._statusBar.addItem(menu);

        let menupopup = this._hiddenDoc.createElement("menupopup");
        menupopup.setAttribute("type", aName);
        menupopup.addEventListener("popupshowing", this._popupShowing);
        menupopup.addEventListener("popuphiding", this._popupHiding);
        menupopup.addEventListener("command", this._command);
        menu.appendChild(menupopup);

        this[field] = menu;
      } else if (this[field] && !aState) {
        this._statusBar.removeItem(this[field]);
        this[field].remove();
        this[field] = null;
      }
    },
    updateIndicatorState() {
      this._setIndicatorState("Camera", webrtcUI.showCameraIndicator);
      this._setIndicatorState("Microphone", webrtcUI.showMicrophoneIndicator);
      this._setIndicatorState("Screen", webrtcUI.showScreenSharingIndicator);
    },
    close() {
      this._setIndicatorState("Camera", false);
      this._setIndicatorState("Microphone", false);
      this._setIndicatorState("Screen", false);
    }
  };

  indicator.updateIndicatorState();
  return indicator;
}

function onTabSharingMenuPopupShowing(e) {
  let streams = webrtcUI.getActiveStreams(true, true, true);
  for (let streamInfo of streams) {
    let stringName = "getUserMedia.sharingMenu";
    let types = streamInfo.types;
    if (types.camera)
      stringName += "Camera";
    if (types.microphone)
      stringName += "Microphone";
    if (types.screen)
      stringName += types.screen;

    let doc = e.target.ownerDocument;
    let bundle = doc.defaultView.gNavigatorBundle;

    let origin = getHost(null, streamInfo.uri);
    let menuitem = doc.createElement("menuitem");
    menuitem.setAttribute("label", bundle.getFormattedString(stringName, [origin]));
    menuitem.stream = streamInfo;
    menuitem.addEventListener("command", onTabSharingMenuPopupCommand);
    e.target.appendChild(menuitem);
  }
}

function onTabSharingMenuPopupHiding(e) {
  while (this.lastChild)
    this.lastChild.remove();
}

function onTabSharingMenuPopupCommand(e) {
  webrtcUI.showSharingDoorhanger(e.target.stream);
}

function showOrCreateMenuForWindow(aWindow) {
  let document = aWindow.document;
  let menu = document.getElementById("tabSharingMenu");
  if (!menu) {
    let stringBundle = aWindow.gNavigatorBundle;
    menu = document.createElement("menu");
    menu.id = "tabSharingMenu";
    let labelStringId = "getUserMedia.sharingMenu.label";
    menu.setAttribute("label", stringBundle.getString(labelStringId));

    let container, insertionPoint;
    if (AppConstants.platform == "macosx") {
      container = document.getElementById("windowPopup");
      insertionPoint = document.getElementById("sep-window-list");
      let separator = document.createElement("menuseparator");
      separator.id = "tabSharingSeparator";
      container.insertBefore(separator, insertionPoint);
    } else {
      let accesskeyStringId = "getUserMedia.sharingMenu.accesskey";
      menu.setAttribute("accesskey", stringBundle.getString(accesskeyStringId));
      container = document.getElementById("main-menubar");
      insertionPoint = document.getElementById("helpMenu");
    }
    let popup = document.createElement("menupopup");
    popup.id = "tabSharingMenuPopup";
    popup.addEventListener("popupshowing", onTabSharingMenuPopupShowing);
    popup.addEventListener("popuphiding", onTabSharingMenuPopupHiding);
    menu.appendChild(popup);
    container.insertBefore(menu, insertionPoint);
  } else {
    menu.hidden = false;
    if (AppConstants.platform == "macosx") {
      document.getElementById("tabSharingSeparator").hidden = false;
    }
  }
}

function maybeAddMenuIndicator(window) {
  if (webrtcUI.showGlobalIndicator) {
    showOrCreateMenuForWindow(window);
  }
}

var gIndicatorWindow = null;

function updateIndicators(data, target) {
  if (data) {
    // the global indicators specific to this process
    let indicators;
    if (webrtcUI.processIndicators.has(target)) {
      indicators = webrtcUI.processIndicators.get(target);
    } else {
      indicators = {};
      webrtcUI.processIndicators.set(target, indicators);
    }

    indicators.showGlobalIndicator = data.showGlobalIndicator;
    indicators.showCameraIndicator = data.showCameraIndicator;
    indicators.showMicrophoneIndicator = data.showMicrophoneIndicator;
    indicators.showScreenSharingIndicator = data.showScreenSharingIndicator;
  }

  let browserWindowEnum = Services.wm.getEnumerator("navigator:browser");
  while (browserWindowEnum.hasMoreElements()) {
    let chromeWin = browserWindowEnum.getNext();
    if (webrtcUI.showGlobalIndicator) {
      showOrCreateMenuForWindow(chromeWin);
    } else {
      let doc = chromeWin.document;
      let existingMenu = doc.getElementById("tabSharingMenu");
      if (existingMenu) {
        existingMenu.hidden = true;
      }
      if (AppConstants.platform == "macosx") {
        let separator = doc.getElementById("tabSharingSeparator");
        if (separator) {
          separator.hidden = true;
        }
      }
    }
  }

  if (webrtcUI.showGlobalIndicator) {
    if (!gIndicatorWindow) {
      gIndicatorWindow = getGlobalIndicator();
    } else {
      try {
        gIndicatorWindow.updateIndicatorState();
      } catch (err) {
        Cu.reportError(`error in gIndicatorWindow.updateIndicatorState(): ${err.message}`);
      }
    }
  } else if (gIndicatorWindow) {
    gIndicatorWindow.close();
    gIndicatorWindow = null;
  }
}

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["webrtcUI"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

this.webrtcUI = {
  init: function () {
    Services.obs.addObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished", false);

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);
    ppmm.addMessageListener("webrtc:UpdatingIndicators", this);
    ppmm.addMessageListener("webrtc:UpdateGlobalIndicators", this);
    ppmm.addMessageListener("child-process-shutdown", this);

    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
    mm.addMessageListener("rtcpeer:Request", this);
    mm.addMessageListener("rtcpeer:CancelRequest", this);
    mm.addMessageListener("webrtc:Request", this);
    mm.addMessageListener("webrtc:CancelRequest", this);
    mm.addMessageListener("webrtc:UpdateBrowserIndicators", this);
  },

  uninit: function () {
    Services.obs.removeObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished");

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);
    ppmm.removeMessageListener("webrtc:UpdatingIndicators", this);
    ppmm.removeMessageListener("webrtc:UpdateGlobalIndicators", this);

    let mm = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIMessageListenerManager);
    mm.removeMessageListener("rtcpeer:Request", this);
    mm.removeMessageListener("rtcpeer:CancelRequest", this);
    mm.removeMessageListener("webrtc:Request", this);
    mm.removeMessageListener("webrtc:CancelRequest", this);
    mm.removeMessageListener("webrtc:UpdateBrowserIndicators", this);

    if (gIndicatorWindow) {
      gIndicatorWindow.close();
      gIndicatorWindow = null;
    }
  },

  processIndicators: new Map(),

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

    list.sort((a, b) => { return precedence.indexOf(a) -
                                 precedence.indexOf(b); });

    return list[0];
  },

  _streams: [],
  // The boolean parameters indicate which streams should be included in the result.
  getActiveStreams: function(aCamera, aMicrophone, aScreen) {
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
      return {uri: state.documentURI, tab: tab, browser: browser, types: types};
    });
  },

  swapBrowserForNotification: function(aOldBrowser, aNewBrowser) {
    for (let stream of this._streams) {
      if (stream.browser == aOldBrowser)
        stream.browser = aNewBrowser;
    }
  },

  forgetStreamsFromBrowser: function(aBrowser) {
    this._streams = this._streams.filter(stream => stream.browser != aBrowser);
  },

  showSharingDoorhanger: function(aActiveStream, aType) {
    let browserWindow = aActiveStream.browser.ownerGlobal;
    if (aActiveStream.tab) {
      browserWindow.gBrowser.selectedTab = aActiveStream.tab;
    } else {
      aActiveStream.browser.focus();
    }
    browserWindow.focus();
    let identityBox = browserWindow.document.getElementById("identity-box");
    if (AppConstants.platform == "macosx" && !Services.focus.activeWindow) {
      browserWindow.addEventListener("activate", function onActivate() {
        browserWindow.removeEventListener("activate", onActivate);
        Services.tm.mainThread.dispatch(function() {
          identityBox.click();
        }, Ci.nsIThread.DISPATCH_NORMAL);
      });
      Cc["@mozilla.org/widget/macdocksupport;1"].getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
      return;
    }
    identityBox.click();
  },

  updateMainActionLabel: function(aMenuList) {
    let type = aMenuList.selectedItem.getAttribute("devicetype");
    let document = aMenuList.ownerDocument;
    document.getElementById("webRTC-all-windows-shared").hidden = type != "Screen";

    // If we are also requesting audio in addition to screen sharing,
    // always use a generic label.
    if (!document.getElementById("webRTC-selectMicrophone").hidden)
      type = "";

    let bundle = document.defaultView.gNavigatorBundle;
    let stringId = "getUserMedia.share" + (type || "SelectedItems") + ".label";
    let popupnotification = aMenuList.parentNode.parentNode;
    popupnotification.setAttribute("buttonlabel", bundle.getString(stringId));
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {

      // Add-ons can override stock permission behavior by doing:
      //
      //   var stockReceiveMessage = webrtcUI.receiveMessage;
      //
      //   webrtcUI.receiveMessage = function(aMessage) {
      //     switch (aMessage.name) {
      //      case "rtcpeer:Request": {
      //        // new code.
      //        break;
      //      ...
      //      default:
      //        return stockReceiveMessage.call(this, aMessage);
      //
      // Intercepting gUM and peerConnection requests should let an add-on
      // limit PeerConnection activity with automatic rules and/or prompts
      // in a sensible manner that avoids double-prompting in typical
      // gUM+PeerConnection scenarios. For example:
      //
      //   State                                    Sample Action
      //   --------------------------------------------------------------
      //   No IP leaked yet + No gUM granted        Warn user
      //   No IP leaked yet + gUM granted           Avoid extra dialog
      //   No IP leaked yet + gUM request pending.  Delay until gUM grant
      //   IP already leaked                        Too late to warn

      case "rtcpeer:Request": {
        // Always allow. This code-point exists for add-ons to override.
        let { callID, windowID } = aMessage.data;
        // Also available: isSecure, innerWindowID. For contentWindow:
        //
        //   let contentWindow = Services.wm.getOuterWindowWithId(windowID);

        let mm = aMessage.target.messageManager;
        mm.sendAsyncMessage("rtcpeer:Allow",
                            { callID: callID, windowID: windowID });
        break;
      }
      case "rtcpeer:CancelRequest":
        // No data to release. This code-point exists for add-ons to override.
        break;
      case "webrtc:Request":
        prompt(aMessage.target, aMessage.data);
        break;
      case "webrtc:CancelRequest":
        removePrompt(aMessage.target, aMessage.data);
        break;
      case "webrtc:UpdatingIndicators":
        webrtcUI._streams = [];
        break;
      case "webrtc:UpdateGlobalIndicators":
        updateIndicators(aMessage.data, aMessage.target);
        break;
      case "webrtc:UpdateBrowserIndicators":
        let id = aMessage.data.windowId;
        let index;
        for (index = 0; index < webrtcUI._streams.length; ++index) {
          if (webrtcUI._streams[index].state.windowId == id)
            break;
        }
        // If there's no documentURI, the update is actually a removal of the
        // stream, triggered by the recording-window-ended notification.
        if (!aMessage.data.documentURI && index < webrtcUI._streams.length)
          webrtcUI._streams.splice(index, 1);
        else
          webrtcUI._streams[index] = {browser: aMessage.target, state: aMessage.data};
        let tabbrowser = aMessage.target.ownerGlobal.gBrowser;
        if (tabbrowser)
          tabbrowser.setBrowserSharing(aMessage.target, aMessage.data);
        break;
      case "child-process-shutdown":
        webrtcUI.processIndicators.delete(aMessage.target);
        updateIndicators(null, null);
        break;
    }
  }
};

function getBrowserForWindow(aContentWindow) {
  return aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                       .getInterface(Ci.nsIWebNavigation)
                       .QueryInterface(Ci.nsIDocShell)
                       .chromeEventHandler;
}

function denyRequest(aBrowser, aRequest) {
  aBrowser.messageManager.sendAsyncMessage("webrtc:Deny",
                                           {callID: aRequest.callID,
                                            windowID: aRequest.windowID});
}

function getHost(uri, href) {
  let host;
  try {
    if (!uri) {
      uri = Services.io.newURI(href, null, null);
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

function prompt(aBrowser, aRequest) {
  let {audioDevices: audioDevices, videoDevices: videoDevices,
       sharingScreen: sharingScreen, sharingAudio: sharingAudio,
       requestTypes: requestTypes} = aRequest;
  let uri = Services.io.newURI(aRequest.documentURI, null, null);
  let host = getHost(uri);
  let chromeDoc = aBrowser.ownerDocument;
  let chromeWin = chromeDoc.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;
  let stringId = "getUserMedia.share" + requestTypes.join("And") + ".message";
  let message = stringBundle.getFormattedString(stringId, [host]);

  let mainLabel;
  if (sharingScreen || sharingAudio) {
    mainLabel = stringBundle.getString("getUserMedia.shareSelectedItems.label");
  } else {
    let string = stringBundle.getString("getUserMedia.shareSelectedDevices.label");
    mainLabel = PluralForm.get(requestTypes.length, string);
  }

  let notification; // Used by action callbacks.
  let mainAction = {
    label: mainLabel,
    accessKey: stringBundle.getString("getUserMedia.shareSelectedDevices.accesskey"),
    // The real callback will be set during the "showing" event. The
    // empty function here is so that PopupNotifications.show doesn't
    // reject the action.
    callback: function() {}
  };

  let secondaryActions = [
    {
      label: stringBundle.getString("getUserMedia.denyRequest.label"),
      accessKey: stringBundle.getString("getUserMedia.denyRequest.accesskey"),
      callback: function () {
        denyRequest(notification.browser, aRequest);
      }
    }
  ];
  // Bug 1037438: implement 'never' for screen sharing.
  if (!sharingScreen && !sharingAudio) {
    secondaryActions.push({
      label: stringBundle.getString("getUserMedia.never.label"),
      accessKey: stringBundle.getString("getUserMedia.never.accesskey"),
      callback: function () {
        denyRequest(notification.browser, aRequest);
        // Let someone save "Never" for http sites so that they can be stopped from
        // bothering you with doorhangers.
        let perms = Services.perms;
        if (audioDevices.length)
          perms.add(uri, "microphone", perms.DENY_ACTION);
        if (videoDevices.length)
          perms.add(uri, "camera", perms.DENY_ACTION);
      }
    });
  }

  if (aRequest.secure && !sharingScreen && !sharingAudio) {
    // Don't show the 'Always' action if the connection isn't secure, or for
    // screen/audio sharing (because we can't guess which window the user wants
    // to share without prompting).
    secondaryActions.unshift({
      label: stringBundle.getString("getUserMedia.always.label"),
      accessKey: stringBundle.getString("getUserMedia.always.accesskey"),
      callback: function (aState) {
        mainAction.callback(aState, true);
      }
    });
  }

  let options = {
    eventCallback: function(aTopic, aNewBrowser) {
      if (aTopic == "swapping")
        return true;

      let chromeDoc = this.browser.ownerDocument;

      // Clean-up video streams of screensharing previews.
      if ((aTopic == "dismissed" || aTopic == "removed") &&
          requestTypes.includes("Screen")) {
        let video = chromeDoc.getElementById("webRTC-previewVideo");
        video.deviceId = undefined;
        if (video.stream) {
          video.stream.getTracks().forEach(t => t.stop());
          video.stream = null;
          video.src = null;
          chromeDoc.getElementById("webRTC-preview").hidden = true;
        }
        let menupopup = chromeDoc.getElementById("webRTC-selectWindow-menupopup");
        if (menupopup._commandEventListener) {
          menupopup.removeEventListener("command", menupopup._commandEventListener);
          menupopup._commandEventListener = null;
        }
      }

      if (aTopic != "showing")
        return false;

      // DENY_ACTION is handled immediately by MediaManager, but handling
      // of ALLOW_ACTION is delayed until the popupshowing event
      // to avoid granting permissions automatically to background tabs.
      if (aRequest.secure) {
        let perms = Services.perms;

        let micPerm = perms.testExactPermission(uri, "microphone");
        if (micPerm == perms.PROMPT_ACTION)
          micPerm = perms.UNKNOWN_ACTION;

        let camPerm = perms.testExactPermission(uri, "camera");

        let mediaManagerPerm =
          perms.testExactPermission(uri, "MediaManagerVideo");
        if (mediaManagerPerm) {
          perms.remove(uri, "MediaManagerVideo");
        }

        if (camPerm == perms.PROMPT_ACTION)
          camPerm = perms.UNKNOWN_ACTION;

        // Screen sharing shouldn't follow the camera permissions.
        if (videoDevices.length && sharingScreen)
          camPerm = perms.UNKNOWN_ACTION;

        // We don't check that permissions are set to ALLOW_ACTION in this
        // test; only that they are set. This is because if audio is allowed
        // and video is denied persistently, we don't want to show the prompt,
        // and will grant audio access immediately.
        if ((!audioDevices.length || micPerm) && (!videoDevices.length || camPerm)) {
          // All permissions we were about to request are already persistently set.
          let allowedDevices = [];
          if (videoDevices.length && camPerm == perms.ALLOW_ACTION) {
            allowedDevices.push(videoDevices[0].deviceIndex);
            let perms = Services.perms;
            perms.add(uri, "MediaManagerVideo", perms.ALLOW_ACTION,
                      perms.EXPIRE_SESSION);
          }
          if (audioDevices.length && micPerm == perms.ALLOW_ACTION)
            allowedDevices.push(audioDevices[0].deviceIndex);

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

        for (let device of devices)
          addDeviceToList(menupopup, device.name, device.deviceIndex);
      }

      function listScreenShareDevices(menupopup, devices) {
        while (menupopup.lastChild)
          menupopup.removeChild(menupopup.lastChild);

        let type = devices[0].mediaSource;
        let typeName = type.charAt(0).toUpperCase() + type.substr(1);

        let label = chromeDoc.getElementById("webRTC-selectWindow-label");
        let stringId = "getUserMedia.select" + typeName;
        label.setAttribute("value",
                           stringBundle.getString(stringId + ".label"));
        label.setAttribute("accesskey",
                           stringBundle.getString(stringId + ".accesskey"));

        // "No <type>" is the default because we can't pick a
        // 'default' window to share.
        addDeviceToList(menupopup,
                        stringBundle.getString("getUserMedia.no" + typeName + ".label"),
                        "-1");
        menupopup.appendChild(chromeDoc.createElement("menuseparator"));

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
          }
          else {
            name = device.name;
            if (type == "application") {
              // The application names returned by the platform are of the form:
              // <window count>\x1e<application name>
              let sepIndex = name.indexOf("\x1e");
              let count = name.slice(0, sepIndex);
              let stringId = "getUserMedia.shareApplicationWindowCount.label";
              name = PluralForm.get(parseInt(count), stringBundle.getString(stringId))
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
        chromeDoc.getElementById("webRTC-selectWindow-menulist").removeAttribute("value");
        chromeDoc.getElementById("webRTC-all-windows-shared").hidden = true;
        menupopup._commandEventListener = event => {
          let video = chromeDoc.getElementById("webRTC-previewVideo");
          if (video.stream) {
            video.stream.getTracks().forEach(t => t.stop());
            video.stream = null;
          }

          let deviceId = event.target.deviceId;
          if (deviceId == undefined) {
            chromeDoc.getElementById("webRTC-preview").hidden = true;
            video.src = null;
            return;
          }

          let scary = event.target.scary;
          let warning = chromeDoc.getElementById("webRTC-previewWarning");
          warning.hidden = !scary;
          let chromeWin = chromeDoc.defaultView;
          if (scary) {
            warning.hidden = false;
            let string;
            let bundle = chromeWin.gNavigatorBundle;

            let learnMoreText =
              bundle.getString("getUserMedia.shareScreen.learnMoreLabel");
            let baseURL =
              Services.urlFormatter.formatURLPref("app.support.baseURL");
            let learnMore =
              "<label class='text-link' href='" + baseURL + "screenshare-safety'>" +
              learnMoreText + "</label>";

            if (type == "screen") {
              string = bundle.getFormattedString("getUserMedia.shareScreenWarning.message",
                                                 [learnMore]);
            }
            else {
              let brand =
                chromeDoc.getElementById("bundle_brand").getString("brandShortName");
              string = bundle.getFormattedString("getUserMedia.shareFirefoxWarning.message",
                                                 [brand, learnMore]);
            }
            warning.innerHTML = string;
          }

          let perms = Services.perms;
          let chromeUri = Services.io.newURI(chromeDoc.documentURI, null, null);
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
            video.src = chromeWin.URL.createObjectURL(stream);
            video.stream = stream;
            chromeDoc.getElementById("webRTC-preview").hidden = false;
            video.onloadedmetadata = function(e) {
              video.play();
            };
          });
        };
        menupopup.addEventListener("command", menupopup._commandEventListener);
      }

      function addDeviceToList(menupopup, deviceName, deviceIndex, type) {
        let menuitem = chromeDoc.createElement("menuitem");
        menuitem.setAttribute("value", deviceIndex);
        menuitem.setAttribute("label", deviceName);
        menuitem.setAttribute("tooltiptext", deviceName);
        if (type)
          menuitem.setAttribute("devicetype", type);
        menupopup.appendChild(menuitem);
        return menuitem;
      }

      chromeDoc.getElementById("webRTC-selectCamera").hidden = !videoDevices.length || sharingScreen;
      chromeDoc.getElementById("webRTC-selectWindowOrScreen").hidden = !sharingScreen || !videoDevices.length;
      chromeDoc.getElementById("webRTC-selectMicrophone").hidden = !audioDevices.length || sharingAudio;

      let camMenupopup = chromeDoc.getElementById("webRTC-selectCamera-menupopup");
      let windowMenupopup = chromeDoc.getElementById("webRTC-selectWindow-menupopup");
      let micMenupopup = chromeDoc.getElementById("webRTC-selectMicrophone-menupopup");
      if (sharingScreen)
        listScreenShareDevices(windowMenupopup, videoDevices);
      else
        listDevices(camMenupopup, videoDevices);

      if (!sharingAudio)
        listDevices(micMenupopup, audioDevices);

      this.mainAction.callback = function(aState, aRemember) {
        let allowedDevices = [];
        let perms = Services.perms;
        if (videoDevices.length) {
          let listId = "webRTC-select" + (sharingScreen ? "Window" : "Camera") + "-menulist";
          let videoDeviceIndex = chromeDoc.getElementById(listId).value;
          let allowCamera = videoDeviceIndex != "-1";
          if (allowCamera) {
            allowedDevices.push(videoDeviceIndex);
            // Session permission will be removed after use
            // (it's really one-shot, not for the entire session)
            perms.add(uri, "MediaManagerVideo", perms.ALLOW_ACTION,
                      perms.EXPIRE_SESSION);
          }
          if (aRemember) {
            perms.add(uri, "camera",
                      allowCamera ? perms.ALLOW_ACTION : perms.DENY_ACTION);
          }
        }
        if (audioDevices.length) {
          if (!sharingAudio) {
            let audioDeviceIndex = chromeDoc.getElementById("webRTC-selectMicrophone-menulist").value;
            let allowMic = audioDeviceIndex != "-1";
            if (allowMic)
              allowedDevices.push(audioDeviceIndex);
            if (aRemember) {
              perms.add(uri, "microphone",
                        allowMic ? perms.ALLOW_ACTION : perms.DENY_ACTION);
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

        if (aRemember) {
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
    chromeWin.PopupNotifications.show(aBrowser, "webRTC-shareDevices", message,
                                      anchorId, mainAction, secondaryActions,
                                      options);
  notification.callID = aRequest.callID;
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

    _hiddenDoc: Cc["@mozilla.org/appshell/appShellService;1"]
                  .getService(Ci.nsIAppShellService)
                  .hiddenDOMWindow.document,
    _statusBar: Cc["@mozilla.org/widget/macsystemstatusbar;1"]
                  .getService(Ci.nsISystemStatusBar),

    _command: function(aEvent) {
      let type = this.getAttribute("type");
      if (type == "Camera" || type == "Microphone")
        type = "Devices";
      else if (type == "Window" || type == "Application" || type == "Browser")
        type = "Screen";
      webrtcUI.showSharingDoorhanger(aEvent.target.stream, type);
    },

    _popupShowing: function(aEvent) {
      let type = this.getAttribute("type");
      let activeStreams;
      if (type == "Camera") {
        activeStreams = webrtcUI.getActiveStreams(true, false, false);
      }
      else if (type == "Microphone") {
        activeStreams = webrtcUI.getActiveStreams(false, true, false);
      }
      else if (type == "Screen") {
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
        menuitem.setAttribute("type", type);
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
        let labelId = "webrtcIndicator.controlSharingOn.menuitem";
        let label = stream.browser.contentTitle || stream.uri;
        item.setAttribute("label", bundle.formatStringFromName(labelId, [label], 1));
        item.setAttribute("type", type);
        item.stream = stream;
        item.addEventListener("command", indicator._command);
        this.appendChild(item);
      }

      return true;
    },

    _popupHiding: function(aEvent) {
      while (this.firstChild)
        this.firstChild.remove();
    },

    _setIndicatorState: function(aName, aState) {
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
      }
      else if (this[field] && !aState) {
        this._statusBar.removeItem(this[field]);
        this[field].remove();
        this[field] = null
      }
    },
    updateIndicatorState: function() {
      this._setIndicatorState("Camera", webrtcUI.showCameraIndicator);
      this._setIndicatorState("Microphone", webrtcUI.showMicrophoneIndicator);
      this._setIndicatorState("Screen", webrtcUI.showScreenSharingIndicator);
    },
    close: function() {
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

    // We can only open 1 doorhanger at a time. Guessing that users would be
    // most eager to control screen/window/app sharing, and only then
    // camera/microphone sharing, in that (decreasing) order of priority.
    let doorhangerType;
    if ((/Screen|Window|Application/).test(stringName)) {
      doorhangerType = "Screen";
    } else {
      doorhangerType = "Devices";
    }
    menuitem.setAttribute("doorhangertype", doorhangerType);
    menuitem.addEventListener("command", onTabSharingMenuPopupCommand);
    e.target.appendChild(menuitem);
  }
}

function onTabSharingMenuPopupHiding(e) {
  while (this.lastChild)
    this.lastChild.remove();
}

function onTabSharingMenuPopupCommand(e) {
  let type = e.target.getAttribute("doorhangertype");
  webrtcUI.showSharingDoorhanger(e.target.stream, type);
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
    if (!gIndicatorWindow)
      gIndicatorWindow = getGlobalIndicator();
    else
      gIndicatorWindow.updateIndicatorState();
  } else if (gIndicatorWindow) {
    gIndicatorWindow.close();
    gIndicatorWindow = null;
  }
}

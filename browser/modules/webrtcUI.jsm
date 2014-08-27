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

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "MediaManagerService",
                                   "@mozilla.org/mediaManagerService;1",
                                   "nsIMediaManagerService");

this.webrtcUI = {
  init: function () {
    Services.obs.addObserver(handleRequest, "getUserMedia:request", false);
    Services.obs.addObserver(updateIndicators, "recording-device-events", false);
    Services.obs.addObserver(removeBrowserSpecificIndicator, "recording-window-ended", false);
    Services.obs.addObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished", false);
  },

  uninit: function () {
    Services.obs.removeObserver(handleRequest, "getUserMedia:request");
    Services.obs.removeObserver(updateIndicators, "recording-device-events");
    Services.obs.removeObserver(removeBrowserSpecificIndicator, "recording-window-ended");
    Services.obs.removeObserver(maybeAddMenuIndicator, "browser-delayed-startup-finished");
  },

  showGlobalIndicator: false,
  showCameraIndicator: false,
  showMicrophoneIndicator: false,
  showScreenSharingIndicator: "", // either "Application", "Screen" or "Window"

  // The boolean parameters indicate which streams should be included in the result.
  getActiveStreams: function(aCamera, aMicrophone, aScreen) {
    let contentWindowSupportsArray = MediaManagerService.activeMediaCaptureWindows;
    let count = contentWindowSupportsArray.Count();
    let activeStreams = [];
    for (let i = 0; i < count; i++) {
      let contentWindow = contentWindowSupportsArray.GetElementAt(i);

      let info = {
        Camera: {},
        Microphone: {},
        Window: {},
        Screen: {},
        Application: {}
      };
      MediaManagerService.mediaCaptureWindowState(contentWindow, info.Camera,
                                                  info.Microphone, info.Screen,
                                                  info.Window, info.Application);
      if (!(aCamera && info.Camera.value ||
            aMicrophone && info.Microphone.value ||
            aScreen && (info.Screen.value || info.Window.value ||
                        info.Application.value)))
        continue;

      let browser = getBrowserForWindow(contentWindow);
      let browserWindow = browser.ownerDocument.defaultView;
      let tab = browserWindow.gBrowser &&
                browserWindow.gBrowser._getTabForContentWindow(contentWindow.top);
      activeStreams.push({
        uri: contentWindow.location.href,
        tab: tab,
        browser: browser,
        types: info
      });
    }
    return activeStreams;
  },

  showSharingDoorhanger: function(aActiveStream, aType) {
    let browserWindow = aActiveStream.browser.ownerDocument.defaultView;
    if (aActiveStream.tab) {
      browserWindow.gBrowser.selectedTab = aActiveStream.tab;
    } else {
      aActiveStream.browser.focus();
    }
    browserWindow.focus();
    let PopupNotifications = browserWindow.PopupNotifications;
    let notif = PopupNotifications.getNotification("webRTC-sharing" + aType,
                                                   aActiveStream.browser);
#ifdef XP_MACOSX
    if (!Services.focus.activeWindow) {
      browserWindow.addEventListener("activate", function onActivate() {
        browserWindow.removeEventListener("activate", onActivate);
        Services.tm.mainThread.dispatch(function() {
          notif.reshow();
        }, Ci.nsIThread.DISPATCH_NORMAL);
      });
      Cc["@mozilla.org/widget/macdocksupport;1"].getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
      return;
    }
#endif
    notif.reshow();
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
  let constraints = aSubject.getConstraints();
  let secure = aSubject.isSecure;
  let contentWindow = Services.wm.getOuterWindowWithId(aSubject.windowID);

  contentWindow.navigator.mozGetUserMediaDevices(
    constraints,
    function (devices) {
      prompt(contentWindow, aSubject.callID, constraints.audio,
             constraints.video || constraints.picture, devices, secure);
    },
    function (error) {
      // bug 827146 -- In the future, the UI should catch NO_DEVICES_FOUND
      // and allow the user to plug in a device, instead of immediately failing.
      denyRequest(aSubject.callID, error);
    },
    aSubject.innerWindowID);
}

function denyRequest(aCallID, aError) {
  let msg = null;
  if (aError) {
    msg = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    msg.data = aError;
  }
  Services.obs.notifyObservers(msg, "getUserMedia:response:deny", aCallID);
}

function prompt(aContentWindow, aCallID, aAudio, aVideo, aDevices, aSecure) {
  let audioDevices = [];
  let videoDevices = [];

  // MediaStreamConstraints defines video as 'boolean or MediaTrackConstraints'.
  let sharingScreen = aVideo && typeof(aVideo) != "boolean" &&
                      aVideo.mediaSource != "camera";
  for (let device of aDevices) {
    device = device.QueryInterface(Ci.nsIMediaDevice);
    switch (device.type) {
      case "audio":
        if (aAudio)
          audioDevices.push(device);
        break;
      case "video":
        // Verify that if we got a camera, we haven't requested a screen share,
        // or that if we requested a screen share we aren't getting a camera.
        if (aVideo && (device.mediaSource == "camera") != sharingScreen)
          videoDevices.push(device);
        break;
    }
  }

  let requestTypes = [];
  if (videoDevices.length)
    requestTypes.push(sharingScreen ? "Screen" : "Camera");
  if (audioDevices.length)
    requestTypes.push("Microphone");

  if (!requestTypes.length) {
    denyRequest(aCallID, "NO_DEVICES_FOUND");
    return;
  }

  let uri = aContentWindow.document.documentURIObject;
  let browser = getBrowserForWindow(aContentWindow);
  let chromeDoc = browser.ownerDocument;
  let chromeWin = chromeDoc.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;
  let stringId = "getUserMedia.share" + requestTypes.join("And") + ".message";
  let message = stringBundle.getFormattedString(stringId, [uri.host]);

  let mainLabel;
  if (sharingScreen) {
    mainLabel = stringBundle.getString("getUserMedia.shareSelectedItems.label");
  }
  else {
    let string = stringBundle.getString("getUserMedia.shareSelectedDevices.label");
    mainLabel = PluralForm.get(requestTypes.length, string);
  }
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
        denyRequest(aCallID);
      }
    }
  ];

  if (!sharingScreen) { // Bug 1037438: implement 'never' for screen sharing.
    secondaryActions.push({
      label: stringBundle.getString("getUserMedia.never.label"),
      accessKey: stringBundle.getString("getUserMedia.never.accesskey"),
      callback: function () {
        denyRequest(aCallID);
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

  if (aSecure && !sharingScreen) {
    // Don't show the 'Always' action if the connection isn't secure, or for
    // screen sharing (because we can't guess which window the user wants to
    // share without prompting).
    secondaryActions.unshift({
      label: stringBundle.getString("getUserMedia.always.label"),
      accessKey: stringBundle.getString("getUserMedia.always.accesskey"),
      callback: function () {
        mainAction.callback(true);
      }
    });
  }

  let options = {
    eventCallback: function(aTopic, aNewBrowser) {
      if (aTopic == "swapping")
        return true;

      let chromeDoc = this.browser.ownerDocument;

      if (aTopic == "shown") {
        let PopupNotifications = chromeDoc.defaultView.PopupNotifications;
        let popupId = "Devices";
        if (requestTypes.length == 1 && requestTypes[0] == "Microphone")
          popupId = "Microphone";
        if (requestTypes.indexOf("Screen") != -1)
          popupId = "Screen";
        PopupNotifications.panel.firstChild.setAttribute("popupid", "webRTC-share" + popupId);
      }

      if (aTopic != "showing")
        return false;

      // DENY_ACTION is handled immediately by MediaManager, but handling
      // of ALLOW_ACTION is delayed until the popupshowing event
      // to avoid granting permissions automatically to background tabs.
      if (aSecure) {
        let perms = Services.perms;

        let micPerm = perms.testExactPermission(uri, "microphone");
        if (micPerm == perms.PROMPT_ACTION)
          micPerm = perms.UNKNOWN_ACTION;

        let camPerm = perms.testExactPermission(uri, "camera");
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
          let allowedDevices = Cc["@mozilla.org/supports-array;1"]
                                 .createInstance(Ci.nsISupportsArray);
          if (videoDevices.length && camPerm == perms.ALLOW_ACTION)
            allowedDevices.AppendElement(videoDevices[0]);
          if (audioDevices.length && micPerm == perms.ALLOW_ACTION)
            allowedDevices.AppendElement(audioDevices[0]);
          Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
          this.remove();
          return true;
        }
      }

      function listDevices(menupopup, devices) {
        while (menupopup.lastChild)
          menupopup.removeChild(menupopup.lastChild);

        let deviceIndex = 0;
        for (let device of devices) {
          addDeviceToList(menupopup, device.name, deviceIndex);
          deviceIndex++;
        }
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
        for (let i = 0; i < devices.length; ++i) {
          let name;
          // Screen has a special treatment because we currently only support
          // sharing the primary screen and want to display a localized string.
          if (type == "screen") {
            name = stringBundle.getString("getUserMedia.shareEntireScreen.label");
          }
          else {
            name = devices[i].name;
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
          addDeviceToList(menupopup, name, i, typeName);
        }

        // Always re-select the "No <type>" item.
        chromeDoc.getElementById("webRTC-selectWindow-menulist").removeAttribute("value");
        chromeDoc.getElementById("webRTC-all-windows-shared").hidden = true;
      }

      function addDeviceToList(menupopup, deviceName, deviceIndex, type) {
        let menuitem = chromeDoc.createElement("menuitem");
        menuitem.setAttribute("value", deviceIndex);
        menuitem.setAttribute("label", deviceName);
        menuitem.setAttribute("tooltiptext", deviceName);
        if (type)
          menuitem.setAttribute("devicetype", type);
        menupopup.appendChild(menuitem);
      }

      chromeDoc.getElementById("webRTC-selectCamera").hidden = !videoDevices.length || sharingScreen;
      chromeDoc.getElementById("webRTC-selectWindowOrScreen").hidden = !sharingScreen || !videoDevices.length;
      chromeDoc.getElementById("webRTC-selectMicrophone").hidden = !audioDevices.length;

      let camMenupopup = chromeDoc.getElementById("webRTC-selectCamera-menupopup");
      let windowMenupopup = chromeDoc.getElementById("webRTC-selectWindow-menupopup");
      let micMenupopup = chromeDoc.getElementById("webRTC-selectMicrophone-menupopup");
      if (sharingScreen)
        listScreenShareDevices(windowMenupopup, videoDevices);
      else
        listDevices(camMenupopup, videoDevices);
      listDevices(micMenupopup, audioDevices);
      if (requestTypes.length == 2) {
        let stringBundle = chromeDoc.defaultView.gNavigatorBundle;
        if (!sharingScreen)
          addDeviceToList(camMenupopup, stringBundle.getString("getUserMedia.noVideo.label"), "-1");
        addDeviceToList(micMenupopup, stringBundle.getString("getUserMedia.noAudio.label"), "-1");
      }

      this.mainAction.callback = function(aRemember) {
        let allowedDevices = Cc["@mozilla.org/supports-array;1"]
                               .createInstance(Ci.nsISupportsArray);
        let perms = Services.perms;
        if (videoDevices.length) {
          let listId = "webRTC-select" + (sharingScreen ? "Window" : "Camera") + "-menulist";
          let videoDeviceIndex = chromeDoc.getElementById(listId).value;
          let allowCamera = videoDeviceIndex != "-1";
          if (allowCamera)
            allowedDevices.AppendElement(videoDevices[videoDeviceIndex]);
          if (aRemember) {
            perms.add(uri, "camera",
                      allowCamera ? perms.ALLOW_ACTION : perms.DENY_ACTION);
          }
        }
        if (audioDevices.length) {
          let audioDeviceIndex = chromeDoc.getElementById("webRTC-selectMicrophone-menulist").value;
          let allowMic = audioDeviceIndex != "-1";
          if (allowMic)
            allowedDevices.AppendElement(audioDevices[audioDeviceIndex]);
          if (aRemember) {
            perms.add(uri, "microphone",
                      allowMic ? perms.ALLOW_ACTION : perms.DENY_ACTION);
          }
        }

        if (allowedDevices.Count() == 0) {
          denyRequest(aCallID);
          return;
        }

        Services.obs.notifyObservers(allowedDevices, "getUserMedia:response:allow", aCallID);
      };
      return false;
    }
  };

  let anchorId = "webRTC-shareDevices-notification-icon";
  if (requestTypes.length == 1 && requestTypes[0] == "Microphone")
    anchorId = "webRTC-shareMicrophone-notification-icon";
  if (requestTypes.indexOf("Screen") != -1)
    anchorId = "webRTC-shareScreen-notification-icon";
  chromeWin.PopupNotifications.show(browser, "webRTC-shareDevices", message,
                                    anchorId, mainAction, secondaryActions, options);
}

function getGlobalIndicator() {
#ifndef XP_MACOSX
  const INDICATOR_CHROME_URI = "chrome://browser/content/webrtcIndicator.xul";
  const features = "chrome,dialog=yes,titlebar=no,popup=yes";

  return Services.ww.openWindow(null, INDICATOR_CHROME_URI, "_blank", features, []);
#else
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
      else if (type == "Window" || type == "Application")
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
#endif
}

function onTabSharingMenuPopupShowing(e) {
  let streams = webrtcUI.getActiveStreams(true, true, true);
  for (let streamInfo of streams) {
    let stringName = "getUserMedia.sharingMenu";
    let types = streamInfo.types;
    if (types.Camera.value)
      stringName += "Camera";
    if (types.Microphone.value)
      stringName += "Microphone";
    if (types.Screen.value)
      stringName += "Screen";
    else if (types.Application.value)
      stringName += "Application";
    else if (types.Window.value)
      stringName += "Window";

    let doc = e.target.ownerDocument;
    let bundle = doc.defaultView.gNavigatorBundle;

    let origin;
    let uri;
    let href = streamInfo.uri;
    try {
      uri = Services.io.newURI(href, null, null);
      origin = uri.asciiHost;
    } catch (ex) {};
    if (!origin) {
      if (uri && uri.scheme == "about") {
        // For about URIs, just use the full spec, without any #hash parts
        origin = uri.specIgnoringRef;
      } else {
        // This is unfortunate, but we should display *something*...
        origin = bundle.getString("getUserMedia.sharingMenuUnknownHost");
      }
    }

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
#ifdef XP_MACOSX
    let container = document.getElementById("windowPopup");
    let insertionPoint = document.getElementById("sep-window-list");
    let separator = document.createElement("menuseparator");
    separator.id = "tabSharingSeparator";
    container.insertBefore(separator, insertionPoint);
#else
    let accesskeyStringId = "getUserMedia.sharingMenu.accesskey";
    menu.setAttribute("accesskey", stringBundle.getString(accesskeyStringId));
    let container = document.getElementById("main-menubar");
    let insertionPoint = document.getElementById("helpMenu");
#endif
    let popup = document.createElement("menupopup");
    popup.id = "tabSharingMenuPopup";
    popup.addEventListener("popupshowing", onTabSharingMenuPopupShowing);
    popup.addEventListener("popuphiding", onTabSharingMenuPopupHiding);
    menu.appendChild(popup);
    container.insertBefore(menu, insertionPoint);
  } else {
    menu.hidden = false;
#ifdef XP_MACOSX
    document.getElementById("tabSharingSeparator").hidden = false;
#endif
  }
}

function maybeAddMenuIndicator(window) {
  if (webrtcUI.showGlobalIndicator) {
    showOrCreateMenuForWindow(window);
  }
}

var gIndicatorWindow = null;

function updateIndicators() {
  let contentWindowSupportsArray = MediaManagerService.activeMediaCaptureWindows;
  let count = contentWindowSupportsArray.Count();

  webrtcUI.showGlobalIndicator = count > 0;
  webrtcUI.showCameraIndicator = false;
  webrtcUI.showMicrophoneIndicator = false;
  webrtcUI.showScreenSharingIndicator = "";

  for (let i = 0; i < count; ++i) {
    let contentWindow = contentWindowSupportsArray.GetElementAt(i);
    let camera = {}, microphone = {}, screen = {}, window = {}, app = {};
    MediaManagerService.mediaCaptureWindowState(contentWindow, camera,
                                                microphone, screen, window, app);
    if (camera.value)
      webrtcUI.showCameraIndicator = true;
    if (microphone.value)
      webrtcUI.showMicrophoneIndicator = true;
    if (screen.value)
      webrtcUI.showScreenSharingIndicator = "Screen";
    else if (window.value && webrtcUI.showScreenSharingIndicator != "Screen")
      webrtcUI.showScreenSharingIndicator = "Window";
    else if (app.value && !webrtcUI.showScreenSharingIndicator)
      webrtcUI.showScreenSharingIndicator = "Application";

    updateBrowserSpecificIndicator(getBrowserForWindow(contentWindow));
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
#ifdef XP_MACOSX
      let separator = doc.getElementById("tabSharingSeparator");
      if (separator) {
        separator.hidden = true;
      }
#endif
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

function updateBrowserSpecificIndicator(aBrowser) {
  let camera = {}, microphone = {}, screen = {}, window = {}, app = {};
  MediaManagerService.mediaCaptureWindowState(aBrowser.contentWindow,
                                              camera, microphone, screen,
                                              window, app);
  let captureState;
  if (camera.value && microphone.value) {
    captureState = "CameraAndMicrophone";
  } else if (camera.value) {
    captureState = "Camera";
  } else if (microphone.value) {
    captureState = "Microphone";
  }

  let chromeWin = aBrowser.ownerDocument.defaultView;
  let stringBundle = chromeWin.gNavigatorBundle;

  let uri = aBrowser.contentWindow.document.documentURIObject;
  let windowId = aBrowser.contentWindow
                         .QueryInterface(Ci.nsIInterfaceRequestor)
                         .getInterface(Ci.nsIDOMWindowUtils)
                         .currentInnerWindowID;
  let mainAction = {
    label: stringBundle.getString("getUserMedia.continueSharing.label"),
    accessKey: stringBundle.getString("getUserMedia.continueSharing.accesskey"),
    callback: function () {},
    dismiss: true
  };
  let secondaryActions = [{
    label: stringBundle.getString("getUserMedia.stopSharing.label"),
    accessKey: stringBundle.getString("getUserMedia.stopSharing.accesskey"),
    callback: function () {
      let perms = Services.perms;
      if (camera.value &&
          perms.testExactPermission(uri, "camera") == perms.ALLOW_ACTION)
        perms.remove(uri.host, "camera");
      if (microphone.value &&
          perms.testExactPermission(uri, "microphone") == perms.ALLOW_ACTION)
        perms.remove(uri.host, "microphone");

      Services.obs.notifyObservers(null, "getUserMedia:revoke", windowId);
    }
  }];
  let options = {
    hideNotNow: true,
    dismissed: true,
    eventCallback: function(aTopic) {
      if (aTopic == "shown") {
        let PopupNotifications = this.browser.ownerDocument.defaultView.PopupNotifications;
        let popupId = captureState == "Microphone" ? "Microphone" : "Devices";
        PopupNotifications.panel.firstChild.setAttribute("popupid", "webRTC-sharing" + popupId);
      }
      return aTopic == "swapping";
    }
  };
  if (captureState) {
    let anchorId = captureState == "Microphone" ? "webRTC-sharingMicrophone-notification-icon"
                                                : "webRTC-sharingDevices-notification-icon";
    let message = stringBundle.getString("getUserMedia.sharing" + captureState + ".message2");
    chromeWin.PopupNotifications.show(aBrowser, "webRTC-sharingDevices", message,
                                      anchorId, mainAction, secondaryActions, options);
  }
  else {
    removeBrowserNotification(aBrowser,"webRTC-sharingDevices");
  }

  // Now handle the screen sharing indicator.
  if (!screen.value && !window.value && !app.value) {
    removeBrowserNotification(aBrowser,"webRTC-sharingScreen");
    return;
  }

  options = {
    hideNotNow: true,
    dismissed: true,
    eventCallback: function(aTopic) {
      if (aTopic == "shown") {
        let PopupNotifications = this.browser.ownerDocument.defaultView.PopupNotifications;
        PopupNotifications.panel.firstChild.setAttribute("popupid", "webRTC-sharingScreen");
      }
      return aTopic == "swapping";
    }
  };
  secondaryActions = [{
    label: stringBundle.getString("getUserMedia.stopSharing.label"),
    accessKey: stringBundle.getString("getUserMedia.stopSharing.accesskey"),
    callback: function () {
      Services.obs.notifyObservers(null, "getUserMedia:revoke", "screen:" + windowId);
    }
  }];
  // If we are sharing both a window and the screen, show 'Screen'.
  let stringId = "getUserMedia.sharing";
  if (screen.value)
    stringId += "Screen";
  else if (app.value)
    stringId += "Application";
  else
    stringId += "Window";
  chromeWin.PopupNotifications.show(aBrowser, "webRTC-sharingScreen",
                                    stringBundle.getString(stringId + ".message"),
                                    "webRTC-sharingScreen-notification-icon",
                                    mainAction, secondaryActions, options);
}

function removeBrowserNotification(aBrowser, aNotificationId) {
  let win = aBrowser.ownerDocument.defaultView;
  let notification =
    win.PopupNotifications.getNotification(aNotificationId, aBrowser);
  if (notification)
    win.PopupNotifications.remove(notification);
}

function removeBrowserSpecificIndicator(aSubject, aTopic, aData) {
  let browser = getBrowserForWindowId(aData);
  // If the tab has already been closed, ignore the notification.
  if (browser.contentWindow)
    updateBrowserSpecificIndicator(browser);
}

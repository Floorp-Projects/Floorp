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
    Services.obs.addObserver(updateIndicators, "recording-device-events", false);
    Services.obs.addObserver(removeBrowserSpecificIndicator, "recording-window-ended", false);
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "webrtc:StopSharing":
        Services.obs.notifyObservers(null, "getUserMedia:revoke", aMessage.data);
        break;
    }
  }
};

function updateIndicators() {
  let contentWindowSupportsArray = MediaManagerService.activeMediaCaptureWindows;
  let count = contentWindowSupportsArray.Count();

  let state = {
    showGlobalIndicator: count > 0,
    showCameraIndicator: false,
    showMicrophoneIndicator: false,
    showScreenSharingIndicator: ""
  };

  for (let i = 0; i < count; ++i) {
    let contentWindow = contentWindowSupportsArray.GetElementAt(i);
    let camera = {}, microphone = {}, screen = {}, window = {}, app = {};
    MediaManagerService.mediaCaptureWindowState(contentWindow, camera,
                                                microphone, screen, window, app);
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

    tabState.windowId = getInnerWindowIDForWindow(contentWindow);
    tabState.documentURI = contentWindow.document.documentURI;
    let mm = getMessageManagerForWindow(contentWindow);
    mm.sendAsyncMessage("webrtc:UpdateBrowserIndicators", tabState);
  }

  let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
               .getService(Ci.nsIMessageSender);
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

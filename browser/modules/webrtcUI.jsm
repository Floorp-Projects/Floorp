/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["webrtcUI"];

const { EventEmitter } = ChromeUtils.import(
  "resource:///modules/syncedtabs/EventEmitter.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "XPCOMUtils",
  "resource://gre/modules/XPCOMUtils.jsm"
);

var webrtcUI = {
  initialized: false,

  peerConnectionBlockers: new Set(),
  emitter: new EventEmitter(),

  init() {
    if (!this.initialized) {
      Services.obs.addObserver(this, "browser-delayed-startup-finished");
      this.initialized = true;

      XPCOMUtils.defineLazyPreferenceGetter(
        this,
        "useLegacyGlobalIndicator",
        "privacy.webrtc.legacyGlobalIndicator",
        true
      );
    }
  },

  uninit() {
    if (this.initialized) {
      Services.obs.removeObserver(this, "browser-delayed-startup-finished");
      this.initialized = false;
    }
  },

  observe(subject, topic, data) {
    if (topic == "browser-delayed-startup-finished") {
      if (webrtcUI.showGlobalIndicator) {
        showOrCreateMenuForWindow(subject);
      }
    }
  },

  SHARING_NONE: 0,
  SHARING_WINDOW: 1,
  SHARING_SCREEN: 2,

  // Set of browser windows that are being shared over WebRTC.
  sharedWindows: new WeakSet(),
  sharingScreen: false,

  // Map of browser elements to indicator data.
  perTabIndicators: new Map(),
  activePerms: new Map(),

  get showGlobalIndicator() {
    for (let [, indicators] of this.perTabIndicators) {
      if (indicators.showGlobalIndicator) {
        return true;
      }
    }
    return false;
  },

  get showCameraIndicator() {
    for (let [, indicators] of this.perTabIndicators) {
      if (indicators.showCameraIndicator) {
        return true;
      }
    }
    return false;
  },

  get showMicrophoneIndicator() {
    for (let [, indicators] of this.perTabIndicators) {
      if (indicators.showMicrophoneIndicator) {
        return true;
      }
    }
    return false;
  },

  get showScreenSharingIndicator() {
    let list = [""];
    for (let [, indicators] of this.perTabIndicators) {
      if (indicators.showScreenSharingIndicator) {
        list.push(indicators.showScreenSharingIndicator);
      }
    }

    let precedence = ["Screen", "Window", "Application", "Browser", ""];

    list.sort((a, b) => {
      return precedence.indexOf(a) - precedence.indexOf(b);
    });

    return list[0];
  },

  _streams: [],
  // The boolean parameters indicate which streams should be included in the result.
  getActiveStreams(aCamera, aMicrophone, aScreen) {
    return webrtcUI._streams
      .filter(aStream => {
        let state = aStream.state;
        return (
          (aCamera && state.camera) ||
          (aMicrophone && state.microphone) ||
          (aScreen && state.screen)
        );
      })
      .map(aStream => {
        let state = aStream.state;
        let types = {
          camera: state.camera,
          microphone: state.microphone,
          screen: state.screen,
        };
        let browser = aStream.topBrowsingContext.embedderElement;
        let browserWindow = browser.ownerGlobal;
        let tab =
          browserWindow.gBrowser &&
          browserWindow.gBrowser.getTabForBrowser(browser);
        return { uri: state.documentURI, tab, browser, types };
      });
  },

  /**
   * Determine the combined state of all the active streams associated with
   * the specified top-level browsing context.
   */
  getCombinedStateForBrowser(aTopBrowsingContext) {
    function combine(x, y) {
      if (
        x == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
        y == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED
      ) {
        return Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;
      }
      if (
        x == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED ||
        y == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED
      ) {
        return Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED;
      }
      return Ci.nsIMediaManagerService.STATE_NOCAPTURE;
    }

    let camera, microphone, screen, window, browser;
    for (let stream of this._streams) {
      if (stream.topBrowsingContext == aTopBrowsingContext) {
        camera = combine(stream.state.camera, camera);
        microphone = combine(stream.state.microphone, microphone);
        screen = combine(stream.state.screen, screen);
        window = combine(stream.state.window, window);
        browser = combine(stream.state.browser, browser);
      }
    }

    let tabState = { camera, microphone };
    if (screen == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED) {
      tabState.screen = "Screen";
    } else if (window == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED) {
      tabState.screen = "Window";
    } else if (browser == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED) {
      tabState.screen = "Browser";
    } else if (screen == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED) {
      tabState.screen = "ScreenPaused";
    } else if (window == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED) {
      tabState.screen = "WindowPaused";
    } else if (browser == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED) {
      tabState.screen = "BrowserPaused";
    }

    let screenEnabled = tabState.screen && !tabState.screen.includes("Paused");
    let cameraEnabled =
      tabState.camera == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;
    let microphoneEnabled =
      tabState.microphone == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED;

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
    tabState.paused =
      tabState.sharing &&
      !screenEnabled &&
      !cameraEnabled &&
      !microphoneEnabled;

    if (
      tabState.camera == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
      tabState.camera == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED
    ) {
      tabState.showCameraIndicator = true;
    }
    if (
      tabState.microphone == Ci.nsIMediaManagerService.STATE_CAPTURE_ENABLED ||
      tabState.microphone == Ci.nsIMediaManagerService.STATE_CAPTURE_DISABLED
    ) {
      tabState.showMicrophoneIndicator = true;
    }

    tabState.showScreenSharingIndicator = "";
    if (tabState.screen) {
      if (tabState.screen.startsWith("Screen")) {
        tabState.showScreenSharingIndicator = "Screen";
      } else if (tabState.screen.startsWith("Window")) {
        if (tabState.showScreenSharingIndicator != "Screen") {
          tabState.showScreenSharingIndicator = "Window";
        }
      } else if (tabState.screen.startsWith("Browser")) {
        if (!tabState.showScreenSharingIndicator) {
          tabState.showScreenSharingIndicator = "Browser";
        }
      }
    }

    return tabState;
  },

  /*
   * Indicate that a stream has been added or removed from the given
   * browsing context. If it has been added, aData specifies the
   * specific indicator types it uses. If aData is null or has no
   * documentURI assigned, then the stream has been removed.
   */
  streamAddedOrRemoved(aBrowsingContext, aData) {
    this.init();

    let index;
    for (index = 0; index < webrtcUI._streams.length; ++index) {
      let stream = this._streams[index];
      if (stream.browsingContext == aBrowsingContext) {
        break;
      }
    }
    // If there's no documentURI, the update is actually a removal of the
    // stream, triggered by the recording-window-ended notification.
    if (!aData || !aData.documentURI) {
      if (index < this._streams.length) {
        this._streams.splice(index, 1);
      }
    } else if (aData) {
      this._streams[index] = {
        browsingContext: aBrowsingContext,
        topBrowsingContext: aBrowsingContext.top,
        state: aData,
      };
    }

    let sharedWindowRawDeviceIds = new Set();
    this.sharingScreen = false;
    let suppressNotifications = false;
    for (let stream of this._streams) {
      let { state } = stream;
      suppressNotifications |= state.suppressNotifications;

      for (let device of state.devices) {
        if (!device.scary) {
          continue;
        }

        let mediaSource = device.mediaSource;

        if (mediaSource == "window") {
          sharedWindowRawDeviceIds.add(device.rawId);
        } else if (mediaSource == "screen") {
          this.sharingScreen = true;
        }
      }
    }

    this.sharedWindows = new WeakSet();

    for (let win of BrowserWindowTracker.orderedWindows) {
      let rawDeviceId;
      try {
        rawDeviceId = win.windowUtils.webrtcRawDeviceId;
      } catch (e) {
        // This can theoretically throw if some of the underlying
        // window primitives don't exist. In that case, we can skip
        // to the next window.
        continue;
      }
      if (sharedWindowRawDeviceIds.has(rawDeviceId)) {
        this.sharedWindows.add(win);
      }
    }

    if (
      Services.prefs.getBoolPref(
        "privacy.webrtc.allowSilencingNotifications",
        false
      )
    ) {
      let alertsService = Cc["@mozilla.org/alerts-service;1"]
        .getService(Ci.nsIAlertsService)
        .QueryInterface(Ci.nsIAlertsDoNotDisturb);
      alertsService.suppressForScreenSharing = suppressNotifications;
    }
  },

  /**
   * Remove all the streams associated with a given
   * browsing context.
   */
  forgetStreamsFromBrowserContext(aBrowsingContext) {
    for (let index = 0; index < webrtcUI._streams.length; ) {
      let stream = this._streams[index];
      if (stream.browsingContext == aBrowsingContext) {
        this._streams.splice(index, 1);
      } else {
        index++;
      }
    }

    // Remove the per-tab indicator if it no longer needs to be displayed.
    let topBC = aBrowsingContext.top;
    if (this.perTabIndicators.has(topBC)) {
      let tabState = this.getCombinedStateForBrowser(topBC);
      if (
        !tabState.showCameraIndicator &&
        !tabState.showMicrophoneIndicator &&
        !tabState.showScreenSharingIndicator
      ) {
        this.perTabIndicators.delete(topBC);
      }
    }

    this.updateGlobalIndicator();
  },

  updateIndicators(aTopBrowsingContext) {
    let tabState = this.getCombinedStateForBrowser(aTopBrowsingContext);

    let indicators;
    if (this.perTabIndicators.has(aTopBrowsingContext)) {
      indicators = this.perTabIndicators.get(aTopBrowsingContext);
    } else {
      indicators = {};
      this.perTabIndicators.set(aTopBrowsingContext, indicators);
    }

    indicators.showGlobalIndicator = !!webrtcUI._streams.length;
    indicators.showCameraIndicator = tabState.showCameraIndicator;
    indicators.showMicrophoneIndicator = tabState.showMicrophoneIndicator;
    indicators.showScreenSharingIndicator = tabState.showScreenSharingIndicator;
    this.updateGlobalIndicator();

    return tabState;
  },

  swapBrowserForNotification(aOldBrowser, aNewBrowser) {
    for (let stream of this._streams) {
      if (stream.browser == aOldBrowser) {
        stream.browser = aNewBrowser;
      }
    }
  },

  forgetActivePermissionsFromBrowser(aBrowser) {
    this.activePerms.delete(aBrowser.outerWindowID);
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
      browserWindow.addEventListener(
        "activate",
        function() {
          Services.tm.dispatchToMainThread(function() {
            identityBox.click();
          });
        },
        { once: true }
      );
      Cc["@mozilla.org/widget/macdocksupport;1"]
        .getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
      return;
    }
    identityBox.click();
  },

  updateWarningLabel(aMenuList) {
    let type = aMenuList.selectedItem.getAttribute("devicetype");
    let document = aMenuList.ownerDocument;
    document.getElementById("webRTC-all-windows-shared").hidden =
      type != "screen";
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

  getHostOrExtensionName(uri, href) {
    let host;
    try {
      if (!uri) {
        uri = Services.io.newURI(href);
      }

      let addonPolicy = WebExtensionPolicy.getByURI(uri);
      host = addonPolicy ? addonPolicy.name : uri.host;
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
  },

  updateGlobalIndicator() {
    for (let chromeWin of Services.wm.getEnumerator("navigator:browser")) {
      if (this.showGlobalIndicator) {
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

    if (this.showGlobalIndicator) {
      if (!gIndicatorWindow) {
        gIndicatorWindow = getGlobalIndicator();
      } else {
        try {
          gIndicatorWindow.updateIndicatorState();
        } catch (err) {
          Cu.reportError(
            `error in gIndicatorWindow.updateIndicatorState(): ${err.message}`
          );
        }
      }
    } else if (gIndicatorWindow) {
      gIndicatorWindow.close();
      gIndicatorWindow = null;
    }
  },

  getWindowShareState(window) {
    if (this.sharingScreen) {
      return this.SHARING_SCREEN;
    } else if (this.sharedWindows.has(window)) {
      return this.SHARING_WINDOW;
    }
    return this.SHARING_NONE;
  },
};

function getGlobalIndicator() {
  if (!webrtcUI.useLegacyGlobalIndicator) {
    const INDICATOR_CHROME_URI =
      "chrome://browser/content/webrtcIndicator.xhtml";
    const features = "chrome,dialog=yes,titlebar=no,popup=yes";

    return Services.ww.openWindow(
      null,
      INDICATOR_CHROME_URI,
      "_blank",
      features,
      []
    );
  }

  if (AppConstants.platform != "macosx") {
    const LEGACY_INDICATOR_CHROME_URI =
      "chrome://browser/content/webrtcLegacyIndicator.xhtml";
    const features = "chrome,dialog=yes,titlebar=no,popup=yes";

    return Services.ww.openWindow(
      null,
      LEGACY_INDICATOR_CHROME_URI,
      "_blank",
      features,
      []
    );
  }

  let indicator = {
    _camera: null,
    _microphone: null,
    _screen: null,

    _hiddenDoc: Services.appShell.hiddenDOMWindow.document,
    _statusBar: Cc["@mozilla.org/widget/macsystemstatusbar;1"].getService(
      Ci.nsISystemStatusBar
    ),

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

      let bundle = Services.strings.createBundle(
        "chrome://browser/locale/webrtcIndicator.properties"
      );

      if (activeStreams.length == 1) {
        let stream = activeStreams[0];

        let menuitem = this.ownerDocument.createXULElement("menuitem");
        let labelId = "webrtcIndicator.sharing" + type + "With.menuitem";
        let label = stream.browser.contentTitle || stream.uri;
        menuitem.setAttribute(
          "label",
          bundle.formatStringFromName(labelId, [label])
        );
        menuitem.setAttribute("disabled", "true");
        this.appendChild(menuitem);

        menuitem = this.ownerDocument.createXULElement("menuitem");
        menuitem.setAttribute(
          "label",
          bundle.GetStringFromName("webrtcIndicator.controlSharing.menuitem")
        );
        menuitem.stream = stream;
        menuitem.addEventListener("command", indicator._command);

        this.appendChild(menuitem);
        return true;
      }

      // We show a different menu when there are several active streams.
      let menuitem = this.ownerDocument.createXULElement("menuitem");
      let labelId = "webrtcIndicator.sharing" + type + "WithNTabs.menuitem";
      let count = activeStreams.length;
      let label = PluralForm.get(
        count,
        bundle.GetStringFromName(labelId)
      ).replace("#1", count);
      menuitem.setAttribute("label", label);
      menuitem.setAttribute("disabled", "true");
      this.appendChild(menuitem);

      for (let stream of activeStreams) {
        let item = this.ownerDocument.createXULElement("menuitem");
        labelId = "webrtcIndicator.controlSharingOn.menuitem";
        label = stream.browser.contentTitle || stream.uri;
        item.setAttribute(
          "label",
          bundle.formatStringFromName(labelId, [label])
        );
        item.stream = stream;
        item.addEventListener("command", indicator._command);
        this.appendChild(item);
      }

      return true;
    },

    _popupHiding(aEvent) {
      while (this.firstChild) {
        this.firstChild.remove();
      }
    },

    _setIndicatorState(aName, aState) {
      let field = "_" + aName.toLowerCase();
      if (aState && !this[field]) {
        let menu = this._hiddenDoc.createXULElement("menu");
        menu.setAttribute("id", "webRTC-sharing" + aName + "-menu");

        // The CSS will only be applied if the menu is actually inserted in the DOM.
        this._hiddenDoc.documentElement.appendChild(menu);

        this._statusBar.addItem(menu);

        let menupopup = this._hiddenDoc.createXULElement("menupopup");
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
    },
  };

  indicator.updateIndicatorState();
  return indicator;
}

function onTabSharingMenuPopupShowing(e) {
  let streams = webrtcUI.getActiveStreams(true, true, true);
  for (let streamInfo of streams) {
    let stringName = "getUserMedia.sharingMenu";
    let types = streamInfo.types;
    if (types.camera) {
      stringName += "Camera";
    }
    if (types.microphone) {
      stringName += "Microphone";
    }
    if (types.screen) {
      stringName += types.screen;
    }

    let doc = e.target.ownerDocument;
    let bundle = doc.defaultView.gNavigatorBundle;

    let origin = webrtcUI.getHostOrExtensionName(null, streamInfo.uri);
    let menuitem = doc.createXULElement("menuitem");
    menuitem.setAttribute(
      "label",
      bundle.getFormattedString(stringName, [origin])
    );
    menuitem.stream = streamInfo;
    menuitem.addEventListener("command", onTabSharingMenuPopupCommand);
    e.target.appendChild(menuitem);
  }
}

function onTabSharingMenuPopupHiding(e) {
  while (this.lastChild) {
    this.lastChild.remove();
  }
}

function onTabSharingMenuPopupCommand(e) {
  webrtcUI.showSharingDoorhanger(e.target.stream);
}

function showOrCreateMenuForWindow(aWindow) {
  let document = aWindow.document;
  let menu = document.getElementById("tabSharingMenu");
  if (!menu) {
    let stringBundle = aWindow.gNavigatorBundle;
    menu = document.createXULElement("menu");
    menu.id = "tabSharingMenu";
    let labelStringId = "getUserMedia.sharingMenu.label";
    menu.setAttribute("label", stringBundle.getString(labelStringId));

    let container, insertionPoint;
    if (AppConstants.platform == "macosx") {
      container = document.getElementById("windowPopup");
      insertionPoint = document.getElementById("sep-window-list");
      let separator = document.createXULElement("menuseparator");
      separator.id = "tabSharingSeparator";
      container.insertBefore(separator, insertionPoint);
    } else {
      let accesskeyStringId = "getUserMedia.sharingMenu.accesskey";
      menu.setAttribute("accesskey", stringBundle.getString(accesskeyStringId));
      container = document.getElementById("main-menubar");
      insertionPoint = document.getElementById("helpMenu");
    }
    let popup = document.createXULElement("menupopup");
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

var gIndicatorWindow = null;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["webrtcUI", "MacOSWebRTCStatusbarIndicator"];

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
ChromeUtils.defineModuleGetter(
  this,
  "SitePermissions",
  "resource:///modules/SitePermissions.jsm"
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

      Services.telemetry.setEventRecordingEnabled("webrtc.ui", true);
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
  sharedBrowserWindows: new WeakSet(),

  // True if one or more screens is being shared.
  sharingScreen: false,

  allowedSharedBrowsers: new WeakSet(),
  allowTabSwitchesForSession: false,
  tabSwitchCountForSession: 0,

  // True if a window or screen is being shared.
  sharingDisplay: false,

  // The session ID is used to try to differentiate between instances
  // where the user is sharing their display somehow. If the user
  // transitions from a state of not sharing their display, to sharing a
  // display, we bump the ID.
  sharingDisplaySessionId: 0,

  // Map of browser elements to indicator data.
  perTabIndicators: new Map(),
  activePerms: new Map(),

  get showGlobalIndicator() {
    for (let [, indicators] of this.perTabIndicators) {
      if (
        indicators.showCameraIndicator ||
        indicators.showMicrophoneIndicator ||
        indicators.showScreenSharingIndicator
      ) {
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
  getActiveStreams(aCamera, aMicrophone, aScreen, aWindow = false) {
    return webrtcUI._streams
      .filter(aStream => {
        let state = aStream.state;
        return (
          (aCamera && state.camera) ||
          (aMicrophone && state.microphone) ||
          (aScreen && state.screen) ||
          (aWindow && state.window)
        );
      })
      .map(aStream => {
        let state = aStream.state;
        let types = {
          camera: state.camera,
          microphone: state.microphone,
          screen: state.screen,
          window: state.window,
        };
        let browser = aStream.topBrowsingContext.embedderElement;
        let browserWindow = browser.ownerGlobal;
        let tab =
          browserWindow.gBrowser &&
          browserWindow.gBrowser.getTabForBrowser(browser);
        return {
          uri: state.documentURI,
          tab,
          browser,
          types,
          devices: state.devices,
        };
      });
  },

  /**
   * Returns true if aBrowser has an active WebRTC stream.
   */
  browserHasStreams(aBrowser) {
    for (let stream of this._streams) {
      if (stream.topBrowsingContext.embedderElement == aBrowser) {
        return true;
      }
    }

    return false;
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
    // The update is a removal of the stream, triggered by the
    // recording-window-ended notification.
    if (aData.remove) {
      if (index < this._streams.length) {
        this._streams.splice(index, 1);
      }
    } else {
      this._streams[index] = {
        browsingContext: aBrowsingContext,
        topBrowsingContext: aBrowsingContext.top,
        state: aData,
      };
    }

    let wasSharingDisplay = this.sharingDisplay;

    // Reset our internal notion of whether or not we're sharing
    // a screen or browser window. Now we'll go through the shared
    // devices and re-determine what's being shared.
    let sharingBrowserWindow = false;
    let sharedWindowRawDeviceIds = new Set();
    this.sharingDisplay = false;
    this.sharingScreen = false;
    let suppressNotifications = false;

    // First, go through the streams and collect the counts on things
    // like the total number of shared windows, and whether or not we're
    // sharing screens.
    for (let stream of this._streams) {
      let { state } = stream;
      suppressNotifications |= state.suppressNotifications;

      for (let device of state.devices) {
        let mediaSource = device.mediaSource;

        if (mediaSource == "window" || mediaSource == "screen") {
          this.sharingDisplay = true;
        }

        if (!device.scary) {
          continue;
        }

        if (mediaSource == "window") {
          sharedWindowRawDeviceIds.add(device.rawId);
        } else if (mediaSource == "screen") {
          this.sharingScreen = true;
        }

        // If the user has granted a particular site the ability
        // to get a stream from a window or screen, we will
        // presume that it's exempt from the tab switch warning.
        //
        // We use the permanentKey here so that the allowing of
        // the tab survives tab tear-in and tear-out. We ignore
        // browsers that don't have permanentKey, since those aren't
        // tabbrowser browsers.
        let browser = stream.topBrowsingContext.embedderElement;
        if (browser.permanentKey) {
          this.allowedSharedBrowsers.add(browser.permanentKey);
        }
      }
    }

    // Next, go through the list of shared windows, and map them
    // to our browser windows so that we know which ones are shared.
    this.sharedBrowserWindows = new WeakSet();

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
        this.sharedBrowserWindows.add(win);

        // If we've shared a window, then the initially selected tab
        // in that window should be exempt from tab switch warnings,
        // since it's already been shared.
        let selectedBrowser = win.gBrowser.selectedBrowser;
        this.allowedSharedBrowsers.add(selectedBrowser.permanentKey);

        sharingBrowserWindow = true;
      }
    }

    // If we weren't sharing a window or screen, and now are, bump
    // the sharingDisplaySessionId. We use this ID for Event
    // telemetry, and consider a transition from no shared displays
    // to some shared displays as a new session.
    if (!wasSharingDisplay && this.sharingDisplay) {
      this.sharingDisplaySessionId++;
    }

    // If we were adding a new display stream, record some Telemetry for
    // it with the most recent sharedDisplaySessionId. We do this separately
    // from the loops above because those take into account the pre-existing
    // streams that might already have been shared.
    if (aData.devices) {
      // The mixture of camelCase with under_score notation here is due to
      // an unfortunate collision of conventions between this file and
      // Event Telemetry.
      let silence_notifs = suppressNotifications ? "true" : "false";
      for (let device of aData.devices) {
        if (device.mediaSource == "screen") {
          this.recordEvent("share_display", "screen", {
            silence_notifs,
          });
        } else if (device.mediaSource == "window") {
          if (device.scary) {
            this.recordEvent("share_display", "browser_window", {
              silence_notifs,
            });
          } else {
            this.recordEvent("share_display", "window", {
              silence_notifs,
            });
          }
        }
      }
    }

    // Since we're not sharing a screen or browser window,
    // we can clear these state variables, which are used
    // to warn users on tab switching when sharing. These
    // are safe to reset even if we hadn't been sharing
    // the screen or browser window already.
    if (!this.sharingScreen && !sharingBrowserWindow) {
      this.allowedSharedBrowsers = new WeakSet();
      this.allowTabSwitchesForSession = false;
      this.tabSwitchCountForSession = 0;
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

  /**
   * Given some set of streams, stops device access for those streams.
   * Optionally, it's possible to stop a subset of the devices on those
   * streams by passing in optional arguments.
   *
   * Once the streams have been stopped, this method will also find the
   * newest stream's <xul:browser> and window, focus the window, and
   * select the browser.
   *
   * For camera and microphone streams, this will also revoke any associated
   * permissions from SitePermissions.
   *
   * @param {Array<Object>} activeStreams - An array of streams obtained via webrtcUI.getActiveStreams.
   * @param {boolean} stopCameras - True to stop the camera streams (defaults to true)
   * @param {boolean} stopMics - True to stop the microphone streams (defaults to true)
   * @param {boolean} stopScreens - True to stop the screen streams (defaults to true)
   * @param {boolean} stopWindows - True to stop the window streams (defaults to true)
   */
  stopSharingStreams(
    activeStreams,
    stopCameras = true,
    stopMics = true,
    stopScreens = true,
    stopWindows = true
  ) {
    if (!activeStreams.length) {
      return;
    }

    let ids = [];
    if (stopCameras) {
      ids.push("camera");
    }
    if (stopMics) {
      ids.push("microphone");
    }
    if (stopScreens || stopWindows) {
      ids.push("screen");
    }

    for (let stream of activeStreams) {
      let { browser } = stream;

      let gBrowser = browser.getTabBrowser();
      if (!gBrowser) {
        Cu.reportError("Can't stop sharing stream - cannot find gBrowser.");
        continue;
      }

      let tab = gBrowser.getTabForBrowser(browser);
      if (!tab) {
        Cu.reportError("Can't stop sharing stream - cannot find tab.");
        continue;
      }

      this.clearPermissionsAndStopSharing(ids, tab);
    }

    // Switch to the newest stream's browser.
    let mostRecentStream = activeStreams[activeStreams.length - 1];
    let { browser: browserToSelect } = mostRecentStream;

    let window = browserToSelect.ownerGlobal;
    let gBrowser = browserToSelect.getTabBrowser();
    let tab = gBrowser.getTabForBrowser(browserToSelect);
    window.focus();
    gBrowser.selectedTab = tab;
  },

  /**
   * Clears permissions and stops sharing (if active) for a list of device types
   * and a specific tab.
   * @param {("camera"|"microphone"|"screen")[]} types - Device types to stop
   * and clear permissions for.
   * @param tab - Tab of the devices to stop and clear permissions.
   */
  clearPermissionsAndStopSharing(types, tab) {
    let invalidTypes = types.filter(
      type => type != "camera" && type != "screen" && type != "microphone"
    );
    if (invalidTypes.length) {
      throw new Error(`Invalid device types ${invalidTypes.join(",")}`);
    }
    let browser = tab.linkedBrowser;
    let sharingState = tab._sharingState?.webRTC;

    // If we clear a WebRTC permission we need to remove all permissions of
    // the same type across device ids. We also need to stop active WebRTC
    // devices related to the permission.
    let perms = SitePermissions.getAllForBrowser(browser);

    let sharingCameraAndMic =
      sharingState?.camera &&
      sharingState?.microphone &&
      (types.includes("camera") || types.includes("microphone"));
    perms
      .filter(perm => {
        let [permId] = perm.id.split(SitePermissions.PERM_KEY_DELIMITER);
        // It's not possible to stop sharing one of camera/microphone
        // without the other.
        if (
          sharingCameraAndMic &&
          (permId == "camera" || permId == "microphone")
        ) {
          return true;
        }
        return types.includes(permId);
      })
      .forEach(perm => {
        SitePermissions.removeFromPrincipal(
          browser.contentPrincipal,
          perm.id,
          browser
        );
      });

    if (!sharingState?.windowId) {
      return;
    }

    // If the device of the permission we're clearing is currently active,
    // tell the WebRTC implementation to stop sharing it.
    let { windowId } = sharingState;

    let windowIds = [];
    if (types.includes("screen") && sharingState.screen) {
      windowIds.push(`screen:${windowId}`);
    }
    if (
      (types.includes("camera") && sharingState.camera) ||
      (types.includes("microphone") && sharingState.microphone)
    ) {
      windowIds.push(windowId);
    }

    if (!windowIds.length) {
      return;
    }

    let actor = sharingState.browsingContext.currentWindowGlobal.getActor(
      "WebRTC"
    );
    windowIds.forEach(id => actor.sendAsyncMessage("webrtc:StopSharing", id));
    webrtcUI.forgetActivePermissionsFromBrowser(browser);
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
    let permissionBox = browserWindow.document.getElementById(
      "identity-permission-box"
    );
    if (AppConstants.platform == "macosx" && !Services.focus.activeWindow) {
      browserWindow.addEventListener(
        "activate",
        function() {
          Services.tm.dispatchToMainThread(function() {
            permissionBox.click();
          });
        },
        { once: true }
      );
      Cc["@mozilla.org/widget/macdocksupport;1"]
        .getService(Ci.nsIMacDockSupport)
        .activateApplication(true);
      return;
    }
    permissionBox.click();
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
      if (
        !webrtcUI.useLegacyGlobalIndicator &&
        gIndicatorWindow.closingInternally
      ) {
        // Before calling .close(), we call .closingInternally() to allow us to
        // differentiate between situations where the indicator closes because
        // we no longer want to show the indicator (this case), and cases where
        // the user has found a way to close the indicator via OS window control
        // mechanisms.
        gIndicatorWindow.closingInternally();
      }
      gIndicatorWindow.close();
      gIndicatorWindow = null;
    }
  },

  getWindowShareState(window) {
    if (this.sharingScreen) {
      return this.SHARING_SCREEN;
    } else if (this.sharedBrowserWindows.has(window)) {
      return this.SHARING_WINDOW;
    }
    return this.SHARING_NONE;
  },

  tabAddedWhileSharing(tab) {
    this.allowedSharedBrowsers.add(tab.linkedBrowser.permanentKey);
  },

  shouldShowSharedTabWarning(tab) {
    if (!tab || !tab.linkedBrowser) {
      return false;
    }

    let browser = tab.linkedBrowser;
    // We want the user to be able to switch to one tab after starting
    // to share their window or screen. The presumption here is that
    // most users will have a single window with multiple tabs, where
    // the selected tab will be the one with the screen or window
    // sharing web application, and it's most likely that the contents
    // that the user wants to share are in another tab that they'll
    // switch to immediately upon sharing. These presumptions are based
    // on research that our user research team did with users using
    // video conferencing web applications.
    if (!this.tabSwitchCountForSession) {
      this.allowedSharedBrowsers.add(browser.permanentKey);
    }

    this.tabSwitchCountForSession++;
    let shouldShow =
      !this.allowTabSwitchesForSession &&
      !this.allowedSharedBrowsers.has(browser.permanentKey);

    return shouldShow;
  },

  allowSharedTabSwitch(tab, allowForSession) {
    let browser = tab.linkedBrowser;
    let gBrowser = browser.getTabBrowser();
    this.allowedSharedBrowsers.add(browser.permanentKey);
    gBrowser.selectedTab = tab;
    this.allowTabSwitchesForSession = allowForSession;
  },

  recordEvent(type, object, args = {}) {
    Services.telemetry.recordEvent(
      "webrtc.ui",
      type,
      object,
      this.sharingDisplaySessionId.toString(),
      args
    );
  },
};

function getGlobalIndicator() {
  webrtcUI.recordEvent("show_indicator", "show_indicator");

  if (!webrtcUI.useLegacyGlobalIndicator) {
    const INDICATOR_CHROME_URI =
      "chrome://browser/content/webrtcIndicator.xhtml";
    let features = "chrome,titlebar=no,alwaysontop,minimizable=yes";

    /* Don't use dialog on Gtk as it adds extra border and titlebar to indicator */
    if (!AppConstants.MOZ_WIDGET_GTK) {
      features += ",dialog=yes";
    }

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

  return new MacOSWebRTCStatusbarIndicator();
}

/**
 * Controls the visibility of screen, camera and microphone sharing indicators
 * in the macOS global menu bar. This class should only ever be instantiated
 * on macOS.
 *
 * The public methods on this class intentionally match the interface for the
 * WebRTC global sharing indicator, because the MacOSWebRTCStatusbarIndicator
 * acts as the indicator when in the legacy indicator configuration.
 */
class MacOSWebRTCStatusbarIndicator {
  constructor() {
    this._camera = null;
    this._microphone = null;
    this._screen = null;

    this._hiddenDoc = Services.appShell.hiddenDOMWindow.document;
    this._statusBar = Cc["@mozilla.org/widget/systemstatusbar;1"].getService(
      Ci.nsISystemStatusBar
    );

    this.updateIndicatorState();
  }

  /**
   * Public method that will determine the most appropriate
   * set of indicators to show, and then show them or hide
   * them as necessary.
   */
  updateIndicatorState() {
    this._setIndicatorState("Camera", webrtcUI.showCameraIndicator);
    this._setIndicatorState("Microphone", webrtcUI.showMicrophoneIndicator);
    this._setIndicatorState("Screen", webrtcUI.showScreenSharingIndicator);
  }

  /**
   * Public method that will hide all indicators.
   */
  close() {
    this._setIndicatorState("Camera", false);
    this._setIndicatorState("Microphone", false);
    this._setIndicatorState("Screen", false);
  }

  handleEvent(event) {
    switch (event.type) {
      case "popupshowing": {
        this._popupShowing(event);
        break;
      }
      case "popuphiding": {
        this._popupHiding(event);
        break;
      }
      case "command": {
        this._command(event);
        break;
      }
    }
  }

  /**
   * Handler for command events fired by the <menuitem> elements
   * inside any of the indicator <menu>'s.
   *
   * @param {Event} aEvent - The command event for the <menuitem>.
   */
  _command(aEvent) {
    webrtcUI.showSharingDoorhanger(aEvent.target.stream);
  }

  /**
   * Handler for the popupshowing event for one of the status
   * bar indicator menus.
   *
   * @param {Event} aEvent - The popupshowing event for the <menu>.
   */
  _popupShowing(aEvent) {
    let menu = aEvent.target;
    let type = menu.getAttribute("type");
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

      let menuitem = menu.ownerDocument.createXULElement("menuitem");
      let labelId = "webrtcIndicator.sharing" + type + "With.menuitem";
      let label = stream.browser.contentTitle || stream.uri;
      menuitem.setAttribute(
        "label",
        bundle.formatStringFromName(labelId, [label])
      );
      menuitem.setAttribute("disabled", "true");
      menu.appendChild(menuitem);

      menuitem = menu.ownerDocument.createXULElement("menuitem");
      menuitem.setAttribute(
        "label",
        bundle.GetStringFromName("webrtcIndicator.controlSharing.menuitem")
      );
      menuitem.stream = stream;
      menuitem.addEventListener("command", this);

      menu.appendChild(menuitem);
      return true;
    }

    // We show a different menu when there are several active streams.
    let menuitem = menu.ownerDocument.createXULElement("menuitem");
    let labelId = "webrtcIndicator.sharing" + type + "WithNTabs.menuitem";
    let count = activeStreams.length;
    let label = PluralForm.get(
      count,
      bundle.GetStringFromName(labelId)
    ).replace("#1", count);
    menuitem.setAttribute("label", label);
    menuitem.setAttribute("disabled", "true");
    menu.appendChild(menuitem);

    for (let stream of activeStreams) {
      let item = menu.ownerDocument.createXULElement("menuitem");
      labelId = "webrtcIndicator.controlSharingOn.menuitem";
      label = stream.browser.contentTitle || stream.uri;
      item.setAttribute("label", bundle.formatStringFromName(labelId, [label]));
      item.stream = stream;
      item.addEventListener("command", this);
      menu.appendChild(item);
    }

    return true;
  }

  /**
   * Handler for the popuphiding event for one of the status
   * bar indicator menus.
   *
   * @param {Event} aEvent - The popuphiding event for the <menu>.
   */
  _popupHiding(aEvent) {
    let menu = aEvent.target;
    while (menu.firstChild) {
      menu.firstChild.remove();
    }
  }

  /**
   * Updates the status bar to show or hide a screen, camera or
   * microphone indicator.
   *
   * @param {String} aName - One of the following: "screen", "camera",
   *   "microphone"
   * @param {boolean} aState - True to show the indicator for the aName
   *   type of stream, false ot hide it.
   */
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
      menupopup.addEventListener("popupshowing", this);
      menupopup.addEventListener("popuphiding", this);
      menupopup.addEventListener("command", this);
      menu.appendChild(menupopup);

      this[field] = menu;
    } else if (this[field] && !aState) {
      this._statusBar.removeItem(this[field]);
      this[field].remove();
      this[field] = null;
    }
  }
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

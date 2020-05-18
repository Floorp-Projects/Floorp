/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { webrtcUI } = ChromeUtils.import("resource:///modules/webrtcUI.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "SitePermissions",
  "resource:///modules/SitePermissions.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gScreenManager",
  "@mozilla.org/gfx/screenmanager;1",
  "nsIScreenManager"
);

/**
 * Public function called by webrtcUI to update the indicator
 * display when the active streams change.
 */
function updateIndicatorState() {
  WebRTCIndicator.updateIndicatorState();
}

/**
 * Main control object for the WebRTC global indicator
 */
const WebRTCIndicator = {
  init(event) {
    addEventListener("load", this);

    // If the user customizes the position of the indicator, we will
    // not try to re-center it on the primary display after indicator
    // state updates.
    this.positionCustomized = false;

    this.updatingIndicatorState = false;
    this.loaded = false;
  },

  /**
   * Exposed externally so that webrtcUI can alert the indicator to
   * update itself when sharing states have changed.
   */
  updateIndicatorState(initialLayout = false) {
    // It's possible that we were called externally before the indicator
    // finished loading. If so, then bail out - we're going to call
    // updateIndicatorState ourselves automatically once the load
    // event fires.
    if (!this.loaded) {
      return;
    }

    // We've started to update the indicator state. We set this flag so
    // that the MozUpdateWindowPos event handler doesn't interpret indicator
    // state updates as window movement caused by the user.
    this.updatingIndicatorState = true;

    this.updateWindowAttr("sharingvideo", webrtcUI.showCameraIndicator);
    this.updateWindowAttr("sharingaudio", webrtcUI.showMicrophoneIndicator);
    this.updateWindowAttr(
      "sharingscreen",
      webrtcUI.showScreenSharingIndicator.startsWith("Screen")
    );

    // We don't currently support the browser-tab sharing case, so we don't
    // check if the screen sharing indicator starts with "Browser".

    // We special-case sharing a window, because we want to have a slightly
    // different UI if we're sharing a browser window.
    let sharingWindow = webrtcUI.showScreenSharingIndicator.startsWith(
      "Window"
    );
    this.updateWindowAttr("sharingwindow", sharingWindow);

    if (sharingWindow) {
      // Get the active window streams and see if any of them are "scary".
      // If so, then we're sharing a browser window.
      let activeStreams = webrtcUI.getActiveStreams(
        false /* camera */,
        false /* microphone */,
        false /* screen */,
        true /* window */
      );
      let hasBrowserWindow = activeStreams.some(stream => {
        return stream.devices.some(device => device.scary);
      });

      this.updateWindowAttr("sharingbrowserwindow", hasBrowserWindow);
      this.sharingBrowserWindow = hasBrowserWindow;
    } else {
      this.updateWindowAttr("sharingbrowserwindow");
      this.sharingBrowserWindow = false;
    }

    // Resize and ensure the window position is correct
    // (sizeToContent messes with our position).
    window.sizeToContent();

    this.ensureOnScreen();

    if (!this.positionCustomized) {
      this.centerOnPrimaryDisplay();
    }
    this.updatingIndicatorState = false;
  },

  /**
   * After the indicator has been updated, checks to see if it has expanded
   * such that part of the indicator is now outside of the screen. If so,
   * it then adjusts the position to put the entire indicator on screen.
   */
  ensureOnScreen() {
    let desiredX = Math.max(window.screenX, screen.availLeft);
    let maxX =
      screen.availLeft +
      screen.availWidth -
      document.documentElement.clientWidth;
    window.moveTo(Math.min(desiredX, maxX), window.screenY);
  },

  /**
   * Finds the primary display and moves the indicator at the bottom,
   * horizontally centered.
   */
  centerOnPrimaryDisplay() {
    // This should be called in initialize right after we've just called
    // updateIndicatorState. Since updateIndicatorState uses
    // window.sizeToContent, the layout information should be up to date,
    // and so the numbers that we get without flushing should be sufficient.
    let {
      height: windowHeight,
      width: windowWidth,
    } = window.windowUtils.getBoundsWithoutFlushing(document.documentElement);

    // The initial position of the fwindow should be at the bottom of the
    // primary screen, above any OS UI (like the task bar or dock), centered
    // horizontally.
    let screen = gScreenManager.primaryScreen;
    let scaleFactor = screen.contentsScaleFactor / screen.defaultCSSScaleFactor;

    let widthDevPix = {};
    screen.GetRectDisplayPix({}, {}, widthDevPix, {});
    let screenWidth = widthDevPix.value * scaleFactor;

    let availTopDevPix = {};
    let availHeightDevPix = {};
    screen.GetAvailRectDisplayPix({}, availTopDevPix, {}, availHeightDevPix);

    let availHeight =
      (availTopDevPix.value + availHeightDevPix.value) * scaleFactor;
    // To center the window, we subtract the window width from the screen
    // width, and divide by 2.
    //
    // To put the window at the bottom of the screen, just above any OS UI,
    // we subtract the window height from the available height.
    window.moveTo((screenWidth - windowWidth) / 2, availHeight - windowHeight);
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
        this.onLoad(event);
        break;
      }
      case "click": {
        this.onClick(event);
        break;
      }
      case "MozUpdateWindowPos": {
        if (!this.updatingIndicatorState) {
          // The window moved while not updating the indicator state,
          // so the user probably moved it.
          this.positionCustomized = true;
        }
        break;
      }
    }
  },

  onLoad() {
    this.loaded = true;

    this.updateIndicatorState(true /* initialLayout */);
    this.centerOnPrimaryDisplay();

    window.addEventListener("click", this);
    window.windowRoot.addEventListener("MozUpdateWindowPos", this);

    // Alert accessibility implementations stuff just changed. We only need to do
    // this initially, because changes after this will automatically fire alert
    // events if things change materially.
    let ev = new CustomEvent("AlertActive", {
      bubbles: true,
      cancelable: true,
    });
    document.documentElement.dispatchEvent(ev);
    this.loaded = true;
  },

  onClick(event) {
    switch (event.target.id) {
      case "stop-sharing-screen": {
        let activeStreams = webrtcUI.getActiveStreams(
          false /* camera */,
          false /* microphone */,
          true /* screen */,
          false /* window */
        );
        this.stopSharingScreen(activeStreams);
        break;
      }
      case "stop-sharing-window": {
        let activeStreams = webrtcUI.getActiveStreams(
          false /* camera */,
          false /* microphone */,
          false /* screen */,
          true /* window */
        );
        if (this.sharingBrowserWindow) {
          let browserWindowStreams = activeStreams.filter(stream => {
            return stream.devices.some(device => device.scary);
          });
          this.stopSharingScreen(browserWindowStreams);
        } else {
          this.stopSharingScreen(activeStreams);
        }
        break;
      }
      case "microphone-button":
      // Intentional fall-through
      case "camera-button": {
        // Revoking the microphone also revokes the camera and vice-versa.
        let activeStreams = webrtcUI.getActiveStreams(
          true /* camera */,
          true /* microphone */,
          false /* screen */
        );
        this.showSharingDoorhanger(activeStreams);
        break;
      }
      case "minimize": {
        window.minimize();
        break;
      }
    }
  },

  /**
   * Finds the most recent share in the set of active streams passed,
   * and ends it.
   *
   * @param activeStreams (Array<Object>)
   *   An array of streams obtained via webrtcUI.getActiveStreams.
   *   It is presumed that one or more of those streams includes
   *   one that is sharing a screen or window.
   */
  stopSharingScreen(activeStreams) {
    if (!activeStreams.length) {
      return;
    }

    // We'll default to choosing the most recent active stream to
    // revoke the permissions from.
    let chosenStream = activeStreams[activeStreams.length - 1];
    let { browser } = chosenStream;

    // This intentionally copies its approach from browser-siteIdentity.js,
    // which powers the permission revocation from the Permissions Panel.
    // Ideally, we would de-duplicate this with a shared revocation mechanism,
    // but to lower the risk of uplifting this change, we keep it separate for
    // now.
    let gBrowser = browser.getTabBrowser();
    if (!gBrowser) {
      Cu.reportError("Can't stop sharing screen - cannot find gBrowser.");
      return;
    }

    let tab = gBrowser.getTabForBrowser(browser);
    if (!tab) {
      Cu.reportError("Can't stop sharing screen - cannot find tab.");
      return;
    }

    let permissions = SitePermissions.getAllPermissionDetailsForBrowser(
      browser
    );

    let webrtcState = tab._sharingState.webRTC;
    let windowId = `screen:${webrtcState.windowId}`;
    // If WebRTC device or screen permissions are in use, we need to find
    // the associated permission item to set the sharingState field.
    if (webrtcState.screen) {
      let found = false;
      for (let permission of permissions) {
        if (permission.id != "screen") {
          continue;
        }
        found = true;
        permission.sharingState = webrtcState.screen;
        break;
      }
      if (!found) {
        // If the permission item we were looking for doesn't exist,
        // the user has temporarily allowed sharing and we need to add
        // an item in the permissions array to reflect this.
        permissions.push({
          id: "screen",
          state: SitePermissions.ALLOW,
          scope: SitePermissions.SCOPE_REQUEST,
          sharingState: webrtcState.screen,
        });
      }
    }

    let permission = permissions.find(perm => {
      return perm.id == "screen";
    });

    if (!permission) {
      Cu.reportError(
        "Can't stop sharing screen - cannot find screen permission."
      );
      return;
    }

    let bc = webrtcState.browsingContext;
    bc.currentWindowGlobal
      .getActor("WebRTC")
      .sendAsyncMessage("webrtc:StopSharing", windowId);
    webrtcUI.forgetActivePermissionsFromBrowser(browser);

    SitePermissions.removeFromPrincipal(
      browser.contentPrincipal,
      permission.id,
      browser
    );
  },

  /**
   * Find the most recent share in the set of active streams passed,
   * and opens up the Permissions Panel for the associated tab to
   * let the user revoke the streaming permission.
   *
   * @param activeStreams (Array<Object>)
   *   An array of streams obtained via webrtcUI.getActiveStreams.
   */
  showSharingDoorhanger(activeStreams) {
    if (!activeStreams.length) {
      return;
    }

    let index = activeStreams.length - 1;
    webrtcUI.showSharingDoorhanger(activeStreams[index]);
  },

  /**
   * Updates an attribute on the <window> element.
   *
   * @param attr (String)
   *   The name of the attribute to update.
   * @param value (String?)
   *   A string to set the attribute to. If the value is false-y,
   *   the attribute is removed.
   */
  updateWindowAttr(attr, value) {
    let docEl = document.documentElement;
    if (value) {
      docEl.setAttribute(attr, "true");
    } else {
      docEl.removeAttribute(attr);
    }
  },
};

WebRTCIndicator.init();

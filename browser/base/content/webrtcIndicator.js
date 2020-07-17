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
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "MacOSWebRTCStatusbarIndicator",
  "resource:///modules/webrtcUI.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "BrowserWindowTracker",
  "resource:///modules/BrowserWindowTracker.jsm"
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
 * Public function called by webrtcUI to indicate that webrtcUI
 * is about to close the indicator. This is so that we can differentiate
 * between closes that are caused by webrtcUI, and closes that are
 * caused by other reasons (like the user closing the window via the
 * OS window controls somehow).
 *
 * If the window is closed without having called this method first, the
 * indicator will ask webrtcUI to shutdown any remaining streams and then
 * select and focus the most recent browser tab that a stream was shared
 * with.
 */
function closingInternally() {
  WebRTCIndicator.closingInternally();
}

/**
 * Main control object for the WebRTC global indicator
 */
const WebRTCIndicator = {
  // This is the vertical offset from the bottom of the primary display where the
  // indicator will first appear.
  VERTICAL_OFFSET_PX: 80,

  init(event) {
    addEventListener("load", this);
    addEventListener("unload", this);

    // If the user customizes the position of the indicator, we will
    // not try to re-center it on the primary display after indicator
    // state updates.
    this.positionCustomized = false;

    this.updatingIndicatorState = false;
    this.loaded = false;
    this.isClosingInternally = false;

    if (AppConstants.platform == "macosx") {
      this.macOSIndicator = new MacOSWebRTCStatusbarIndicator();
    }

    if (
      Services.prefs.getBoolPref("privacy.webrtc.hideGlobalIndicator", false)
    ) {
      let baseWin = window.docShell.treeOwner.QueryInterface(Ci.nsIBaseWindow);
      baseWin.visibility = false;
    }
  },

  /**
   * Exposed externally so that webrtcUI can alert the indicator to
   * update itself when sharing states have changed.
   */
  updateIndicatorState(initialLayout = false) {
    if (this.macOSIndicator) {
      this.macOSIndicator.updateIndicatorState();
    }
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

    let sharingScreen = webrtcUI.showScreenSharingIndicator.startsWith(
      "Screen"
    );
    this.updateWindowAttr("sharingscreen", sharingScreen);

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

    // The label that's displayed when sharing a display followed a priority.
    // The more "risky" we deem the display is for sharing, the higher priority.
    // This gives us the following priorities, from highest to lowest.
    //
    // 1. Screen
    // 2. Browser window
    // 3. Other application window
    // 4. Browser tab (unimplemented)
    //
    // The CSS for the indicator does the work of showing or hiding these labels
    // for us, but we need to update the aria-labelledby attribute on the container
    // of those labels to make it clearer for screenreaders which one the user cares
    // about.
    let displayShare = document.getElementById("display-share");
    let labelledBy;
    if (sharingScreen) {
      labelledBy = "screen-share-info";
    } else if (this.sharingBrowserWindow) {
      labelledBy = "browser-window-share-info";
    } else if (sharingWindow) {
      labelledBy = "window-share-info";
    }
    displayShare.setAttribute("aria-labelledby", labelledBy);

    if (window.windowState != window.STATE_MINIMIZED) {
      // Resize and ensure the window position is correct
      // (sizeToContent messes with our position).
      let docElStyle = document.documentElement.style;
      docElStyle.minWidth = docElStyle.maxWidth = "unset";
      docElStyle.minHeight = docElStyle.maxHeight = "unset";
      window.sizeToContent();

      // On Linux GTK, the style of window we're using by default is resizable. We
      // workaround this by setting explicit limits on the height and width of the
      // window.
      if (AppConstants.platform == "linux") {
        let { width, height } = window.windowUtils.getBoundsWithoutFlushing(
          document.documentElement
        );

        docElStyle.minWidth = docElStyle.maxWidth = `${width}px`;
        docElStyle.minHeight = docElStyle.maxHeight = `${height}px`;
      }

      this.ensureOnScreen();

      if (!this.positionCustomized) {
        this.centerOnDisplay(initialLayout);
      }
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
   * Finds the appropriate display and moves the indicator at the bottom,
   * horizontally centered.
   *
   * If the indicator is first being opened, the appropriate display is the same
   * display as the most recently used browser window. Otherwise, the
   * appropriate display is the same display that the indicator is currently on.
   */
  centerOnDisplay(aInitialLayout) {
    // This should be called in initialize right after we've just called
    // updateIndicatorState. Since updateIndicatorState uses
    // window.sizeToContent, the layout information should be up to date,
    // and so the numbers that we get without flushing should be sufficient.
    let {
      height: windowHeight,
      width: windowWidth,
    } = window.windowUtils.getBoundsWithoutFlushing(document.documentElement);

    let screen;

    if (aInitialLayout) {
      // The indicator is opening, so find the most recent browser window, and
      // make sure the indicator opens on the same display.
      let recentWindow = BrowserWindowTracker.getTopWindow();

      let {
        height: originatorHeight,
        width: originatorWidth,
      } = recentWindow.windowUtils.getBoundsWithoutFlushing(
        recentWindow.document.documentElement
      );

      screen = gScreenManager.screenForRect(
        recentWindow.screenX,
        recentWindow.screenY,
        originatorWidth,
        originatorHeight
      );
    } else {
      // The indicator is already open, so use the same display that the
      // indicator is already on.
      screen = gScreenManager.screenForRect(
        window.screenX,
        window.screenY,
        windowWidth,
        windowHeight
      );
    }

    let scaleFactor = screen.contentsScaleFactor / screen.defaultCSSScaleFactor;

    // We want to center the indicator horizontally on the display regardless of
    // UI (like a vertical dock) on the left or right sides. This is why we're
    // using GetRectDisplayPix for the left and width screen values.
    let leftDevPix = {};
    let widthDevPix = {};
    screen.GetRectDisplayPix(leftDevPix, {}, widthDevPix, {});
    let screenWidth = widthDevPix.value * scaleFactor;

    // However, we want to make sure that vertically, the indicator is above any
    // existing OS UI, so we use GetAvailRectDisplayPix for the top and height
    // values.
    let availTopDevPix = {};
    let availHeightDevPix = {};
    screen.GetAvailRectDisplayPix({}, availTopDevPix, {}, availHeightDevPix);

    let left = leftDevPix.value * scaleFactor;
    let availHeight =
      (availTopDevPix.value + availHeightDevPix.value) * scaleFactor;
    // To center the window, we subtract the window width from the screen
    // width, and divide by 2.
    //
    // To put the window at the bottom of the screen, just above any OS UI,
    // we subtract the window height from the available height.
    window.moveTo(
      left + (screenWidth - windowWidth) / 2,
      availHeight - windowHeight - this.VERTICAL_OFFSET_PX
    );
  },

  handleEvent(event) {
    switch (event.type) {
      case "load": {
        this.onLoad();
        break;
      }
      case "unload": {
        this.onUnload();
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
      case "sizemodechange": {
        if (window.windowState != window.STATE_MINIMIZED) {
          this.updateIndicatorState();
        }
        break;
      }
    }
  },

  onLoad() {
    this.loaded = true;

    this.updateIndicatorState(true /* initialLayout */);

    window.addEventListener("click", this);
    window.addEventListener("sizemodechange", this);
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

  onUnload() {
    if (this.macOSIndicator) {
      this.macOSIndicator.close();
      this.macOSIndicator = null;
    }

    if (!this.isClosingInternally) {
      // Something has closed the indicator, but it wasn't webrtcUI. This
      // means we might still have some streams being shared. To protect
      // the user from unknowingly sharing streams, we shut those streams
      // down.
      let activeStreams = webrtcUI.getActiveStreams(
        true /* camera */,
        true /* microphone */,
        true /* screen */,
        true /* window */
      );
      webrtcUI.stopSharingStreams(activeStreams);
    }
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
        webrtcUI.stopSharingStreams(activeStreams);
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
          webrtcUI.stopSharingStreams(
            browserWindowStreams,
            false /* camera */,
            false /* microphone */,
            false /* screen */,
            true /* window */
          );
          break;
        }

        webrtcUI.stopSharingStreams(activeStreams);
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

  /**
   * See the documentation on the script global closingInternally() function.
   */
  closingInternally() {
    this.isClosingInternally = true;
  },
};

WebRTCIndicator.init();

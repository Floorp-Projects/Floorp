/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

var PointerlockFsWarning = {
  _element: null,
  _origin: null,

  /**
   * Timeout object for managing timeout request. If it is started when
   * the previous call hasn't finished, it would automatically cancelled
   * the previous one.
   */
  Timeout: class {
    constructor(func, delay) {
      this._id = 0;
      this._func = func;
      this._delay = delay;
    }
    start() {
      this.cancel();
      this._id = setTimeout(() => this._handle(), this._delay);
    }
    cancel() {
      if (this._id) {
        clearTimeout(this._id);
        this._id = 0;
      }
    }
    _handle() {
      this._id = 0;
      this._func();
    }
    get delay() {
      return this._delay;
    }
  },

  showPointerLock(aOrigin) {
    if (!document.fullscreen) {
      let timeout = Services.prefs.getIntPref(
        "pointer-lock-api.warning.timeout"
      );
      this.show(aOrigin, "pointerlock-warning", timeout, 0);
    }
  },

  showFullScreen(aOrigin) {
    let timeout = Services.prefs.getIntPref("full-screen-api.warning.timeout");
    let delay = Services.prefs.getIntPref("full-screen-api.warning.delay");
    this.show(aOrigin, "fullscreen-warning", timeout, delay);
  },

  // Shows a warning that the site has entered fullscreen or
  // pointer lock for a short duration.
  show(aOrigin, elementId, timeout, delay) {
    if (!this._element) {
      this._element = document.getElementById(elementId);
      // Setup event listeners
      this._element.addEventListener("transitionend", this);
      window.addEventListener("mousemove", this, true);
      // The timeout to hide the warning box after a while.
      this._timeoutHide = new this.Timeout(() => {
        this._state = "hidden";
      }, timeout);
      // The timeout to show the warning box when the pointer is at the top
      this._timeoutShow = new this.Timeout(() => {
        this._state = "ontop";
        this._timeoutHide.start();
      }, delay);
    }

    // Set the strings on the warning UI.
    if (aOrigin) {
      this._origin = aOrigin;
    }
    let uri = Services.io.newURI(this._origin);
    let host = null;
    // Make an exception for PDF.js - we'll show "This document" instead.
    if (this._origin != "resource://pdf.js") {
      try {
        host = uri.host;
      } catch (e) {}
    }
    let textElem = this._element.querySelector(
      ".pointerlockfswarning-domain-text"
    );
    if (!host) {
      textElem.hidden = true;
    } else {
      textElem.removeAttribute("hidden");
      // Document's principal's URI has a host. Display a warning including it.
      let utils = {};
      ChromeUtils.import("resource://gre/modules/DownloadUtils.jsm", utils);
      let displayHost = utils.DownloadUtils.getURIHost(uri.spec)[0];
      let l10nString = {
        "fullscreen-warning": "fullscreen-warning-domain",
        "pointerlock-warning": "pointerlock-warning-domain",
      }[elementId];
      document.l10n.setAttributes(textElem, l10nString, {
        domain: displayHost,
      });
    }

    this._element.dataset.identity =
      gIdentityHandler.pointerlockFsWarningClassName;

    // User should be allowed to explicitly disable
    // the prompt if they really want.
    if (this._timeoutHide.delay <= 0) {
      return;
    }

    // Explicitly set the last state to hidden to avoid the warning
    // box being hidden immediately because of mousemove.
    this._state = "onscreen";
    this._lastState = "hidden";
    this._timeoutHide.start();
  },

  close() {
    if (!this._element) {
      return;
    }
    // Cancel any pending timeout
    this._timeoutHide.cancel();
    this._timeoutShow.cancel();
    // Reset state of the warning box
    this._state = "hidden";
    // Reset state of the text so we don't persist or retranslate it.
    this._element
      .querySelector(".pointerlockfswarning-domain-text")
      .removeAttribute("data-l10n-id");
    this._element.hidden = true;
    // Remove all event listeners
    this._element.removeEventListener("transitionend", this);
    window.removeEventListener("mousemove", this, true);
    // Clear fields
    this._element = null;
    this._timeoutHide = null;
    this._timeoutShow = null;

    // Ensure focus switches away from the (now hidden) warning box.
    // If the user clicked buttons in the warning box, it would have
    // been focused, and any key events would be directed at the (now
    // hidden) chrome document instead of the target document.
    gBrowser.selectedBrowser.focus();
  },

  // State could be one of "onscreen", "ontop", "hiding", and
  // "hidden". Setting the state to "onscreen" and "ontop" takes
  // effect immediately, while setting it to "hidden" actually
  // turns the state to "hiding" before the transition finishes.
  _lastState: null,
  _STATES: ["hidden", "ontop", "onscreen"],
  get _state() {
    for (let state of this._STATES) {
      if (this._element.hasAttribute(state)) {
        return state;
      }
    }
    return "hiding";
  },
  set _state(newState) {
    let currentState = this._state;
    if (currentState == newState) {
      return;
    }
    if (currentState != "hiding") {
      this._lastState = currentState;
      this._element.removeAttribute(currentState);
    }
    if (newState != "hidden") {
      if (currentState != "hidden") {
        this._element.setAttribute(newState, true);
      } else {
        // When the previous state is hidden, the display was none,
        // thus no box was constructed. We need to wait for the new
        // display value taking effect first, otherwise, there won't
        // be any transition. Since requestAnimationFrame callback is
        // generally triggered before any style flush and layout, we
        // should wait for the second animation frame.
        requestAnimationFrame(() => {
          requestAnimationFrame(() => {
            if (this._element) {
              this._element.setAttribute(newState, true);
            }
          });
        });
      }
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "mousemove": {
        let state = this._state;
        if (state == "hidden") {
          // If the warning box is currently hidden, show it after
          // a short delay if the pointer is at the top.
          if (event.clientY != 0) {
            this._timeoutShow.cancel();
          } else if (this._timeoutShow.delay >= 0) {
            this._timeoutShow.start();
          }
        } else {
          let elemRect = this._element.getBoundingClientRect();
          if (state == "hiding" && this._lastState != "hidden") {
            // If we are on the hiding transition, and the pointer
            // moved near the box, restore to the previous state.
            if (event.clientY <= elemRect.bottom + 50) {
              this._state = this._lastState;
              this._timeoutHide.start();
            }
          } else if (state == "ontop" || this._lastState != "hidden") {
            // State being "ontop" or the previous state not being
            // "hidden" indicates this current warning box is shown
            // in response to user's action. Hide it immediately when
            // the pointer leaves that area.
            if (event.clientY > elemRect.bottom + 50) {
              this._state = "hidden";
              this._timeoutHide.cancel();
            }
          }
        }
        break;
      }
      case "transitionend": {
        if (this._state == "hiding") {
          this._element.hidden = true;
        }
        break;
      }
    }
  },
};

var PointerLock = {
  entered(originNoSuffix) {
    PointerlockFsWarning.showPointerLock(originNoSuffix);
  },

  exited() {
    PointerlockFsWarning.close();
  },
};

var FullScreen = {
  init() {
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "permissionsFullScreenAllowed",
      "permissions.fullscreen.allowed"
    );

    // Called when the Firefox window go into fullscreen.
    addEventListener("fullscreen", this, true);

    // Called only when fullscreen is requested
    // by the parent (eg: via the browser-menu).
    // Should not be called when the request comes from
    // the content.
    addEventListener("willenterfullscreen", this, true);
    addEventListener("willexitfullscreen", this, true);
    addEventListener("MacFullscreenMenubarRevealUpdate", this, true);

    if (window.fullScreen) {
      this.toggle();
    }
  },

  uninit() {
    this.cleanup();
  },

  willToggle(aWillEnterFullscreen) {
    if (aWillEnterFullscreen) {
      document.documentElement.setAttribute("inFullscreen", true);
    } else {
      document.documentElement.removeAttribute("inFullscreen");
    }
  },

  toggle() {
    var enterFS = window.fullScreen;

    // Toggle the View:FullScreen command, which controls elements like the
    // fullscreen menuitem, and menubars.
    let fullscreenCommand = document.getElementById("View:FullScreen");
    if (enterFS) {
      fullscreenCommand.setAttribute("checked", enterFS);
    } else {
      fullscreenCommand.removeAttribute("checked");
    }

    if (AppConstants.platform == "macosx") {
      // Make sure the menu items are adjusted.
      document.getElementById("enterFullScreenItem").hidden = enterFS;
      document.getElementById("exitFullScreenItem").hidden = !enterFS;
      this.shiftMacToolbarDown(0);
    }

    if (!this._fullScrToggler) {
      this._fullScrToggler = document.getElementById("fullscr-toggler");
      this._fullScrToggler.addEventListener("mouseover", this._expandCallback);
      this._fullScrToggler.addEventListener("dragenter", this._expandCallback);
      this._fullScrToggler.addEventListener("touchmove", this._expandCallback, {
        passive: true,
      });
    }

    if (enterFS) {
      gNavToolbox.setAttribute("inFullscreen", true);
      document.documentElement.setAttribute("inFullscreen", true);
      let alwaysUsesNativeFullscreen =
        AppConstants.platform == "macosx" &&
        Services.prefs.getBoolPref("full-screen-api.macos-native-full-screen");
      if (
        (alwaysUsesNativeFullscreen || !document.fullscreenElement) &&
        AppConstants.platform == "macosx"
      ) {
        document.documentElement.setAttribute("macOSNativeFullscreen", true);
      }
    } else {
      gNavToolbox.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("macOSNativeFullscreen");
    }

    if (!document.fullscreenElement) {
      this._updateToolbars(enterFS);
    }

    if (enterFS) {
      document.addEventListener("keypress", this._keyToggleCallback);
      document.addEventListener("popupshown", this._setPopupOpen);
      document.addEventListener("popuphidden", this._setPopupOpen);
      gURLBar.controller.addQueryListener(this);

      // In DOM fullscreen mode, we hide toolbars with CSS
      if (!document.fullscreenElement) {
        this.hideNavToolbox(true);
      }
    } else {
      this.showNavToolbox(false);
      // This is needed if they use the context menu to quit fullscreen
      this._isPopupOpen = false;
      this.cleanup();
    }
  },

  exitDomFullScreen() {
    if (document.fullscreen) {
      document.exitFullscreen();
    }
  },

  /**
   * Shifts the browser toolbar down when it is moused over on macOS in
   * fullscreen.
   * @param {number} shiftSize
   *   A distance, in pixels, by which to shift the browser toolbar down.
   */
  shiftMacToolbarDown(shiftSize) {
    if (typeof shiftSize !== "number") {
      Cu.reportError("Tried to shift the toolbar by a non-numeric distance.");
      return;
    }

    // shiftSize is sent from Cocoa widget code as a very precise double. We
    // don't need that kind of precision in our CSS.
    shiftSize = shiftSize.toFixed(2);
    let toolbox = document.getElementById("navigator-toolbox");
    let browserEl = document.getElementById("browser");
    if (shiftSize > 0) {
      toolbox.style.setProperty("transform", `translateY(${shiftSize}px)`);
      toolbox.style.setProperty("z-index", "2");
      toolbox.style.setProperty("position", "relative");
      browserEl.style.setProperty("position", "relative");
    } else {
      toolbox.style.removeProperty("transform");
      toolbox.style.removeProperty("z-index");
      toolbox.style.removeProperty("position");
      browserEl.style.removeProperty("position");
    }
  },

  handleEvent(event) {
    switch (event.type) {
      case "willenterfullscreen":
        this.willToggle(true);
        break;
      case "willexitfullscreen":
        this.willToggle(false);
        break;
      case "fullscreen":
        this.toggle();
        break;
      case "MacFullscreenMenubarRevealUpdate":
        this.shiftMacToolbarDown(event.detail);
        break;
    }
  },

  _logWarningPermissionPromptFS(actionStringKey) {
    let consoleMsg = Cc["@mozilla.org/scripterror;1"].createInstance(
      Ci.nsIScriptError
    );
    let message = gBrowserBundle.GetStringFromName(
      `permissions.fullscreen.${actionStringKey}`
    );
    consoleMsg.initWithWindowID(
      message,
      gBrowser.currentURI.spec,
      null,
      0,
      0,
      Ci.nsIScriptError.warningFlag,
      "FullScreen",
      gBrowser.selectedBrowser.innerWindowID
    );
    Services.console.logMessage(consoleMsg);
  },

  _handlePermPromptShow() {
    if (
      !FullScreen.permissionsFullScreenAllowed &&
      window.fullScreen &&
      PopupNotifications.getNotification(
        this._permissionNotificationIDs
      ).filter(n => !n.dismissed).length
    ) {
      this.exitDomFullScreen();
      this._logWarningPermissionPromptFS("fullScreenCanceled");
    }
  },

  enterDomFullscreen(aBrowser, aActor) {
    if (!document.fullscreenElement) {
      aActor.requestOrigin = null;
      return;
    }

    // If we have a current pointerlock warning shown then hide it
    // before transition.
    PointerlockFsWarning.close();

    // If it is a remote browser, send a message to ask the content
    // to enter fullscreen state. We don't need to do so if it is an
    // in-process browser, since all related document should have
    // entered fullscreen state at this point.
    // Additionally, in Fission world, we may need to notify the
    // frames in the middle (content frames that embbed the oop iframe where
    // the element requesting fullscreen lives) to enter fullscreen
    // first.
    // This should be done before the active tab check below to ensure
    // that the content document handles the pending request. Doing so
    // before the check is fine since we also check the activeness of
    // the requesting document in content-side handling code.
    if (this._isRemoteBrowser(aBrowser)) {
      let [targetActor, inProcessBC] = this._getNextMsgRecipientActor(aActor);
      if (!targetActor) {
        // If there is no appropriate actor to send the message we have
        // no way to complete the transition and should abort by exiting
        // fullscreen.
        this._abortEnterFullscreen();
        return;
      }
      targetActor.sendAsyncMessage("DOMFullscreen:Entered", {
        remoteFrameBC: inProcessBC,
      });

      // Record that the actor is waiting for its child to enter
      // fullscreen so that if it dies we can abort.
      targetActor.waitingForChildFullscreen = true;
      if (inProcessBC) {
        // We aren't messaging the request origin yet, skip this time.
        return;
      }
    }

    // If we've received a fullscreen notification, we have to ensure that the
    // element that's requesting fullscreen belongs to the browser that's currently
    // active. If not, we exit fullscreen since the "full-screen document" isn't
    // actually visible now.
    if (
      !aBrowser ||
      gBrowser.selectedBrowser != aBrowser ||
      // The top-level window has lost focus since the request to enter
      // full-screen was made. Cancel full-screen.
      Services.focus.activeWindow != window
    ) {
      this._abortEnterFullscreen();
      return;
    }

    // Remove permission prompts when entering full-screen.
    if (!FullScreen.permissionsFullScreenAllowed) {
      let notifications = PopupNotifications.getNotification(
        this._permissionNotificationIDs
      ).filter(n => !n.dismissed);
      PopupNotifications.remove(notifications, true);
      if (notifications.length) {
        this._logWarningPermissionPromptFS("promptCanceled");
      }
    }
    document.documentElement.setAttribute("inDOMFullscreen", true);

    if (gFindBarInitialized) {
      gFindBar.close(true);
    }

    // Exit DOM full-screen mode when switching to a different tab.
    gBrowser.tabContainer.addEventListener("TabSelect", this.exitDomFullScreen);

    // Addon installation should be cancelled when entering DOM fullscreen for security and usability reasons.
    // Installation prompts in fullscreen can trick the user into installing unwanted addons.
    // In fullscreen the notification box does not have a clear visual association with its parent anymore.
    if (gXPInstallObserver.removeAllNotifications(aBrowser)) {
      // If notifications have been removed, log a warning to the website console
      gXPInstallObserver.logWarningFullScreenInstallBlocked();
    }

    PopupNotifications.panel.addEventListener(
      "popupshowing",
      () => this._handlePermPromptShow(),
      true
    );
  },

  cleanup() {
    if (!window.fullScreen) {
      MousePosTracker.removeListener(this);
      document.removeEventListener("keypress", this._keyToggleCallback);
      document.removeEventListener("popupshown", this._setPopupOpen);
      document.removeEventListener("popuphidden", this._setPopupOpen);
      gURLBar.controller.removeQueryListener(this);
    }
  },

  /**
   * Clean up full screen, starting from the request origin's first ancestor
   * frame that is OOP.
   *
   * If there are OOP ancestor frames, we notify the first of those and then bail to
   * be called again in that process when it has dealt with the change. This is
   * repeated until all ancestor processes have been updated. Once that has happened
   * we remove our handlers and attributes and notify the request origin to complete
   * the cleanup.
   */
  cleanupDomFullscreen(aActor) {
    let [target, inProcessBC] = this._getNextMsgRecipientActor(aActor);
    if (target) {
      target.sendAsyncMessage("DOMFullscreen:CleanUp", {
        remoteFrameBC: inProcessBC,
      });
      if (inProcessBC) {
        return;
      }
    }

    PopupNotifications.panel.removeEventListener(
      "popupshowing",
      () => this._handlePermPromptShow(),
      true
    );

    PointerlockFsWarning.close();
    gBrowser.tabContainer.removeEventListener(
      "TabSelect",
      this.exitDomFullScreen
    );

    document.documentElement.removeAttribute("inDOMFullscreen");
  },

  _abortEnterFullscreen() {
    // This function is called synchronously in fullscreen change, so
    // we have to avoid calling exitFullscreen synchronously here.
    setTimeout(() => document.exitFullscreen(), 0);
    if (TelemetryStopwatch.running("FULLSCREEN_CHANGE_MS")) {
      // Cancel the stopwatch for any fullscreen change to avoid
      // errors if it is started again.
      TelemetryStopwatch.cancel("FULLSCREEN_CHANGE_MS");
    }
  },

  /**
   * Search for the first ancestor of aActor that lives in a different process.
   * If found, that ancestor actor and the browsing context for its child which
   * was in process are returned. Otherwise [request origin, null].
   *
   *
   * @param {JSWindowActorParent} aActor
   *        The actor that called this function.
   *
   * @return {[JSWindowActorParent, BrowsingContext]}
   *         The parent actor which should be sent the next msg and the
   *         in process browsing context which is its child. Will be
   *         [null, null] if there is no OOP parent actor and request origin
   *         is unset. [null, null] is also returned if the intended actor or
   *         the calling actor has been destroyed.
   */
  _getNextMsgRecipientActor(aActor) {
    if (aActor.hasBeenDestroyed()) {
      return [null, null];
    }

    let childBC = aActor.browsingContext;
    let parentBC = childBC.parent;

    // Walk up the browsing context tree from aActor's browsing context
    // to find the first ancestor browsing context that's in a different process.
    while (parentBC) {
      if (!childBC.currentWindowGlobal || !parentBC.currentWindowGlobal) {
        break;
      }
      let childPid = childBC.currentWindowGlobal.osPid;
      let parentPid = parentBC.currentWindowGlobal.osPid;

      if (childPid == parentPid) {
        childBC = parentBC;
        parentBC = childBC.parent;
      } else {
        break;
      }
    }

    let target = null;
    let inProcessBC = null;

    if (parentBC && parentBC.currentWindowGlobal) {
      target = parentBC.currentWindowGlobal.getActor("DOMFullscreen");
      inProcessBC = childBC;
    } else {
      target = aActor.requestOrigin;
    }

    if (!target || target.hasBeenDestroyed()) {
      return [null, null];
    }
    return [target, inProcessBC];
  },

  _isRemoteBrowser(aBrowser) {
    return gMultiProcessBrowser && aBrowser.getAttribute("remote") == "true";
  },

  getMouseTargetRect() {
    return this._mouseTargetRect;
  },

  // Event callbacks
  _expandCallback() {
    FullScreen.showNavToolbox();
  },

  onMouseEnter() {
    this.hideNavToolbox();
  },

  _keyToggleCallback(aEvent) {
    // if we can use the keyboard (eg Ctrl+L or Ctrl+E) to open the toolbars, we
    // should provide a way to collapse them too.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      FullScreen.hideNavToolbox();
    } else if (aEvent.keyCode == aEvent.DOM_VK_F6) {
      // F6 is another shortcut to the address bar, but its not covered in OpenLocation()
      FullScreen.showNavToolbox();
    }
  },

  // Checks whether we are allowed to collapse the chrome
  _isPopupOpen: false,
  _isChromeCollapsed: false,

  _setPopupOpen(aEvent) {
    // Popups should only veto chrome collapsing if they were opened when the chrome was not collapsed.
    // Otherwise, they would not affect chrome and the user would expect the chrome to go away.
    // e.g. we wouldn't want the autoscroll icon firing this event, so when the user
    // toggles chrome when moving mouse to the top, it doesn't go away again.
    let target = aEvent.originalTarget;
    if (target.localName == "tooltip") {
      return;
    }
    if (
      aEvent.type == "popupshown" &&
      !FullScreen._isChromeCollapsed &&
      target.getAttribute("nopreventnavboxhide") != "true"
    ) {
      FullScreen._isPopupOpen = true;
    } else if (aEvent.type == "popuphidden") {
      FullScreen._isPopupOpen = false;
      // Try again to hide toolbar when we close the popup.
      FullScreen.hideNavToolbox(true);
    }
  },

  // UrlbarController listener method
  onViewOpen() {
    if (!this._isChromeCollapsed) {
      this._isPopupOpen = true;
    }
  },

  // UrlbarController listener method
  onViewClose() {
    this._isPopupOpen = false;
    this.hideNavToolbox(true);
  },

  get navToolboxHidden() {
    return this._isChromeCollapsed;
  },

  // Autohide helpers for the context menu item
  getAutohide(aItem) {
    aItem.setAttribute(
      "checked",
      Services.prefs.getBoolPref("browser.fullscreen.autohide")
    );
  },
  setAutohide() {
    Services.prefs.setBoolPref(
      "browser.fullscreen.autohide",
      !Services.prefs.getBoolPref("browser.fullscreen.autohide")
    );
    // Try again to hide toolbar when we change the pref.
    FullScreen.hideNavToolbox(true);
  },

  showNavToolbox(trackMouse = true) {
    if (BrowserHandler.kiosk) {
      return;
    }
    this._fullScrToggler.hidden = true;
    gNavToolbox.removeAttribute("fullscreenShouldAnimate");
    gNavToolbox.style.marginTop = "";

    if (!this._isChromeCollapsed) {
      return;
    }

    // Track whether mouse is near the toolbox
    if (trackMouse) {
      let rect = gBrowser.tabpanels.getBoundingClientRect();
      this._mouseTargetRect = {
        top: rect.top + 50,
        bottom: rect.bottom,
        left: rect.left,
        right: rect.right,
      };
      MousePosTracker.addListener(this);
    }

    this._isChromeCollapsed = false;
    Services.obs.notifyObservers(null, "fullscreen-nav-toolbox", "shown");
  },

  hideNavToolbox(aAnimate = false) {
    if (this._isChromeCollapsed) {
      return;
    }
    if (!Services.prefs.getBoolPref("browser.fullscreen.autohide")) {
      return;
    }
    // a popup menu is open in chrome: don't collapse chrome
    if (this._isPopupOpen) {
      return;
    }

    // a textbox in chrome is focused (location bar anyone?): don't collapse chrome
    // unless we are kiosk mode
    let focused = document.commandDispatcher.focusedElement;
    if (
      focused &&
      focused.ownerDocument == document &&
      focused.localName == "input" &&
      !BrowserHandler.kiosk
    ) {
      // But try collapse the chrome again when anything happens which can make
      // it lose the focus. We cannot listen on "blur" event on focused here
      // because that event can be triggered by "mousedown", and hiding chrome
      // would cause the content to move. This combination may split a single
      // click into two actionless halves.
      let retryHideNavToolbox = () => {
        // Wait for at least a frame to give it a chance to be passed down to
        // the content.
        requestAnimationFrame(() => {
          setTimeout(() => {
            // In the meantime, it's possible that we exited fullscreen somehow,
            // so only hide the toolbox if we're still in fullscreen mode.
            if (window.fullScreen) {
              this.hideNavToolbox(aAnimate);
            }
          }, 0);
        });
        window.removeEventListener("keydown", retryHideNavToolbox);
        window.removeEventListener("click", retryHideNavToolbox);
      };
      window.addEventListener("keydown", retryHideNavToolbox);
      window.addEventListener("click", retryHideNavToolbox);
      return;
    }

    if (!BrowserHandler.kiosk) {
      this._fullScrToggler.hidden = false;
    }

    if (
      aAnimate &&
      window.matchMedia("(prefers-reduced-motion: no-preference)").matches &&
      !BrowserHandler.kiosk
    ) {
      gNavToolbox.setAttribute("fullscreenShouldAnimate", true);
    }

    gNavToolbox.style.marginTop =
      -gNavToolbox.getBoundingClientRect().height + "px";
    this._isChromeCollapsed = true;
    Services.obs.notifyObservers(null, "fullscreen-nav-toolbox", "hidden");

    MousePosTracker.removeListener(this);
  },

  _updateToolbars(aEnterFS) {
    for (let el of document.querySelectorAll(
      "toolbar[fullscreentoolbar=true]"
    )) {
      if (aEnterFS) {
        // Give the main nav bar and the tab bar the fullscreen context menu,
        // otherwise remove context menu to prevent breakage
        el.setAttribute("saved-context", el.getAttribute("context"));
        if (el.id == "nav-bar" || el.id == "TabsToolbar") {
          el.setAttribute("context", "autohide-context");
        } else {
          el.removeAttribute("context");
        }

        // Set the inFullscreen attribute to allow specific styling
        // in fullscreen mode
        el.setAttribute("inFullscreen", true);
      } else {
        if (el.hasAttribute("saved-context")) {
          el.setAttribute("context", el.getAttribute("saved-context"));
          el.removeAttribute("saved-context");
        }
        el.removeAttribute("inFullscreen");
      }
    }

    ToolbarIconColor.inferFromText("fullscreen", aEnterFS);

    // For macOS, we use native full screen, all full screen controls
    // are hidden, don't bother to touch them. If we don't stop here,
    // the following code could cause the native full screen button be
    // shown unexpectedly. See bug 1165570.
    if (AppConstants.platform == "macosx") {
      return;
    }

    var fullscreenctls = document.getElementById("window-controls");
    var navbar = document.getElementById("nav-bar");
    var ctlsOnTabbar = window.toolbar.visible;
    if (fullscreenctls.parentNode == navbar && ctlsOnTabbar) {
      fullscreenctls.removeAttribute("flex");
      document.getElementById("TabsToolbar").appendChild(fullscreenctls);
    } else if (fullscreenctls.parentNode.id == "TabsToolbar" && !ctlsOnTabbar) {
      fullscreenctls.setAttribute("flex", "1");
      navbar.appendChild(fullscreenctls);
    }
    fullscreenctls.hidden = !aEnterFS;
  },
};

XPCOMUtils.defineLazyGetter(FullScreen, "_permissionNotificationIDs", () => {
  let { PermissionUI } = ChromeUtils.import(
    "resource:///modules/PermissionUI.jsm",
    {}
  );
  return (
    Object.values(PermissionUI)
      .filter(value => value.prototype && value.prototype.notificationID)
      .map(value => value.prototype.notificationID)
      // Additionally include webRTC permission prompt which does not use PermissionUI
      .concat(["webRTC-shareDevices"])
  );
});

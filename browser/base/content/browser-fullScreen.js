# -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

var FullScreen = {
  _MESSAGES: [
    "DOMFullscreen:Request",
    "DOMFullscreen:NewOrigin",
    "DOMFullscreen:Exit",
  ],

  init: function() {
    // called when we go into full screen, even if initiated by a web page script
    window.addEventListener("fullscreen", this, true);
    window.addEventListener("MozDOMFullscreen:Entered", this,
                            /* useCapture */ true,
                            /* wantsUntrusted */ false);
    window.addEventListener("MozDOMFullscreen:Exited", this,
                            /* useCapture */ true,
                            /* wantsUntrusted */ false);
    for (let type of this._MESSAGES) {
      window.messageManager.addMessageListener(type, this);
    }

    if (window.fullScreen)
      this.toggle();
  },

  uninit: function() {
    for (let type of this._MESSAGES) {
      window.messageManager.removeMessageListener(type, this);
    }
    this.cleanup();
  },

  toggle: function () {
    var enterFS = window.fullScreen;

    // Toggle the View:FullScreen command, which controls elements like the
    // fullscreen menuitem, and menubars.
    let fullscreenCommand = document.getElementById("View:FullScreen");
    if (enterFS) {
      fullscreenCommand.setAttribute("checked", enterFS);
    } else {
      fullscreenCommand.removeAttribute("checked");
    }

#ifdef XP_MACOSX
    // Make sure the menu items are adjusted.
    document.getElementById("enterFullScreenItem").hidden = enterFS;
    document.getElementById("exitFullScreenItem").hidden = !enterFS;
#endif

    if (!this._fullScrToggler) {
      this._fullScrToggler = document.getElementById("fullscr-toggler");
      this._fullScrToggler.addEventListener("mouseover", this._expandCallback, false);
      this._fullScrToggler.addEventListener("dragenter", this._expandCallback, false);
    }

    if (enterFS) {
      gNavToolbox.setAttribute("inFullscreen", true);
      document.documentElement.setAttribute("inFullscreen", true);
      if (!document.mozFullScreen && this.useLionFullScreen)
        document.documentElement.setAttribute("OSXLionFullscreen", true);
    } else {
      gNavToolbox.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("inFullscreen");
      document.documentElement.removeAttribute("OSXLionFullscreen");
    }

    if (!document.mozFullScreen)
      this._updateToolbars(enterFS);

    if (enterFS) {
      document.addEventListener("keypress", this._keyToggleCallback, false);
      document.addEventListener("popupshown", this._setPopupOpen, false);
      document.addEventListener("popuphidden", this._setPopupOpen, false);
      // In DOM fullscreen mode, we hide toolbars with CSS
      if (!document.mozFullScreen)
        this.hideNavToolbox(true);
    }
    else {
      this.showNavToolbox(false);
      // This is needed if they use the context menu to quit fullscreen
      this._isPopupOpen = false;
      this.cleanup();
      // In TabsInTitlebar._update(), we cancel the appearance update on
      // resize event for exiting fullscreen, since that happens before we
      // change the UI here in the "fullscreen" event. Hence we need to
      // call it here to ensure the appearance is properly updated. See
      // TabsInTitlebar._update() and bug 1173768.
      TabsInTitlebar.updateAppearance(true);
    }
  },

  exitDomFullScreen : function() {
    document.mozCancelFullScreen();
  },

  handleEvent: function (event) {
    switch (event.type) {
      case "activate":
        if (document.mozFullScreen) {
          this.showWarning(this.fullscreenOrigin);
        }
        break;
      case "fullscreen":
        this.toggle();
        break;
      case "transitionend":
        if (event.propertyName == "opacity")
          this.cancelWarning();
        break;
      case "MozDOMFullscreen:Entered": {
        // The event target is the element which requested the DOM
        // fullscreen. If we were entering DOM fullscreen for a remote
        // browser, the target would be `gBrowser` and the original
        // target would be the browser which was the parameter of
        // `remoteFrameFullscreenChanged` call. If the fullscreen
        // request was initiated from an in-process browser, we need
        // to get its corresponding browser here.
        let browser;
        if (event.target == gBrowser) {
          browser = event.originalTarget;
        } else {
          let topWin = event.target.ownerDocument.defaultView.top;
          browser = gBrowser.getBrowserForContentWindow(topWin);
        }
        if (!browser || !this.enterDomFullscreen(browser)) {
          if (document.mozFullScreen) {
            // MozDOMFullscreen:Entered is dispatched synchronously in
            // fullscreen change, hence we have to avoid calling this
            // method synchronously here.
            setTimeout(() => document.mozCancelFullScreen(), 0);
          }
          break;
        }
        // If it is a remote browser, send a message to ask the content
        // to enter fullscreen state. We don't need to do so if it is an
        // in-process browser, since all related document should have
        // entered fullscreen state at this point.
        if (this._isRemoteBrowser(browser)) {
          browser.messageManager.sendAsyncMessage("DOMFullscreen:Entered");
        }
        break;
      }
      case "MozDOMFullscreen:Exited":
        this.cleanupDomFullscreen();
        break;
    }
  },

  receiveMessage: function(aMessage) {
    let browser = aMessage.target;
    switch (aMessage.name) {
      case "DOMFullscreen:Request": {
        this._windowUtils.remoteFrameFullscreenChanged(browser);
        break;
      }
      case "DOMFullscreen:NewOrigin": {
        this.showWarning(aMessage.data.originNoSuffix);
        break;
      }
      case "DOMFullscreen:Exit": {
        this._windowUtils.remoteFrameFullscreenReverted();
        break;
      }
    }
  },

  enterDomFullscreen : function(aBrowser) {
    if (!document.mozFullScreen)
      return false;

    // If we've received a fullscreen notification, we have to ensure that the
    // element that's requesting fullscreen belongs to the browser that's currently
    // active. If not, we exit fullscreen since the "full-screen document" isn't
    // actually visible now.
    if (gBrowser.selectedBrowser != aBrowser) {
      return false;
    }

    let focusManager = Services.focus;
    if (focusManager.activeWindow != window) {
      // The top-level window has lost focus since the request to enter
      // full-screen was made. Cancel full-screen.
      return false;
    }

    document.documentElement.setAttribute("inDOMFullscreen", true);

    if (gFindBarInitialized)
      gFindBar.close();

    // Exit DOM full-screen mode upon open, close, or change tab.
    gBrowser.tabContainer.addEventListener("TabOpen", this.exitDomFullScreen);
    gBrowser.tabContainer.addEventListener("TabClose", this.exitDomFullScreen);
    gBrowser.tabContainer.addEventListener("TabSelect", this.exitDomFullScreen);

    // Add listener to detect when the fullscreen window is re-focused.
    // If a fullscreen window loses focus, we show a warning when the
    // fullscreen window is refocused.
    window.addEventListener("activate", this);

    return true;
  },

  cleanup: function () {
    if (!window.fullScreen) {
      MousePosTracker.removeListener(this);
      document.removeEventListener("keypress", this._keyToggleCallback, false);
      document.removeEventListener("popupshown", this._setPopupOpen, false);
      document.removeEventListener("popuphidden", this._setPopupOpen, false);
    }
  },

  cleanupDomFullscreen: function () {
    this.cancelWarning();
    gBrowser.tabContainer.removeEventListener("TabOpen", this.exitDomFullScreen);
    gBrowser.tabContainer.removeEventListener("TabClose", this.exitDomFullScreen);
    gBrowser.tabContainer.removeEventListener("TabSelect", this.exitDomFullScreen);
    window.removeEventListener("activate", this);

    document.documentElement.removeAttribute("inDOMFullscreen");

    window.messageManager
          .broadcastAsyncMessage("DOMFullscreen:CleanUp");
  },

  _isRemoteBrowser: function (aBrowser) {
    return gMultiProcessBrowser && aBrowser.getAttribute("remote") == "true";
  },

  get _windowUtils() {
    return window.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIDOMWindowUtils);
  },

  getMouseTargetRect: function()
  {
    return this._mouseTargetRect;
  },

  // Event callbacks
  _expandCallback: function()
  {
    FullScreen.showNavToolbox();
  },
  onMouseEnter: function()
  {
    FullScreen.hideNavToolbox();
  },
  _keyToggleCallback: function(aEvent)
  {
    // if we can use the keyboard (eg Ctrl+L or Ctrl+E) to open the toolbars, we
    // should provide a way to collapse them too.
    if (aEvent.keyCode == aEvent.DOM_VK_ESCAPE) {
      FullScreen.hideNavToolbox();
    }
    // F6 is another shortcut to the address bar, but its not covered in OpenLocation()
    else if (aEvent.keyCode == aEvent.DOM_VK_F6)
      FullScreen.showNavToolbox();
  },

  // Checks whether we are allowed to collapse the chrome
  _isPopupOpen: false,
  _isChromeCollapsed: false,
  _safeToCollapse: function () {
    if (!gPrefService.getBoolPref("browser.fullscreen.autohide"))
      return false;

    // a popup menu is open in chrome: don't collapse chrome
    if (this._isPopupOpen)
      return false;

    // On OS X Lion we don't want to hide toolbars.
    if (this.useLionFullScreen)
      return false;

    // a textbox in chrome is focused (location bar anyone?): don't collapse chrome
    if (document.commandDispatcher.focusedElement &&
        document.commandDispatcher.focusedElement.ownerDocument == document &&
        document.commandDispatcher.focusedElement.localName == "input") {
      return false;
    }

    return true;
  },

  _setPopupOpen: function(aEvent)
  {
    // Popups should only veto chrome collapsing if they were opened when the chrome was not collapsed.
    // Otherwise, they would not affect chrome and the user would expect the chrome to go away.
    // e.g. we wouldn't want the autoscroll icon firing this event, so when the user
    // toggles chrome when moving mouse to the top, it doesn't go away again.
    if (aEvent.type == "popupshown" && !FullScreen._isChromeCollapsed &&
        aEvent.target.localName != "tooltip" && aEvent.target.localName != "window")
      FullScreen._isPopupOpen = true;
    else if (aEvent.type == "popuphidden" && aEvent.target.localName != "tooltip" &&
             aEvent.target.localName != "window")
      FullScreen._isPopupOpen = false;
  },

  // Autohide helpers for the context menu item
  getAutohide: function(aItem)
  {
    aItem.setAttribute("checked", gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },
  setAutohide: function()
  {
    gPrefService.setBoolPref("browser.fullscreen.autohide", !gPrefService.getBoolPref("browser.fullscreen.autohide"));
  },

  cancelWarning: function(event) {
    if (!this.warningBox)
      return;
    this.warningBox.removeEventListener("transitionend", this);
    if (this.warningFadeOutTimeout) {
      clearTimeout(this.warningFadeOutTimeout);
      this.warningFadeOutTimeout = null;
    }

    // Ensure focus switches away from the (now hidden) warning box. If the user
    // clicked buttons in the fullscreen key authorization UI, it would have been
    // focused, and any key events would be directed at the (now hidden) chrome
    // document instead of the target document.
    gBrowser.selectedBrowser.focus();

    this.warningBox.setAttribute("hidden", true);
    this.warningBox.removeAttribute("fade-warning-out");
    this.warningBox.removeAttribute("obscure-browser");
    this.warningBox = null;
  },

  setFullscreenAllowed: function(isApproved) {
    // The "remember decision" checkbox is hidden when showing for documents that
    // the permission manager can't handle (documents with URIs without a host).
    // We simply require those to be approved every time instead.
    let rememberCheckbox = document.getElementById("full-screen-remember-decision");
    let uri = BrowserUtils.makeURI(this.fullscreenOrigin);
    if (!rememberCheckbox.hidden) {
      if (rememberCheckbox.checked)
        Services.perms.add(uri,
                           "fullscreen",
                           isApproved ? Services.perms.ALLOW_ACTION : Services.perms.DENY_ACTION,
                           Services.perms.EXPIRE_NEVER);
      else if (isApproved) {
        // The user has only temporarily approved fullscren for this fullscreen
        // session only. Add the permission (so Gecko knows to approve any further
        // fullscreen requests for this host in this fullscreen session) but add
        // a listener to revoke the permission when the chrome document exits
        // fullscreen.
        Services.perms.add(uri,
                           "fullscreen",
                           Services.perms.ALLOW_ACTION,
                           Services.perms.EXPIRE_SESSION);
        var onFullscreenchange = function onFullscreenchange(event) {
          if (event.target == document && document.mozFullScreenElement == null) {
            // The chrome document has left fullscreen. Remove the temporary permission grant.
            Services.perms.remove(uri, "fullscreen");
            document.removeEventListener("mozfullscreenchange", onFullscreenchange);
          }
        }
        document.addEventListener("mozfullscreenchange", onFullscreenchange);
      }
    }
    if (this.warningBox)
      this.warningBox.setAttribute("fade-warning-out", "true");
    // If the document has been granted fullscreen, notify Gecko so it can resume
    // any pending pointer lock requests, otherwise exit fullscreen; the user denied
    // the fullscreen request.
    if (isApproved) {
      gBrowser.selectedBrowser
              .messageManager
              .sendAsyncMessage("DOMFullscreen:Approved");
    } else {
      document.mozCancelFullScreen();
    }
  },

  warningBox: null,
  warningFadeOutTimeout: null,

  // Shows the fullscreen approval UI, or if the domain has already been approved
  // for fullscreen, shows a warning that the site has entered fullscreen for a short
  // duration.
  showWarning: function(aOrigin) {
    if (!document.mozFullScreen ||
        !gPrefService.getBoolPref("full-screen-api.approval-required"))
      return;

    // Set the strings on the fullscreen approval UI.
    this.fullscreenOrigin = aOrigin;
    let uri = BrowserUtils.makeURI(aOrigin);
    let host = null;
    try {
      host = uri.host;
    } catch (e) { }
    let hostLabel = document.getElementById("full-screen-domain-text");
    let rememberCheckbox = document.getElementById("full-screen-remember-decision");
    let isApproved = false;
    if (host) {
      // Document's principal's URI has a host. Display a warning including the hostname and
      // show UI to enable the user to permanently grant this host permission to enter fullscreen.
      let utils = {};
      Cu.import("resource://gre/modules/DownloadUtils.jsm", utils);
      let displayHost = utils.DownloadUtils.getURIHost(uri.spec)[0];
      let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

      hostLabel.textContent = bundle.formatStringFromName("fullscreen.entered", [displayHost], 1);
      hostLabel.removeAttribute("hidden");

      rememberCheckbox.label = bundle.formatStringFromName("fullscreen.rememberDecision", [displayHost], 1);
      rememberCheckbox.checked = false;
      rememberCheckbox.removeAttribute("hidden");

      // Note we only allow documents whose principal's URI has a host to
      // store permission grants.
      isApproved = Services.perms.testPermission(uri, "fullscreen") == Services.perms.ALLOW_ACTION;
    } else {
      hostLabel.setAttribute("hidden", "true");
      rememberCheckbox.setAttribute("hidden", "true");
    }

    // Note: the warning box can be non-null if the warning box from the previous request
    // wasn't hidden before another request was made.
    if (!this.warningBox) {
      this.warningBox = document.getElementById("full-screen-warning-container");
      // Add a listener to clean up state after the warning is hidden.
      this.warningBox.addEventListener("transitionend", this);
      this.warningBox.removeAttribute("hidden");
    } else {
      if (this.warningFadeOutTimeout) {
        clearTimeout(this.warningFadeOutTimeout);
        this.warningFadeOutTimeout = null;
      }
      this.warningBox.removeAttribute("fade-warning-out");
    }

    // If fullscreen mode has not yet been approved for the fullscreen
    // document's domain, show the approval UI and don't auto fade out the
    // fullscreen warning box. Otherwise, we're just notifying of entry into
    // fullscreen mode. Note if the resource's host is null, we must be
    // showing a local file or a local data URI, and we require explicit
    // approval every time.
    let authUI = document.getElementById("full-screen-approval-pane");
    if (isApproved) {
      authUI.setAttribute("hidden", "true");
      this.warningBox.removeAttribute("obscure-browser");
    } else {
      // Partially obscure the <browser> element underneath the approval UI.
      this.warningBox.setAttribute("obscure-browser", "true");
      authUI.removeAttribute("hidden");
    }

    // If we're not showing the fullscreen approval UI, we're just notifying the user
    // of the transition, so set a timeout to fade the warning out after a few moments.
    if (isApproved)
      this.warningFadeOutTimeout =
        setTimeout(
          function() {
            if (this.warningBox)
              this.warningBox.setAttribute("fade-warning-out", "true");
          }.bind(this),
          3000);
  },

  showNavToolbox: function(trackMouse = true) {
    this._fullScrToggler.hidden = true;
    gNavToolbox.removeAttribute("fullscreenShouldAnimate");
    gNavToolbox.style.marginTop = "";

    if (!this._isChromeCollapsed) {
      return;
    }

    // Track whether mouse is near the toolbox
    if (trackMouse && !this.useLionFullScreen) {
      let rect = gBrowser.mPanelContainer.getBoundingClientRect();
      this._mouseTargetRect = {
        top: rect.top + 50,
        bottom: rect.bottom,
        left: rect.left,
        right: rect.right
      };
      MousePosTracker.addListener(this);
    }

    this._isChromeCollapsed = false;
  },

  hideNavToolbox: function (aAnimate = false) {
    if (this._isChromeCollapsed || !this._safeToCollapse())
      return;

    this._fullScrToggler.hidden = false;

    if (aAnimate && gPrefService.getBoolPref("browser.fullscreen.animate")) {
      gNavToolbox.setAttribute("fullscreenShouldAnimate", true);
      // Hide the fullscreen toggler until the transition ends.
      let listener = () => {
        gNavToolbox.removeEventListener("transitionend", listener, true);
        if (this._isChromeCollapsed)
          this._fullScrToggler.hidden = false;
      };
      gNavToolbox.addEventListener("transitionend", listener, true);
      this._fullScrToggler.hidden = true;
    }

    gNavToolbox.style.marginTop =
      -gNavToolbox.getBoundingClientRect().height + "px";
    this._isChromeCollapsed = true;
    MousePosTracker.removeListener(this);
  },

  _updateToolbars: function (aEnterFS) {
    for (let el of document.querySelectorAll("toolbar[fullscreentoolbar=true]")) {
      if (aEnterFS) {
        // Give the main nav bar and the tab bar the fullscreen context menu,
        // otherwise remove context menu to prevent breakage
        el.setAttribute("saved-context", el.getAttribute("context"));
        if (el.id == "nav-bar" || el.id == "TabsToolbar")
          el.setAttribute("context", "autohide-context");
        else
          el.removeAttribute("context");

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

    ToolbarIconColor.inferFromText();

    // For Lion fullscreen, all fullscreen controls are hidden, don't
    // bother to touch them. If we don't stop here, the following code
    // could cause the native fullscreen button be shown unexpectedly.
    // See bug 1165570.
    if (this.useLionFullScreen) {
      return;
    }

    var fullscreenctls = document.getElementById("window-controls");
    var navbar = document.getElementById("nav-bar");
    var ctlsOnTabbar = window.toolbar.visible;
    if (fullscreenctls.parentNode == navbar && ctlsOnTabbar) {
      fullscreenctls.removeAttribute("flex");
      document.getElementById("TabsToolbar").appendChild(fullscreenctls);
    }
    else if (fullscreenctls.parentNode.id == "TabsToolbar" && !ctlsOnTabbar) {
      fullscreenctls.setAttribute("flex", "1");
      navbar.appendChild(fullscreenctls);
    }
    fullscreenctls.hidden = !aEnterFS;
  }
};
XPCOMUtils.defineLazyGetter(FullScreen, "useLionFullScreen", function() {
  // We'll only use OS X Lion full screen if we're
  // * on OS X
  // * on Lion or higher (Darwin 11+)
  // * have fullscreenbutton="true"
#ifdef XP_MACOSX
  return parseFloat(Services.sysinfo.getProperty("version")) >= 11 &&
         document.documentElement.getAttribute("fullscreenbutton") == "true";
#else
  return false;
#endif
});

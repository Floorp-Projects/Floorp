/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Tracks whether context UI (app bar, tab bar, url bar) is shown or hidden.
 * Manages events to summon and hide the context UI.
 */
var ContextUI = {
  _expandable: true,
  _hidingId: 0,

  /*******************************************
   * init
   */

  init: function init() {
    Elements.browsers.addEventListener("mousedown", this, true);
    Elements.browsers.addEventListener("touchstart", this, true);
    Elements.browsers.addEventListener("AlertActive", this, true);
    window.addEventListener("MozEdgeUIStarted", this, true);
    window.addEventListener("MozEdgeUICanceled", this, true);
    window.addEventListener("MozEdgeUICompleted", this, true);
    window.addEventListener("keypress", this, true);
    window.addEventListener("KeyboardChanged", this, false);

    Elements.tray.addEventListener("transitionend", this, true);

    Appbar.init();
  },

  /*******************************************
   * Context UI state getters & setters
   */

  get isVisible() {
    return (Elements.navbar.hasAttribute("visible") ||
            Elements.navbar.hasAttribute("startpage"));
  },
  get isExpanded() { return Elements.tray.hasAttribute("expanded"); },
  get isExpandable() { return this._expandable; },

  set isExpandable(aFlag) {
    this._expandable = aFlag;
    if (!this._expandable)
      this.dismiss();
  },

  /*******************************************
   * Context UI state control
   */

  toggle: function toggle() {
    if (!this._expandable) {
      // exandable setter takes care of resetting state
      // so if we're not expandable, there's nothing to do here
      return;
    }
    // if we're not showing, show
    if (!this.dismiss()) {
      dump("* ContextUI is hidden, show it\n");
      this.show();
    }
  },

  // show all context UI
  // returns true if any non-visible UI was shown
  show: function() {
    let shown = false;
    if (!this.isExpanded) {
      // show the tab tray
      this._setIsExpanded(true);
      shown = true;
    }
    if (!Elements.navbar.isShowing) {
      // show the navbar
      Elements.navbar.show();
      shown = true;
    }

    this._clearDelayedTimeout();
    if (shown) {
      ContentAreaObserver.update(window.innerWidth, window.innerHeight);
    }
    return shown;
  },

  // Display the nav bar
  displayNavbar: function displayNavbar() {
    this._clearDelayedTimeout();
    Elements.navbar.show();
  },

  // Display the toolbar and tabs
  displayTabs: function displayTabs() {
    this._clearDelayedTimeout();
    this._setIsExpanded(true, true);
  },

  /** Briefly show the tab bar and then hide it */
  peekTabs: function peekTabs() {
    if (this.isExpanded) {
      setTimeout(function () {
        ContextUI.dismissWithDelay(kNewTabAnimationDelayMsec);
      }, 0);
    } else {
      BrowserUI.setOnTabAnimationEnd(function () {
        ContextUI.dismissWithDelay(kNewTabAnimationDelayMsec);
      });

      this.displayTabs();
    }
  },

  // Dismiss all context UI.
  // Returns true if any visible UI was dismissed.
  dismiss: function dismiss() {
    let dismissed = false;
    if (this.isExpanded) {
      this._setIsExpanded(false);
      dismissed = true;
    }
    if (Elements.navbar.isShowing) {
      this.dismissAppbar();
      dismissed = true;
    }
    this._clearDelayedTimeout();
    if (dismissed) {
      ContentAreaObserver.update(window.innerWidth, window.innerHeight);
    }
    return dismissed;
  },

  // Dismiss all context ui after a delay
  dismissWithDelay: function dismissWithDelay(aDelay) {
    aDelay = aDelay || kHideContextAndTrayDelayMsec;
    this._clearDelayedTimeout();
    this._hidingId = setTimeout(function () {
        ContextUI.dismiss();
      }, aDelay);
  },

  // Cancel any pending delayed dismiss
  cancelDismiss: function cancelDismiss() {
    this._clearDelayedTimeout();
  },

  dismissTabs: function dimissTabs() {
    this._clearDelayedTimeout();
    this._setIsExpanded(false, true);
  },

  dismissAppbar: function dismissAppbar() {
    this._fire("MozAppbarDismiss");
  },

  /*******************************************
   * Internal tray state setters
   */

  // tab tray state
  _setIsExpanded: function _setIsExpanded(aFlag, setSilently) {
    // if the tray can't be expanded, don't expand it.
    if (!this.isExpandable || this.isExpanded == aFlag)
      return;

    if (aFlag)
      Elements.tray.setAttribute("expanded", "true");
    else
      Elements.tray.removeAttribute("expanded");

    if (!setSilently)
      this._fire("MozContextUIExpand");
  },

  /*******************************************
   * Internal utils
   */

  _clearDelayedTimeout: function _clearDelayedTimeout() {
    if (this._hidingId) {
      clearTimeout(this._hidingId);
      this._hidingId = 0;
    }
  },

  /*******************************************
   * Events
   */

  _onEdgeUIStarted: function(aEvent) {
    this._hasEdgeSwipeStarted = true;
    this._clearDelayedTimeout();

    if (StartUI.hide()) {
      this.dismiss();
      return;
    }
    this.toggle();
  },

  _onEdgeUICanceled: function(aEvent) {
    this._hasEdgeSwipeStarted = false;
    StartUI.hide();
    this.dismiss();
  },

  _onEdgeUICompleted: function(aEvent) {
    if (this._hasEdgeSwipeStarted) {
      this._hasEdgeSwipeStarted = false;
      return;
    }

    this._clearDelayedTimeout();
    if (StartUI.hide()) {
      this.dismiss();
      return;
    }
    this.toggle();
  },

  handleEvent: function handleEvent(aEvent) {
    switch (aEvent.type) {
      case "MozEdgeUIStarted":
        this._onEdgeUIStarted(aEvent);
        break;
      case "MozEdgeUICanceled":
        this._onEdgeUICanceled(aEvent);
        break;
      case "MozEdgeUICompleted":
        this._onEdgeUICompleted(aEvent);
        break;
      case "mousedown":
        if (aEvent.button == 0 && this.isVisible)
          this.dismiss();
        break;
      case "touchstart":
      // ContextUI can hide the notification bar. Workaround until bug 845348 is fixed.
      case "AlertActive":
        this.dismiss();
        break;
      case "keypress":
        if (String.fromCharCode(aEvent.which) == "z" &&
            aEvent.getModifierState("Win"))
          this.toggle();
        break;
      case "transitionend":
        setTimeout(function () {
          ContentAreaObserver.updateContentArea();
        }, 0);
        break;
      case "KeyboardChanged":
        this.dismissTabs();
        break;
    }
  },

  _fire: function (name) {
    let event = document.createEvent("Events");
    event.initEvent(name, true, true);
    window.dispatchEvent(event);
  }
};

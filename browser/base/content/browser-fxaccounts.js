# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

const PREF_SYNC_START_DOORHANGER = "services.sync.ui.showSyncStartDoorhanger";

let gFxAccounts = {

  _initialized: false,
  _originalLabel: null,
  _inCustomizationMode: false,

  get weave() {
    delete this.weave;
    return this.weave = Cc["@mozilla.org/weave/service;1"]
                          .getService(Ci.nsISupports)
                          .wrappedJSObject;
  },

  get topics() {
    // Do all this dance to lazy-load FxAccountsCommon.
    delete this.topics;
    return this.topics = [
      FxAccountsCommon.ONLOGIN_NOTIFICATION,
      FxAccountsCommon.ONVERIFIED_NOTIFICATION,
      FxAccountsCommon.ONLOGOUT_NOTIFICATION,
      "weave:service:sync:start"
    ];
  },

  // The set of topics that only the active window should handle.
  get activeWindowTopics() {
    // Do all this dance to lazy-load FxAccountsCommon.
    delete this.activeWindowTopics;
    return this.activeWindowTopics = new Set([
      "weave:service:sync:start",
      FxAccountsCommon.ONVERIFIED_NOTIFICATION
    ]);
  },

  get button() {
    delete this.button;
    return this.button = document.getElementById("PanelUI-fxa-status");
  },

  get isActiveWindow() {
    let mostRecentNonPopupWindow =
      RecentWindow.getMostRecentBrowserWindow({allowPopups: false});
    return window == mostRecentNonPopupWindow;
  },

  init: function () {
    // Bail out if we're already initialized and for pop-up windows.
    if (this._initialized || !window.toolbar.visible) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.addObserver(this, topic, false);
    }

    gNavToolbox.addEventListener("customizationstarting", this);
    gNavToolbox.addEventListener("customizationending", this);

    // Save the button's original label so that
    // we can restore it if overridden later.
    this._originalLabel = this.button.getAttribute("label");
    this._initialized = true;

    this.updateUI();
  },

  uninit: function () {
    if (!this._initialized) {
      return;
    }

    for (let topic of this.topics) {
      Services.obs.removeObserver(this, topic);
    }

    this._initialized = false;
  },

  observe: function (subject, topic) {
    // Ignore certain topics if we're not the active window.
    if (this.activeWindowTopics.has(topic) && !this.isActiveWindow) {
      return;
    }

    switch (topic) {
      case FxAccountsCommon.ONVERIFIED_NOTIFICATION:
        Services.prefs.setBoolPref(PREF_SYNC_START_DOORHANGER, true);
        break;
      case "weave:service:sync:start":
        this.onSyncStart();
        break;
      default:
        this.updateUI();
        break;
    }
  },

  onSyncStart: function () {
    let showDoorhanger = false;

    try {
      showDoorhanger = Services.prefs.getBoolPref(PREF_SYNC_START_DOORHANGER);
    } catch (e) { /* The pref might not exist. */ }

    if (showDoorhanger) {
      Services.prefs.clearUserPref(PREF_SYNC_START_DOORHANGER);
      this.showSyncStartedDoorhanger();
    }
  },

  handleEvent: function (event) {
    this._inCustomizationMode = event.type == "customizationstarting";
    this.updateUI();
  },

  showDoorhanger: function (id) {
    let panel = document.getElementById(id);
    let anchor = document.getElementById("PanelUI-menu-button");

    let iconAnchor =
      document.getAnonymousElementByAttribute(anchor, "class",
                                              "toolbarbutton-icon");

    panel.hidden = false;
    panel.openPopup(iconAnchor || anchor, "bottomcenter topright");
  },

  showSyncStartedDoorhanger: function () {
    this.showDoorhanger("sync-start-panel");
  },

  showSyncFailedDoorhanger: function () {
    this.showDoorhanger("sync-error-panel");
  },

  updateUI: function () {
    // Bail out if FxA is disabled.
    if (!this.weave.fxAccountsEnabled) {
      return;
    }

    // FxA is enabled, show the widget.
    this.button.removeAttribute("hidden");

    // Make sure the button is disabled in customization mode.
    if (this._inCustomizationMode) {
      this.button.setAttribute("disabled", "true");
    } else {
      this.button.removeAttribute("disabled");
    }

    // If the user is signed into their Firefox account and we are not
    // currently in customization mode, show their email address.
    fxAccounts.getSignedInUser().then(userData => {
      if (userData && !this._inCustomizationMode) {
        this.button.setAttribute("signedin", "true");
        this.button.setAttribute("label", userData.email);
        this.button.setAttribute("tooltiptext", userData.email);
      } else {
        this.button.removeAttribute("signedin");
        this.button.setAttribute("label", this._originalLabel);
        this.button.removeAttribute("tooltiptext");
      }
    });
  },

  toggle: function (event) {
    if (event.originalTarget.hasAttribute("signedin")) {
      this.openPreferences();
    } else {
      this.openSignInPage();
    }

    PanelUI.hide();
  },

  openPreferences: function () {
    openPreferences("paneSync");
  },

  openSignInPage: function () {
    switchToTabHavingURI("about:accounts", true);
  }
};

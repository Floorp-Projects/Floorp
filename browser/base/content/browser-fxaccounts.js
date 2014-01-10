# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

XPCOMUtils.defineLazyGetter(this, "FxAccountsCommon", function () {
  return Cu.import("resource://gre/modules/FxAccountsCommon.js", {});
});

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
    delete this.topics;
    return this.topics = [
      FxAccountsCommon.ONLOGIN_NOTIFICATION,
      FxAccountsCommon.ONVERIFIED_NOTIFICATION,
      FxAccountsCommon.ONLOGOUT_NOTIFICATION
    ];
  },

  get button() {
    delete this.button;
    return this.button = document.getElementById("PanelUI-fxa-status");
  },

  init: function () {
    if (this._initialized) {
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
    if (topic == FxAccountsCommon.ONVERIFIED_NOTIFICATION) {
      this.showDoorhanger();
    } else {
      this.updateUI();
    }
  },

  handleEvent: function (event) {
    this._inCustomizationMode = event.type == "customizationstarting";
    this.updateUI();
  },

  showDoorhanger: function () {
    let panel = document.getElementById("sync-popup");
    let anchor = document.getElementById("PanelUI-menu-button");

    let iconAnchor =
      document.getAnonymousElementByAttribute(anchor, "class",
                                              "toolbarbutton-icon");

    panel.hidden = false;
    panel.openPopup(iconAnchor || anchor, "bottomcenter topright");
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
      openPreferences("paneSync");
    } else {
      switchToTabHavingURI("about:accounts", true);
    }

    PanelUI.hide();
  }
};

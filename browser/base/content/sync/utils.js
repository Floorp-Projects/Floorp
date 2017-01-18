/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Weave should always exist before before this file gets included.
var gSyncUtils = {

  get fxAccountsEnabled() {
    let service = Components.classes["@mozilla.org/weave/service;1"]
                            .getService(Components.interfaces.nsISupports)
                            .wrappedJSObject;
    return service.fxAccountsEnabled;
  },

  // opens in a new window if we're in a modal prefwindow world, in a new tab otherwise
  _openLink(url) {
    let thisDocEl = document.documentElement,
        openerDocEl = window.opener && window.opener.document.documentElement;
    if (thisDocEl.id == "accountSetup" && window.opener &&
        openerDocEl.id == "BrowserPreferences" && !openerDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (thisDocEl.id == "BrowserPreferences" && !thisDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (document.documentElement.id == "change-dialog")
      Services.wm.getMostRecentWindow("navigator:browser")
              .openUILinkIn(url, "tab");
    else
      openUILinkIn(url, "tab");
  },

  changeName: function changeName(input) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Service.clientsEngine.localName = input.value;
    input.value = Weave.Service.clientsEngine.localName;
  },

  resetPassword() {
    this._openLink(Weave.Service.pwResetURL);
  },

  get tosURL() {
    let root = this.fxAccountsEnabled ? "fxa." : "";
    return Weave.Svc.Prefs.get(root + "termsURL");
  },

  get privacyPolicyURL() {
    let root = this.fxAccountsEnabled ? "fxa." : "";
    return Weave.Svc.Prefs.get(root + "privacyURL");
  }
};

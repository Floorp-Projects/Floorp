/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Weave should always exist before before this file gets included.
let gSyncUtils = {
  // opens in a new window if we're in a modal prefwindow world, in a new tab otherwise
  _openLink: function (url) {
    let thisDocEl = document.documentElement,
        openerDocEl = window.opener && window.opener.document.documentElement;
    if (thisDocEl.id == "BrowserPreferences" && !thisDocEl.instantApply) {
      openUILinkIn(url, "window");
    } else {
      openUILinkIn(url, "tab");
    }
  },

  changeName: function changeName(input) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Service.clientsEngine.localName = input.value;
    input.value = Weave.Service.clientsEngine.localName;
  },

  openToS: function () {
    this._openLink(Weave.Svc.Prefs.get("termsURL"));
  },

  openPrivacyPolicy: function () {
    this._openLink(Weave.Svc.Prefs.get("privacyURL"));
  },

  openAccountsPage: function () {
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    win.switchToTabHavingURI("about:accounts", true);
  }
};

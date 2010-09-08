/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Weave.
 *
 * The Initial Developer of the Original Code is the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Paul O’Shannessy <paul@oshannessy.com>
 *   Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

Components.utils.import("resource://services-sync/service.js");
Components.utils.import("resource://gre/modules/Services.jsm");

const PAGE_NO_ACCOUNT = 0;
const PAGE_HAS_ACCOUNT = 1;

let gSyncPane = {
  _stringBundle: null,
  prefArray: ["engine.bookmarks", "engine.passwords", "engine.prefs",
              "engine.tabs", "engine.history"],

  get page() {
    return document.getElementById("weavePrefsDeck").selectedIndex;
  },

  set page(val) {
    document.getElementById("weavePrefsDeck").selectedIndex = val;
  },

  get _usingCustomServer() {
    return Weave.Svc.Prefs.isSet("serverURL");
  },

  onLoginStart: function () {
    if (this.page == PAGE_NO_ACCOUNT)
      return;

    document.getElementById("loginFeedbackRow").hidden = true;
    document.getElementById("connectThrobber").hidden = false;
  },

  onLoginError: function () {
    if (this.page == PAGE_NO_ACCOUNT)
      return;

    document.getElementById("connectThrobber").hidden = true;
    document.getElementById("loginFeedbackRow").hidden = false;
    let label = document.getElementById("loginError");
    label.value = Weave.Utils.getErrorString(Weave.Status.login);
    label.className = "error";
  },

  onLoginFinish: function () {
    document.getElementById("connectThrobber").hidden = true;
    this.updateWeavePrefs();
  },

  init: function () {
    let obs = [
      ["weave:service:login:start",   "onLoginStart"],
      ["weave:service:login:error",   "onLoginError"],
      ["weave:service:login:finish",  "onLoginFinish"],
      ["weave:service:start-over",    "updateWeavePrefs"],
      ["weave:service:setup-complete","updateWeavePrefs"],
      ["weave:service:logout:finish", "updateWeavePrefs"]];

    // Add the observers now and remove them on unload
    let self = this;
    let addRem = function(add) {
      obs.forEach(function([topic, func]) {
        //XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
        //        of `this`. Fix in a followup. (bug 583347)
        if (add)
          Weave.Svc.Obs.add(topic, self[func], self);
        else
          Weave.Svc.Obs.remove(topic, self[func], self);
      });
    };
    addRem(true);
    window.addEventListener("unload", function() addRem(false), false);

    this._stringBundle =
      Services.strings.createBundle("chrome://browser/locale/preferences/preferences.properties");;
    this.updateWeavePrefs();
  },

  updateWeavePrefs: function () {
    if (Weave.Status.service == Weave.CLIENT_NOT_CONFIGURED ||
        Weave.Svc.Prefs.get("firstSync", "") == "notReady")
      this.page = PAGE_NO_ACCOUNT;
    else {
      this.page = PAGE_HAS_ACCOUNT;
      document.getElementById("currentUser").value = Weave.Service.username;
      document.getElementById("syncComputerName").value = Weave.Clients.localName;
      if (Weave.Status.service == Weave.LOGIN_FAILED)
        this.onLoginError();
      this.updateConnectButton();
      document.getElementById("tosPP").hidden = this._usingCustomServer;
    }
  },

  updateConnectButton: function () {
    let str = Weave.Service.isLoggedIn ? this._stringBundle.GetStringFromName("disconnect.label")
                                       : this._stringBundle.GetStringFromName("connect.label");
    document.getElementById("connectButton").label = str;
  },

  handleConnectCommand: function () {
    Weave.Service.isLoggedIn ? Weave.Service.logout() : Weave.Service.login();
  },

  startOver: function (showDialog) {
    if (showDialog) {
      let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                  Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL;
      let buttonChoice =
        Services.prompt.confirmEx(window,
                                  this._stringBundle.GetStringFromName("differentAccount.title"),
                                  this._stringBundle.GetStringFromName("differentAccount.label"),
                                  flags,
                                  this._stringBundle.GetStringFromName("differentAccountConfirm.label"),
                                  null, null, null, {});

      // If the user selects cancel, just bail
      if (buttonChoice == 1)
        return;
    }

    this.handleExpanderClick();
    Weave.Service.startOver();
    this.updateWeavePrefs();
    document.getElementById("manageAccountExpander").className = "expander-down";
    document.getElementById("manageAccountControls").hidden = true;
  },

  updatePass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.changePassword();
    else
      gSyncUtils.updatePassphrase();
  },

  resetPass: function () {
    if (Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED)
      gSyncUtils.resetPassword();
    else
      gSyncUtils.resetPassphrase();
  },

  handleExpanderClick: function () {
    //XXXzpao Might be fixed in bug 583441, otherwise we'll need a new bug.
    // ok, this is pretty evil, and likely fragile if the prefwindow
    // binding changes, but that won't happen in 3.6 *fingers crossed*
    let prefwindow = document.documentElement;
    let pane = document.getElementById("paneSync");
    if (prefwindow._shouldAnimate)
      prefwindow._currentHeight = pane.contentHeight;

    let expander = document.getElementById("manageAccountExpander");
    let expand = expander.className == "expander-down";
    expander.className =
       expand ? "expander-up" : "expander-down";
    document.getElementById("manageAccountControls").hidden = !expand;

    // and... shazam
    if (prefwindow._shouldAnimate)
      prefwindow.animate("null", pane);
  },

  openSetup: function (resetSync) {
    var win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
    if (win)
      win.focus();
    else {
      window.openDialog("chrome://browser/content/syncSetup.xul",
                        "weaveSetup", "centerscreen,chrome,resizable=no", resetSync);
    }
  },

  resetSync: function () {
    this.openSetup(true);
  }
}


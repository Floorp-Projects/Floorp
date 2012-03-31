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
const PAGE_NEEDS_UPDATE = 2;

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

  needsUpdate: function () {
    this.page = PAGE_NEEDS_UPDATE;
    let label = document.getElementById("loginError");
    label.value = Weave.Utils.getErrorString(Weave.Status.login);
    label.className = "error";
  },

  init: function () {
    let topics = ["weave:service:login:error",
                  "weave:service:login:finish",
                  "weave:service:start-over",
                  "weave:service:setup-complete",
                  "weave:service:logout:finish"];

    // Add the observers now and remove them on unload
    //XXXzpao This should use Services.obs.* but Weave's Obs does nice handling
    //        of `this`. Fix in a followup. (bug 583347)
    topics.forEach(function (topic) {
      Weave.Svc.Obs.add(topic, this.updateWeavePrefs, this);
    }, this);
    window.addEventListener("unload", function() {
      topics.forEach(function (topic) {
        Weave.Svc.Obs.remove(topic, this.updateWeavePrefs, this);
      }, gSyncPane);
    }, false);

    this._stringBundle =
      Services.strings.createBundle("chrome://browser/locale/preferences/preferences.properties");
    this.updateWeavePrefs();
  },

  updateWeavePrefs: function () {
    if (Weave.Status.service == Weave.CLIENT_NOT_CONFIGURED ||
        Weave.Svc.Prefs.get("firstSync", "") == "notReady") {
      this.page = PAGE_NO_ACCOUNT;
    } else if (Weave.Status.login == Weave.LOGIN_FAILED_INVALID_PASSPHRASE ||
               Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED) {
      this.needsUpdate();
    } else {
      this.page = PAGE_HAS_ACCOUNT;
      document.getElementById("accountName").value = Weave.Identity.account;
      document.getElementById("syncComputerName").value = Weave.Clients.localName;
      document.getElementById("tosPP").hidden = this._usingCustomServer;
    }
  },

  startOver: function (showDialog) {
    if (showDialog) {
      let flags = Services.prompt.BUTTON_POS_0 * Services.prompt.BUTTON_TITLE_IS_STRING +
                  Services.prompt.BUTTON_POS_1 * Services.prompt.BUTTON_TITLE_CANCEL + 
                  Services.prompt.BUTTON_POS_1_DEFAULT;
      let buttonChoice =
        Services.prompt.confirmEx(window,
                                  this._stringBundle.GetStringFromName("syncUnlink.title"),
                                  this._stringBundle.GetStringFromName("syncUnlink.label"),
                                  flags,
                                  this._stringBundle.GetStringFromName("syncUnlinkConfirm.label"),
                                  null, null, null, {});

      // If the user selects cancel, just bail
      if (buttonChoice == 1)
        return;
    }

    Weave.Service.startOver();
    this.updateWeavePrefs();
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

  /**
   * Invoke the Sync setup wizard.
   * 
   * @param wizardType
   *        Indicates type of wizard to launch:
   *          null    -- regular set up wizard
   *          "pair"  -- pair a device first
   *          "reset" -- reset sync
   */
  openSetup: function (wizardType) {
    var win = Services.wm.getMostRecentWindow("Weave:AccountSetup");
    if (win)
      win.focus();
    else {
      window.openDialog("chrome://browser/content/sync/setup.xul",
                        "weaveSetup", "centerscreen,chrome,resizable=no",
                        wizardType);
    }
  },

  openQuotaDialog: function () {
    let win = Services.wm.getMostRecentWindow("Sync:ViewQuota");
    if (win)
      win.focus();
    else 
      window.openDialog("chrome://browser/content/sync/quota.xul", "",
                        "centerscreen,chrome,dialog,modal");
  },

  openAddDevice: function () {
    if (!Weave.Utils.ensureMPUnlocked())
      return;
    
    let win = Services.wm.getMostRecentWindow("Sync:AddDevice");
    if (win)
      win.focus();
    else 
      window.openDialog("chrome://browser/content/sync/addDevice.xul",
                        "syncAddDevice", "centerscreen,chrome,resizable=no");
  },

  resetSync: function () {
    this.openSetup("reset");
  }
}


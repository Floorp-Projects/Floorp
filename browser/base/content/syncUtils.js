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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Edward Lee <edilee@mozilla.com>
 *   Mike Connor <mconnor@mozilla.com>
 *   Paul O’Shannessy <paul@oshannessy.com>
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

// Weave should always exist before before this file gets included.
let gSyncUtils = {
  // opens in a new window if we're in a modal prefwindow world, in a new tab otherwise
  _openLink: function (url) {
    let thisDocEl = document.documentElement,
        openerDocEl = window.opener && window.opener.document.documentElement;
    if (thisDocEl.id == "accountSetup" && window.opener &&
        openerDocEl.id == "BrowserPreferences" && !openerDocEl.instantApply)
      openUILinkIn(url, "window");
    else if (thisDocEl.id == "BrowserPreferences" && !thisDocEl.instantApply)
      openUILinkIn(url, "window");
    else
      openUILinkIn(url, "tab");
  },

  changeName: function changeName(input) {
    // Make sure to update to a modified name, e.g., empty-string -> default
    Weave.Clients.localName = input.value;
    input.value = Weave.Clients.localName;
  },

  openChange: function openChange(type) {
    // Just re-show the dialog if it's already open
    let openedDialog = Weave.Svc.WinMediator.getMostRecentWindow("Sync:" + type);
    if (openedDialog != null) {
      openedDialog.focus();
      return;
    }

    // Open up the change dialog
    let changeXUL = "chrome://browser/content/syncGenericChange.xul";
    let changeOpt = "centerscreen,chrome,dialog,modal,resizable=no";
    Weave.Svc.WinWatcher.activeWindow.openDialog(changeXUL, "", changeOpt, type);
  },

  changePassword: function () {
    this.openChange("ChangePassword");
  },

  resetPassphrase: function () {
    this.openChange("ResetPassphrase");
  },

  updatePassphrase: function () {
    this.openChange("UpdatePassphrase");
  },

  resetPassword: function () {
    this._openLink(Weave.Service.pwResetURL);
  },

  openToS: function () {
    this._openLink(Weave.Svc.Prefs.get("termsURL"));
  },

  openPrivacyPolicy: function () {
    this._openLink(Weave.Svc.Prefs.get("privacyURL"));
  },

  // xxxmpc - fix domain before 1.3 final (bug 583652)
  _baseURL: "http://www.mozilla.com/firefox/sync/",

  openFirstClientFirstrun: function () {
    let url = this._baseURL + "firstrun.html";
    this._openLink(url);
  },

  openAddedClientFirstrun: function () {
    let url = this._baseURL + "secondrun.html";
    this._openLink(url);
  },

  /**
   * validatePassword / validatePassphrase
   *
   * @param el1 : the first textbox element in the form
   * @param el2 : the second textbox element, if omitted it's an update form
   * 
   * returns [valid, errorString]
   */

  validatePassword: function (el1, el2) {
    return this._validate(el1, el2, true);
  },

  validatePassphrase: function (el1, el2) {
    return this._validate(el1, el2, false);
  },

  _validate: function (el1, el2, isPassword) {
    let valid = false;
    let val1 = el1.value;
    let val2 = el2 ? el2.value : "";
    let error = "";

    if (isPassword) {
      if (!el2)
        valid = val1.length >= Weave.MIN_PASS_LENGTH;
      else if (val1 && val1 == Weave.Service.username)
        error = "change.password.pwSameAsUsername";
      else if (val1 && val1 == Weave.Service.password)
        error = "change.password.pwSameAsPassword";
      else if (val1 && val1 == Weave.Service.passphrase)
        error = "change.password.pwSameAsPassphrase";
      else if (val1 && val2) {
        if (val1 == val2 && val1.length >= Weave.MIN_PASS_LENGTH)
          valid = true;
        else if (val1.length < Weave.MIN_PASS_LENGTH)
          error = "change.password.tooShort";
        else if (val1 != val2)
          error = "change.password.mismatch";
      }
    }
    else {
      if (!el2)
        valid = val1.length >= Weave.MIN_PP_LENGTH;
      else if (val1 == Weave.Service.username)
        error = "change.passphrase.ppSameAsUsername";
      else if (val1 == Weave.Service.password)
        error = "change.passphrase.ppSameAsPassword";
      else if (val1 == Weave.Service.passphrase)
        error = "change.passphrase.ppSameAsPassphrase";
      else if (val1 && val2) {
        if (val1 == val2 && val1.length >= Weave.MIN_PP_LENGTH)
          valid = true;
        else if (val1.length < Weave.MIN_PP_LENGTH)
          error = "change.passphrase.tooShort";
        else if (val1 != val2)
          error = "change.passphrase.mismatch";
      }
    }
    let errorString = error ? Weave.Utils.getErrorString(error) : "";
    dump("valid: " + valid + " error: " + errorString + "\n");
    return [valid, errorString];
  }
}


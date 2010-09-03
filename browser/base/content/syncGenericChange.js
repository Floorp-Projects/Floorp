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

let Change = {
  _dialog: null,
  _dialogType: null,
  _status: null,
  _statusIcon: null,
  _firstBox: null,
  _secondBox: null,

  get _currentPasswordInvalid() {
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  get _updatingPassphrase() {
    return this._dialogType == "UpdatePassphrase";
  },

  onLoad: function Change_onLoad() {
    /* Load labels */
    let box1label = document.getElementById("textBox1Label");
    let box2label = document.getElementById("textBox2Label");
    let introText = document.getElementById("introText");
    let introText2 = document.getElementById("introText2");
    let warningText = document.getElementById("warningText");

    // load some other elements & info from the window
    this._dialog = document.getElementById("change-dialog");
    this._dialogType = window.arguments[0];
    this._status = document.getElementById("status");
    this._statusIcon = document.getElementById("statusIcon");
    this._firstBox = document.getElementById("textBox1");
    this._secondBox = document.getElementById("textBox2");

    this._stringBundle =
      Services.strings.createBundle("chrome://browser/locale/syncGenericChange.properties");

    switch (this._dialogType) {
      case "UpdatePassphrase":
      case "ResetPassphrase":
        box1label.value = this._str("new.passphrase.label");

        if (this._updatingPassphrase) {
          document.title = this._str("new.passphrase.title");
          introText.textContent = this._str("new.passphrase.introText");
          this._dialog.getButton("accept")
              .setAttribute("label", this._str("new.passphrase.acceptButton"));
          document.getElementById("textBox2Row").hidden = true;
        }
        else {
          document.title = this._str("change.passphrase.title");
          box2label.value = this._str("new.passphrase.confirm");
          introText.textContent = this._str("change.passphrase.introText");
          introText2.textContent = this._str("change.passphrase.introText2");
          warningText.textContent = this._str("change.passphrase.warningText");
          this._dialog.getButton("accept")
              .setAttribute("label", this._str("change.passphrase.acceptButton"));
        }
        break;
      case "ChangePassword":
        box1label.value = this._str("new.password.label");

        if (this._currentPasswordInvalid) {
          document.title = this._str("new.password.title");
          introText.textContent = this._str("new.password.introText");
          this._dialog.getButton("accept")
              .setAttribute("label", this._str("new.password.acceptButton"));
          document.getElementById("textBox2Row").hidden = true;
        }
        else {
          document.title = this._str("change.password.title");
          box2label.value = this._str("new.password.confirm");
          introText.textContent = this._str("change.password.introText");
          warningText.textContent = this._str("change.password.warningText");
          this._dialog.getButton("accept")
              .setAttribute("label", this._str("change.password.acceptButton"));
        }
        break;
    }
  },

  _clearStatus: function _clearStatus() {
    this._status.value = "";
    this._statusIcon.removeAttribute("status");
  },

  _updateStatus: function Change__updateStatus(str, state) {
     this._updateStatusWithString(this._str(str), state);
  },
  
  _updateStatusWithString: function Change__updateStatusWithString(string, state) {
    this._status.value = string;
    this._statusIcon.setAttribute("status", state);

    let error = state == "error";
    this._dialog.getButton("cancel").setAttribute("disabled", !error);
    this._dialog.getButton("accept").setAttribute("disabled", !error);

    if (state == "success")
      window.setTimeout(window.close, 1500);
  },

  onDialogAccept: function() {
    switch (this._dialogType) {
      case "UpdatePassphrase":
      case "ResetPassphrase":
        return this.doChangePassphrase();
        break;
      case "ChangePassword":
        return this.doChangePassword();
        break;
    }
  },

  doChangePassphrase: function Change_doChangePassphrase() {
    if (this._updatingPassphrase) {
      Weave.Service.passphrase = this._firstBox.value;
      if (Weave.Service.login()) {
        this._updateStatus("change.passphrase.success", "success");
        Weave.Service.persistLogin();
      }
      else {
        this._updateStatus("new.passphrase.status.incorrect", "error");
      }
    }
    else {
      this._updateStatus("change.passphrase.label", "active");

      if (Weave.Service.changePassphrase(this._firstBox.value))
        this._updateStatus("change.passphrase.success", "success");
      else
        this._updateStatus("change.passphrase.error", "error");
    }

    return false;
  },

  doChangePassword: function Change_doChangePassword() {
    if (this._currentPasswordInvalid) {
      Weave.Service.password = this._firstBox.value;
      if (Weave.Service.login()) {
        this._updateStatus("change.password.status.success", "success");
        Weave.Service.persistLogin();
      }
      else {
        this._updateStatus("new.password.status.incorrect", "error");
      }
    }
    else {
      this._updateStatus("change.password.status.active", "active");

      if (Weave.Service.changePassword(this._firstBox.value))
        this._updateStatus("change.password.status.success", "success");
      else
        this._updateStatus("change.password.status.error", "error");
    }

    return false;
  },

  validate: function (event) {
    let valid = false;
    let errorString = "";

    if (this._dialogType == "ChangePassword") {
      if (this._currentPasswordInvalid)
        [valid, errorString] = gSyncUtils.validatePassword(this._firstBox);
      else
        [valid, errorString] = gSyncUtils.validatePassword(this._firstBox, this._secondBox);
    }
    else {
      if (this._updatingPassphrase)
        [valid, errorString] = gSyncUtils.validatePassphrase(this._firstBox);
      else
        [valid, errorString] = gSyncUtils.validatePassphrase(this._firstBox, this._secondBox);
    }

    if (errorString == "")
      this._clearStatus();
    else
      this._updateStatusWithString(errorString, "error");

    this._dialog.getButton("accept").disabled = !valid;
  },

  _str: function Change__string(str) {
    return this._stringBundle.GetStringFromName(str);
  }
};

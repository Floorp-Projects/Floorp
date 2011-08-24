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

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://services-sync/main.js");
Components.utils.import("resource://gre/modules/Services.jsm");

let Change = {
  _dialog: null,
  _dialogType: null,
  _status: null,
  _statusIcon: null,
  _firstBox: null,
  _secondBox: null,

  get _passphraseBox() {
    delete this._passphraseBox;
    return this._passphraseBox = document.getElementById("passphraseBox");
  },

  get _currentPasswordInvalid() {
    return Weave.Status.login == Weave.LOGIN_FAILED_LOGIN_REJECTED;
  },

  get _updatingPassphrase() {
    return this._dialogType == "UpdatePassphrase";
  },

  onLoad: function Change_onLoad() {
    /* Load labels */
    let introText = document.getElementById("introText");
    let introText2 = document.getElementById("introText2");
    let warningText = document.getElementById("warningText");

    // load some other elements & info from the window
    this._dialog = document.getElementById("change-dialog");
    this._dialogType = window.arguments[0];
    this._duringSetup = window.arguments[1];
    this._status = document.getElementById("status");
    this._statusIcon = document.getElementById("statusIcon");
    this._statusRow = document.getElementById("statusRow");
    this._firstBox = document.getElementById("textBox1");
    this._secondBox = document.getElementById("textBox2");

    this._dialog.getButton("finish").disabled = true;
    this._dialog.getButton("back").hidden = true;

    this._stringBundle =
      Services.strings.createBundle("chrome://browser/locale/syncGenericChange.properties");

    switch (this._dialogType) {
      case "UpdatePassphrase":
      case "ResetPassphrase":
        document.getElementById("textBox1Row").hidden = true;
        document.getElementById("textBox2Row").hidden = true;
        document.getElementById("passphraseLabel").value
          = this._str("new.recoverykey.label");
        document.getElementById("passphraseSpacer").hidden = false;

        if (this._updatingPassphrase) {
          document.getElementById("passphraseHelpBox").hidden = false;
          document.title = this._str("new.recoverykey.title");
          introText.textContent = this._str("new.recoverykey.introText");
          this._dialog.getButton("finish").label
            = this._str("new.recoverykey.acceptButton");
        }
        else {
          document.getElementById("generatePassphraseButton").hidden = false;
          document.getElementById("passphraseBackupButtons").hidden = false;
          this._passphraseBox.setAttribute("readonly", "true");
          let pp = Weave.Service.passphrase;
          if (Weave.Utils.isPassphrase(pp))
             pp = Weave.Utils.hyphenatePassphrase(pp);
          this._passphraseBox.value = pp;
          this._passphraseBox.focus();
          document.title = this._str("change.recoverykey.title");
          introText.textContent = this._str("change.synckey.introText2");
          warningText.textContent = this._str("change.recoverykey.warningText");
          this._dialog.getButton("finish").label
            = this._str("change.recovery.acceptButton");
          if (this._duringSetup) {
            this._dialog.getButton("finish").disabled = false;
          }
        }
        break;
      case "ChangePassword":
        document.getElementById("passphraseRow").hidden = true;
        let box1label = document.getElementById("textBox1Label");
        let box2label = document.getElementById("textBox2Label");
        box1label.value = this._str("new.password.label");

        if (this._currentPasswordInvalid) {
          document.title = this._str("new.password.title");
          introText.textContent = this._str("new.password.introText");
          this._dialog.getButton("finish").label
            = this._str("new.password.acceptButton");
          document.getElementById("textBox2Row").hidden = true;
        }
        else {
          document.title = this._str("change.password.title");
          box2label.value = this._str("new.password.confirm");
          introText.textContent = this._str("change.password3.introText");
          warningText.textContent = this._str("change.password.warningText");
          this._dialog.getButton("finish").label
            = this._str("change.password.acceptButton");
        }
        break;
    }
    document.getElementById("change-page")
            .setAttribute("label", document.title);
  },

  _clearStatus: function _clearStatus() {
    this._status.value = "";
    this._statusIcon.removeAttribute("status");
  },

  _updateStatus: function Change__updateStatus(str, state) {
     this._updateStatusWithString(this._str(str), state);
  },
  
  _updateStatusWithString: function Change__updateStatusWithString(string, state) {
    this._statusRow.hidden = false;
    this._status.value = string;
    this._statusIcon.setAttribute("status", state);

    let error = state == "error";
    this._dialog.getButton("cancel").disabled = !error;
    this._dialog.getButton("finish").disabled = !error;
    document.getElementById("printSyncKeyButton").disabled = !error;
    document.getElementById("saveSyncKeyButton").disabled = !error;

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

  doGeneratePassphrase: function () {
    let passphrase = Weave.Utils.generatePassphrase();
    this._passphraseBox.value = Weave.Utils.hyphenatePassphrase(passphrase);
    this._dialog.getButton("finish").disabled = false;
  },

  doChangePassphrase: function Change_doChangePassphrase() {
    let pp = Weave.Utils.normalizePassphrase(this._passphraseBox.value);
    if (this._updatingPassphrase) {
      Weave.Service.passphrase = pp;
      if (Weave.Service.login()) {
        this._updateStatus("change.recoverykey.success", "success");
        Weave.Service.persistLogin();
      }
      else {
        this._updateStatus("new.passphrase.status.incorrect", "error");
      }
    }
    else {
      this._updateStatus("change.recoverykey.label", "active");

      if (Weave.Service.changePassphrase(pp))
        this._updateStatus("change.recoverykey.success", "success");
      else
        this._updateStatus("change.recoverykey.error", "error");
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
      if (!this._updatingPassphrase)
        return;

      valid = this._passphraseBox.value != "";
    }

    if (errorString == "")
      this._clearStatus();
    else
      this._updateStatusWithString(errorString, "error");

    this._statusRow.hidden = valid;
    this._dialog.getButton("finish").disabled = !valid;
  },

  _str: function Change__string(str) {
    return this._stringBundle.GetStringFromName(str);
  }
};

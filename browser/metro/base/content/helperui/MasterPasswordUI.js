/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var MasterPasswordUI = {
  _dialog: null,
  _tokenName: "",

  get _secModuleDB() {
    delete this._secModuleDB;
    return this._secModuleDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].getService(Ci.nsIPKCS11ModuleDB);
  },

  get _pk11DB() {
    delete this._pk11DB;
    return this._pk11DB = Cc["@mozilla.org/security/pk11tokendb;1"].getService(Ci.nsIPK11TokenDB);
  },

  _setPassword: function _setPassword(password) {
    try {
      let status;
      let slot = this._secModuleDB.findSlotByName(this._tokenName);
      if (slot)
        status = slot.status;
      else
        return false;

      let token = this._pk11DB.findTokenByName(this._tokenName);

      if (status == Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED) {
        token.initPassword(password);
      } else if (status == Ci.nsIPKCS11Slot.SLOT_READY) {
        token.changePassword("", password);
      }
      return true;
    } catch(e) {
      dump("--- MasterPasswordUI._setPassword exception: " + e + "\n");
      return false;
    }
  },

  _removePassword: function _removePassword(password) {
    try {
      let token = this._pk11DB.getInternalKeyToken();
      if (token.checkPassword(password)) {
        token.changePassword(password, "");
        return true;
      }
    } catch(e) {
      dump("--- MasterPasswordUI._removePassword exception: " + e + "\n");
    }
    return false;
  },

  show: function mp_show(aSet) {
    let dialogId = aSet ? "masterpassword-change" : "masterpassword-remove";
    if (document.getElementById(dialogId))
      return;

    let dialog = aSet ? "chrome://browser/content/prompt/masterPassword.xul"
                        : "chrome://browser/content/prompt/removeMasterPassword.xul";
    this._dialog = DialogUI.importModal(window, dialog, null);
    DialogUI.pushPopup(this, this._dialog);

    if (aSet) {
      this.checkPassword();
      document.getElementById("masterpassword-newpassword1").focus();
    } else {
      document.getElementById("masterpassword-oldpassword").focus();
    }
  },

  hide: function mp_hide(aValue) {
    this.updatePreference();
    this._dialog.close();
    this._dialog = null;
    DialogUI.popPopup(this);
  },

  setPassword: function mp_setPassword() {
    if (!this.checkPassword())
      return;

    let newPasswordValue = document.getElementById("masterpassword-newpassword1").value;
    if (this._setPassword(newPasswordValue)) {
      this.hide();
    }
  },

  removePassword: function mp_removePassword() {
    let oldPassword = document.getElementById("masterpassword-oldpassword").value;
    if (this._removePassword(oldPassword)) {
      this.hide();
    }
  },

  checkPassword: function mp_checkPassword() {
    let newPasswordValue1 = document.getElementById("masterpassword-newpassword1").value;
    let newPasswordValue2 = document.getElementById("masterpassword-newpassword2").value;

    let buttonOk = this._dialog.getElementsByAttribute("class", "prompt-buttons")[0].firstChild;
    let isPasswordValid = this._secModuleDB.isFIPSEnabled ? (newPasswordValue1 != "" && newPasswordValue1 == newPasswordValue2)
                                                          : (newPasswordValue1 == newPasswordValue2);
    if (isPasswordValid) {
      buttonOk.removeAttribute("disabled");
    } else {
      buttonOk.setAttribute("disabled", true);
    }

    return isPasswordValid;
  },

  checkOldPassword: function mp_checkOldPassword() {
    let oldPassword = document.getElementById("masterpassword-oldpassword");

    let buttonOk = this._dialog.getElementsByAttribute("class", "prompt-buttons")[0].firstChild;
    let isPasswordValid = this._pk11DB.getInternalKeyToken().checkPassword(oldPassword.value);
    buttonOk.setAttribute("disabled", !isPasswordValid);
  },

  hasMasterPassword: function mp_hasMasterPassword() {
    let slot = this._secModuleDB.findSlotByName(this._tokenName);
    if (slot) {
      let status = slot.status;
      return status != Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED && status != Ci.nsIPKCS11Slot.SLOT_READY;
    }
    return false;
  },

  updatePreference: function mp_updatePreference() {
    document.getElementById("prefs-master-password").value = this.hasMasterPassword();
  }
};

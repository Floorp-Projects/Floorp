/**
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Thunderbird preferences
 *
 * The Initial Developer of the Original Code is
 * Scott MacGregor
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Matthew Willis <lilmatt@mozilla.com>
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
 * ***** END LICENSE BLOCK *****
 */

var gAdvancedPane = {
    _inited: false,
    init: function advPaneInit() {
        this._inited = true;
        this._initMasterPasswordUI();

        var advancedPrefs = document.getElementById("advancedPrefs");
        var preference = document.getElementById("calendar.preferences.advanced.selectedTabIndex");
        if (preference.value === null) {
            return;
        }
        advancedPrefs.selectedIndex = preference.value;
    },

    tabSelectionChanged: function advPaneTabSelectionChanged() {
        if (!this._inited) {
            return;
        }
        var advancedPrefs = document.getElementById("advancedPrefs");
        var preference = document.getElementById("calendar.preferences.advanced.selectedTabIndex");
        preference.valueFromPreferences = advancedPrefs.selectedIndex;
    },

    showConnections: function advPaneShowConnections() {
        var url = "chrome://calendar/content/preferences/connection.xul";
        document.documentElement.openSubDialog(url, "", "chrome,dialog");
    },

    showConfigEdit: function advPaneShowConfigEdit() {
        document.documentElement.openWindow("Preferences:ConfigManager",
                                            "chrome://global/content/config.xul",
                                            "", null);
    },

    /**
     * Caches the security module service for multiple use.
     */
    __secModDb: null,
    get _secModDb() {
        if (!this.__secModDb) {
            this.__secModDb =
                Components.classes["@mozilla.org/security/pkcs11moduledb;1"]
                          .getService(Components.interfaces.nsIPKCS11ModuleDB);
        }
        return this.__secModDb;
    },

    /**
     * Initializes Master Password UI: the "Use Master Password" checkbox,
     * selects the Master Password button to show, and enables/disables it as
     * necessary.  The Master Password is controlled by various bits of NSS
     * functionality, so the UI for it can't be controlled by the normal
     * preference bindings.
     */
    _initMasterPasswordUI: function advPaneInitMasterPassword() {
        var noMP = !this._masterPasswordSet();

        var button = document.getElementById("changeMasterPassword");
        button.disabled = noMP;

        var checkbox = document.getElementById("useMasterPassword");
        checkbox.checked = !noMP;
    },

    /**
     * Returns true if the user has a Master Password set and false otherwise.
     */
    _masterPasswordSet: function advPaneMasterPasswordSet() {
        var slot = this._secModDb.findSlotByName("");
        if (slot) {
            const nsIPKCS11Slot = Components.interfaces.nsIPKCS11Slot;
            var status = slot.status;
            // Does the user have a Master Password set?
            return ((status != nsIPKCS11Slot.SLOT_UNINITIALIZED) &&
                    (status != nsIPKCS11Slot.SLOT_READY));
        } else {
            return false;
        }
    },

    /**
     * Enables/disables the Master Password button depending on the state of
     * the "Use Master Password" checkbox, and prompts for Master Password
     * removal if one is set. This function is called when the "Use Master
     * Password" checkbox is changed.
     */
    updateMasterPasswordButton: function advPaneUpdateMasterPasswordButton() {
        var checkbox = document.getElementById("useMasterPassword");
        var button = document.getElementById("changeMasterPassword");
        button.disabled = !checkbox.checked;

        // Unchecking the checkbox should try to immediately remove the Master
        // Password, because it's impossible to non-destructively remove the
        // Master Password used to encrypt all the passwords without providing
        // it (by design), and it would be extremely odd to pop up that dialog
        // when the user closes the prefwindow and saves his settings.
        if (!checkbox.checked) {
            this._removeMasterPassword();
        } else {
            this.changeMasterPassword();
        }
    },

    /**
     * Displays the "Remove Master Password" dialog to allow the user to
     * remove the current Master Password.  When the dialog is dismissed,
     * the Master Password UI is automatically updated.
     */
    _removeMasterPassword: function advRemoveMasterPassword() {
        if (this._secModDb.isFIPSEnabled) {
            var bundle = document.getElementById("bundlePreferences");
            var promptSvc = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                                      .getService(Components.interfaces.nsIPromptService);
            promptSvc.alert(window,
                            bundle.getString("pw_change_failed_title"),
                            bundle.getString("pw_change2empty_in_fips_mode"));
        } else {
            var url = "chrome://mozapps/content/preferences/removemp.xul";
            document.documentElement.openSubDialog(url, "", null);

        }
        this._initMasterPasswordUI();
    },

    /**
     * Displays a dialog in which the Master Password may be changed.
     */
    changeMasterPassword: function advPaneChangeMasterPassword() {
        var url = "chrome://mozapps/content/preferences/changemp.xul";
        document.documentElement.openSubDialog(url, "", null);

        this._initMasterPasswordUI();
    },

    /**
     * Shows the sites where the user has saved passwords and the associated
     * login information.
     */
    viewPasswords: function advPaneViewPasswords() {
        var url = "chrome://passwordmgr/content/passwordManager.xul";
        document.documentElement.openWindow("Toolkit:PasswordManager", url,
                                            "", null);
    },

    /**
     * The Extensions checkbox and button are disabled only if the enable
     * Addon update preference is locked.
     */
    updateAddonUpdateUI: function advPaneUpdateAddonUpdateUI() {
      var enabledPref = document.getElementById("extensions.update.enabled");
      var enableAddonUpdate = document.getElementById("enableAddonUpdate");

      enableAddonUpdate.disabled = enabledPref.locked;
    }
};

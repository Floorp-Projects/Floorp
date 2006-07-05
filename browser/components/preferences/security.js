# -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Firefox Preferences System.
#
# The Initial Developer of the Original Code is
# Jeff Walden <jwalden+code@mit.edu>.
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

var gSecurityPane = {
  _pane: null,

  /**
   * Initializes master password UI.
   */
  init: function ()
  {
    this._pane = document.getElementById("paneSecurity");
    this._initMasterPasswordUI();
  },

  // ADD-ONS

  /*
   * Preferences:
   *
   * xpinstall.whitelist.required
   * - true if a site must be added to a site whitelist before extensions
   *   provided by the site may be installed from it, false if the extension
   *   may be directly installed after a confirmation dialog
   */

  /**
   * Enables/disables the add-ons Exceptions button depending on whether
   * or not add-on installation warnings are displayed.
   */
  readWarnAddonInstall: function ()
  {
    var warn = document.getElementById("xpinstall.whitelist.required");
    var exceptions = document.getElementById("addonExceptions");

    exceptions.disabled = !warn.value;

    // don't override the preference value
    return undefined;
  },

  /**
   * Displays the exceptions lists for add-on installation warnings.
   */
  showAddonExceptions: function ()
  {
    var bundlePrefs = document.getElementById("bundlePreferences");

    var params = this._addonParams;
    if (!params.windowTitle || !params.introText) {
      params.windowTitle = bundlePrefs.getString("installpermissionstitle");
      params.introText = bundlePrefs.getString("installpermissionstext");
    }

    document.documentElement.openWindow("Browser:Permissions",
                                        "chrome://browser/content/preferences/permissions.xul",
                                        "", params);
  },

  /**
   * Parameters for the add-on install permissions dialog.
   */
  _addonParams:
    {
      blockVisible: false,
      sessionVisible: false,
      allowVisible: true,
      prefilledHost: "",
      permissionType: "install"
    },

  // PHISHING

  /*
   * Preferences:
   *
   * browser.safebrowsing.enabled
   * - true if phishing checks of all visited sites are enabled, false if
   *   such checks are disabled
   * browser.safebrowsing.remoteLookups
   * - true if every site is checked against a remote phishing provider for
   *   safety on load, false if a cached list should be used instead
   */

  /**
   * Enables/disables the UI for the type of phishing detection used based on
   * whether phishing detection is enabled.
   */
  readCheckPhish: function ()
  {
    var phishEnabled = document.getElementById("browser.safebrowsing.enabled").value;
    var remoteLookup = document.getElementById("browser.safebrowsing.remoteLookups").value;

    var checkPhish = document.getElementById("checkPhishChoice");
    var cacheList = document.getElementById("cacheProvider");
    var loadList = document.getElementById("onloadProvider");
    var onloadAfter = document.getElementById("onloadAfter");

    checkPhish.disabled = onloadAfter.disabled = !phishEnabled;
    loadList.disabled = !phishEnabled || !remoteLookup;
    cacheList.disabled = !phishEnabled || remoteLookup;

    // don't override pref value
    return undefined;
  },

  /**
   * Enables and disables UI as necessary based on which type of phishing
   * detection is currently selected (and whether phishing is even enabled,
   * since this could trample on readCheckPhish during a preference update).
   */
  readPhishChoice: function ()
  {
    var phishPref = document.getElementById("browser.safebrowsing.enabled");
    var phishChoice = document.getElementById("browser.safebrowsing.remoteLookups");

    var cachedList = document.getElementById("cacheProvider");
    var onloadList = document.getElementById("onloadProvider");

    if (phishPref.value) {
      cachedList.disabled = phishChoice.value;
      onloadList.disabled = !phishChoice.value;
    }

    // don't override pref value
    return undefined;
  },

  /**
   * Populates the menulist of providers of cached phishing lists if the
   * menulist isn't already populated.
   */
  readCachedPhishProvider: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;
    const cachePopupId = "cachePhishPopup";
    var popup = document.getElementById(cachePopupId);

    if (!popup) {
      var providers = Cc["@mozilla.org/preferences-service;1"]
                        .getService(Ci.nsIPrefService)
                        .getBranch("browser.safebrowsing.provider.");

      // fill in onload phishing list data
      var kids = providers.getChildList("", {});
      for (var i = 0; i < kids.length; i++) {
        var curr = kids[i];
        var matches = curr.match(/^(\d+)\.name$/);

        // skip preferences not of form "##.name"
        if (!matches)
          continue;

        if (!popup) {
          popup = document.createElement("menupopup");
          popup.id = cachePopupId;
        }

        var providerNum = matches[1];
        var providerName = providers.getCharPref(curr);

        var item = document.createElement("menuitem");
        item.setAttribute("value", providerNum);
        item.setAttribute("label", providerName);

        popup.appendChild(item);
      }

      var onloadProviders = document.getElementById("cacheProvider");
      onloadProviders.appendChild(popup);
    }

    // don't override the preference value in determining the right menuitem
    return undefined;
  },

  /**
   * Populates the menulist of providers of cached phishing lists if the
   * menulist isn't already populated.
   */
  readOnloadPhishProvider: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;
    const onloadPopupId = "onloadPhishPopup";
    var popup = document.getElementById(onloadPopupId);

    if (!popup) {
      var providers = Cc["@mozilla.org/preferences-service;1"]
                        .getService(Ci.nsIPrefService)
                        .getBranch("browser.safebrowsing.provider.");

      // fill in onload phishing list data
      var kids = providers.getChildList("", {});
      for (var i = 0; i < kids.length; i++) {
        var curr = kids[i];
        var matches = curr.match(/^(\d+)\.name$/);

        // skip preferences not of form "##.name"
        if (!matches)
          continue;

        if (!popup) {
          popup = document.createElement("menupopup");
          popup.id = onloadPopupId;
        }

        var providerNum = matches[1];
        var providerName = providers.getCharPref(curr);

        var item = document.createElement("menuitem");
        item.setAttribute("value", providerNum);
        item.setAttribute("label", providerName);

        popup.appendChild(item);
      }

      var onloadProviders = document.getElementById("onloadProvider");
      onloadProviders.appendChild(popup);
    }

    // don't override the preference value in determining the right menuitem
    return undefined;
  },

  // PASSWORDS

  /*
   * Preferences:
   *
   * signon.rememberSignons
   * - true if passwords are remembered, false otherwise
   */

  /**
   * Enables/disables the Exceptions button used to configure sites where
   * passwords are never saved.
   */
  readSavePasswords: function ()
  {
    var pref = document.getElementById("signon.rememberSignons");
    var excepts = document.getElementById("passwordExceptions");

    excepts.disabled = !pref.value;

    // don't override pref value in UI
    return undefined;
  },

  /**
   * Displays a dialog in which the user can view and modify the list of sites
   * where passwords are never saved.
   */
  showPasswordExceptions: function ()
  {
    document.documentElement.openWindow("Toolkit:PasswordManagerExceptions",
                                        "chrome://passwordmgr/content/passwordManagerExceptions.xul",
                                        "", null);
  },

  /**
   * Initializes master password UI: the "use master password" checkbox, selects
   * the master password button to show, and enables/disables it as necessary.
   * The master password is controlled by various bits of NSS functionality, so
   * the UI for it can't be controlled by the normal preference bindings.
   */
  _initMasterPasswordUI: function ()
  {
    var noMP = !this._masterPasswordSet();

    var bundle = document.getElementById("bundlePreferences");
    var button = document.getElementById("masterPassword");
    button.label = bundle.getString(noMP ? "setMasterPassword" : "changeMasterPassword");
    button.setAttribute("accesskey",
                        bundle.getString(noMP ? "setMasterPassword_accesskey" : "changeMasterPassword_accesskey"));
    button.disabled = noMP;

    var checkbox = document.getElementById("useMasterPassword");
    checkbox.checked = !noMP;
  },

  /**
   * Returns true if the user has a master password set and false otherwise.
   */
  _masterPasswordSet: function ()
  {
    const Cc = Components.classes, Ci = Components.interfaces;
    var secmodDB = Cc["@mozilla.org/security/pkcs11moduledb;1"]
                     .getService(Ci.nsIPKCS11ModuleDB);
    var slot = secmodDB.findSlotByName("");
    if (slot) {
      var status = slot.status;
      var hasMP = status != Ci.nsIPKCS11Slot.SLOT_UNINITIALIZED &&
                  status != Ci.nsIPKCS11Slot.SLOT_READY;
      return hasMP;
    } else {
      // XXX I have no bloody idea what this means
      return false;
    }
  },

  /**
   * Enables/disables the master password button depending on the state of the
   * "use master password" checkbox, and prompts for master password removal if
   * one is set.
   */
  updateMasterPasswordButton: function ()
  {
    var checkbox = document.getElementById("useMasterPassword");
    var button = document.getElementById("masterPassword");
    button.disabled = !checkbox.checked;

    // in instant-apply mode, we'd like flipping this to try to immediately
    // remove the master password
    if (!checkbox.checked &&
        this._masterPasswordSet()) {
      this._removeMasterPassword();
      this._initMasterPasswordUI();
    }
  },

  /**
   * Displays the "remove master password" dialog to allow the user to remove
   * the current master password.
   */
  _removeMasterPassword: function ()
  {
    var secmodDB = Components.classes["@mozilla.org/security/pkcs11moduledb;1"]
                              .getService(Components.interfaces.nsIPKCS11ModuleDB);
    if (secmodDB.isFIPSEnabled) {
      var bundle = document.getElementById("bundlePreferences");
      promptService.alert(window,
                          bundle.getString("pw_change_failed_title"),
                          bundle.getString("pw_change2empty_in_fips_mode"));
    }
    else {
      document.documentElement.openSubDialog("chrome://mozapps/content/preferences/removemp.xul",
                                             "", null);
    }
  },

  /**
   * Displays a dialog in which the master password may be changed.
   */
  changeMasterPassword: function ()
  {
    document.documentElement.openSubDialog("chrome://mozapps/content/preferences/changemp.xul",
                                           "", null);
    this._initMasterPasswordUI();
  },

  /**
   * Shows the sites where the user has saved passwords and the associated login
   * information.
   */
  showPasswords: function ()
  {
    document.documentElement.openWindow("Toolkit:PasswordManager",
                                        "chrome://passwordmgr/content/passwordManager.xul",
                                        "", null);
  },


  // WARNING MESSAGES

  /**
   * Displays the security warnings dialog which allows changing the
   * "submitting unencrypted information", "moving from secure to unsecure",
   * etc. dialogs that every user immediately disables when he sees them.
   */
  showWarningMessageSettings: function ()
  {
    document.documentElement.openSubDialog("chrome://browser/content/preferences/securityWarnings.xul",
                                           "", null);
  }

};

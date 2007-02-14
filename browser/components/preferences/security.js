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
#   Ryan Flint <rflint@dslr.net>
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
      params.windowTitle = bundlePrefs.getString("addons_permissions_title");
      params.introText = bundlePrefs.getString("addonspermissionstext");
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
   * browser.safebrowsing.dataProvider
   * - integer identifying the current anti-phishing provider in use
   * browser.safebrowsing.provider.<number>.<property>
   * - identifies each installed Safe Browsing provider; the provider's name is
   *   stored in the "name" property, and the various URLs used in Safe Browsing
   *   detection comprise the values of the rest of the properties
   */

  /**
   * Enables/disables the UI for the type of phishing detection used based on
   * whether phishing detection is enabled.
   */
  readCheckPhish: function ()
  {
    var phishEnabled = document.getElementById("browser.safebrowsing.enabled").value;

    var checkPhish = document.getElementById("checkPhishChoice");
    var loadList = document.getElementById("onloadProvider");
    var onloadAfter = document.getElementById("onloadAfter");

    checkPhish.disabled = onloadAfter.disabled = !phishEnabled;
    loadList.disabled = !phishEnabled;

    // don't override pref value
    return undefined;
  },

  /**
   * Displays the currently-used phishing provider's EULA and offers the user
   * the choice of cancelling the enabling of phishing, but only if the user has
   * not previously agreed to the provider's EULA before.
   *
   * @param   providerNum
   *          the number of the provider whose policy should be displayed
   * @returns bool
   *          true if the user still wants to enable phishing protection with
   *          the current provider, false otherwise
   */
  _userAgreedToPhishingEULA: function (providerNum)
  {
    // create the opt-in preference element for the provider
    const prefName = "browser.safebrowsing.provider." +
                     providerNum +
                     ".privacy.optedIn";
    var pref = document.getElementById(prefName);

    if (!pref) {
      pref = document.createElement("preference");
      pref.setAttribute("type", "bool");
      pref.id = prefName;
      pref.setAttribute("name", prefName);
      document.getElementById("securityPreferences").appendChild(pref);
    }

    // only show privacy policy if it hasn't already been shown or the user
    // hasn't agreed to it
    if (!pref.value) {
      var rv = { userAgreed: false, providerNum: providerNum };
      document.documentElement.openSubDialog("chrome://browser/content/preferences/phishEULA.xul",
                                             "resizable", rv);

      // mark this provider as having had its privacy policy accepted if it was
      if (rv.userAgreed)
        pref.value = true;

      return rv.userAgreed;
    }

    // user has previously agreed
    return true;
  },

  /**
   * Displays a privacy policy if the user enables onload anti-phishing
   * checking. The policy must be accepted if onload checking is to be enabled,
   * and if it isn't we revert to downloaded list-based checking.
   */
  writePhishChoice: function ()
  {
    var radio = document.getElementById("checkPhishChoice");
    var provider = document.getElementById("browser.safebrowsing.dataProvider");

    // display a privacy policy if onload checking is being enabled
    if (radio.value == "true" &&
        !this._userAgreedToPhishingEULA(provider.value)) {
      radio.value = "false";
      return false;
    }

    // don't override pref value
    return undefined;
  },

  /**
   * Ensures that the user has agreed to the selected provider's privacy policy
   * if safe browsing is enabled.
   */
  onSBChange: function ()
  {
    var phishEnabled = document.getElementById("browser.safebrowsing.enabled").value;
    var remoteLookup = document.getElementById("browser.safebrowsing.remoteLookups");
    var providerNum = document.getElementById("onloadProvider").value;

    if (phishEnabled && remoteLookup.value &&
        !this._userAgreedToPhishingEULA(providerNum))
      remoteLookup.value = false;
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
      var providerBranch = Cc["@mozilla.org/preferences-service;1"]
                             .getService(Ci.nsIPrefService)
                             .getBranch("browser.safebrowsing.provider.");

      // fill in onload phishing list data -- but require a privacy policy
      // URL be provided, and require it to be at a chrome URL so it's always
      // available
      var kids = providerBranch.getChildList("", {});
      var providers = [];
      var hasPrivacyPolicy = {};
      for (var i = 0; i < kids.length; i++) {
        var curr = kids[i];
        var matchesName = curr.match(/^(\d+)\.name$/);
        var matchesPolicy = curr.match(/^(\d+)\.privacy\.url$/);

        // skip preferences which aren't names or privacy URLs
        if (!matchesName && !matchesPolicy)
          continue;

        if (matchesName)
          providers.push(matchesName[1]);
        else
          hasPrivacyPolicy[matchesPolicy[1]] = true;
      }

      // construct the menu only from the providers with policies
      for (var i = 0; i < providers.length; i++) {
        // skip providers without a privacy policy
        if (!(providers[i] in hasPrivacyPolicy))
          continue;

        // ensure privacy URL is a chrome URL
        try {
          var providerNum = providers[i];
          var url = providerBranch.getCharPref(providerNum + ".privacy.url");
          var fallbackurl = providerBranch.getCharPref(providerNum +
                                                       ".privacy.fallbackurl");
          var scheme = Cc["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService).
                       extractScheme(fallbackurl);
          if (scheme != "chrome")
            throw "fallbackurl scheme must be chrome";
        }
        catch (e) {
          // don't add this provider
          continue;
        }

        if (!popup) {
          popup = document.createElement("menupopup");
          popup.id = onloadPopupId;
        }

        var providerName = providerBranch.getCharPref(providerNum + ".name");

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

  /**
   * Requires that the user agree to the new phishing provider's EULA when the
   * provider is changed, disabling protection if the user doesn't agree.
   */
  onProviderChanged: function ()
  {
    var pref = document.getElementById("browser.safebrowsing.dataProvider");
    var remoteLookup = document.getElementById("browser.safebrowsing.remoteLookups");

    remoteLookup.value = this._userAgreedToPhishingEULA(pref.value);
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

    var button = document.getElementById("changeMasterPassword");
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
    var button = document.getElementById("changeMasterPassword");
    button.disabled = !checkbox.checked;

    // unchecking the checkbox should try to immediately remove the master
    // password, because it's impossible to non-destructively remove the master
    // password used to encrypt all the passwords without providing it (by
    // design), and it would be extremely odd to pop up that dialog when the
    // user closes the prefwindow and saves his settings
    if (!checkbox.checked)
      this._removeMasterPassword();
    else
      this.changeMasterPassword();

    this._initMasterPasswordUI();
  },

  /**
   * Displays the "remove master password" dialog to allow the user to remove
   * the current master password.  When the dialog is dismissed, master password
   * UI is automatically updated.
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
    this._initMasterPasswordUI();
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

XPCOMUtils.defineLazyModuleGetter(this, "LoginHelper",
 "resource://gre/modules/LoginHelper.jsm");

Components.utils.import("resource://gre/modules/PrivateBrowsingUtils.jsm");

var gSecurityPane = {
  _pane: null,

  /**
   * Initializes master password UI.
   */
  init()
  {
    function setEventListener(aId, aEventType, aCallback)
    {
      document.getElementById(aId)
              .addEventListener(aEventType, aCallback.bind(gSecurityPane));
    }

    this._pane = document.getElementById("paneSecurity");
    this._initMasterPasswordUI();
    this._initSafeBrowsing();

    setEventListener("addonExceptions", "command",
      gSecurityPane.showAddonExceptions);
    setEventListener("passwordExceptions", "command",
      gSecurityPane.showPasswordExceptions);
    setEventListener("useMasterPassword", "command",
      gSecurityPane.updateMasterPasswordButton);
    setEventListener("changeMasterPassword", "command",
      gSecurityPane.changeMasterPassword);
    setEventListener("showPasswords", "command",
      gSecurityPane.showPasswords);
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
  readWarnAddonInstall()
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
  showAddonExceptions()
  {
    var bundlePrefs = document.getElementById("bundlePreferences");

    var params = this._addonParams;
    if (!params.windowTitle || !params.introText) {
      params.windowTitle = bundlePrefs.getString("addons_permissions_title");
      params.introText = bundlePrefs.getString("addonspermissionstext");
    }

    gSubDialog.open("chrome://browser/content/preferences/permissions.xul",
                    null, params);
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

  // PASSWORDS

  /*
   * Preferences:
   *
   * signon.rememberSignons
   * - true if passwords are remembered, false otherwise
   */

  /**
   * Enables/disables the Exceptions button used to configure sites where
   * passwords are never saved. When browser is set to start in Private
   * Browsing mode, the "Remember passwords" UI is useless, so we disable it.
   */
  readSavePasswords()
  {
    var pref = document.getElementById("signon.rememberSignons");
    var excepts = document.getElementById("passwordExceptions");

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      document.getElementById("savePasswords").disabled = true;
      excepts.disabled = true;
      return false;
    }
    excepts.disabled = !pref.value;
    // don't override pref value in UI
    return undefined;
  },

  /**
   * Displays a dialog in which the user can view and modify the list of sites
   * where passwords are never saved.
   */
  showPasswordExceptions()
  {
    var bundlePrefs = document.getElementById("bundlePreferences");
    var params = {
      blockVisible: true,
      sessionVisible: false,
      allowVisible: false,
      hideStatusColumn: true,
      prefilledHost: "",
      permissionType: "login-saving",
      windowTitle: bundlePrefs.getString("savedLoginsExceptions_title"),
      introText: bundlePrefs.getString("savedLoginsExceptions_desc")
    };

    gSubDialog.open("chrome://browser/content/preferences/permissions.xul",
                    null, params);
  },

  /**
   * Initializes master password UI: the "use master password" checkbox, selects
   * the master password button to show, and enables/disables it as necessary.
   * The master password is controlled by various bits of NSS functionality, so
   * the UI for it can't be controlled by the normal preference bindings.
   */
  _initMasterPasswordUI()
  {
    var noMP = !LoginHelper.isMasterPasswordSet();

    var button = document.getElementById("changeMasterPassword");
    button.disabled = noMP;

    var checkbox = document.getElementById("useMasterPassword");
    checkbox.checked = !noMP;
  },

  _initSafeBrowsing() {
    let enableSafeBrowsing = document.getElementById("enableSafeBrowsing");
    let blockDownloads = document.getElementById("blockDownloads");
    let blockUncommonUnwanted = document.getElementById("blockUncommonUnwanted");

    let safeBrowsingPhishingPref = document.getElementById("browser.safebrowsing.phishing.enabled");
    let safeBrowsingMalwarePref = document.getElementById("browser.safebrowsing.malware.enabled");

    let blockDownloadsPref = document.getElementById("browser.safebrowsing.downloads.enabled");
    let malwareTable = document.getElementById("urlclassifier.malwareTable");

    let blockUnwantedPref = document.getElementById("browser.safebrowsing.downloads.remote.block_potentially_unwanted");
    let blockUncommonPref = document.getElementById("browser.safebrowsing.downloads.remote.block_uncommon");

    enableSafeBrowsing.addEventListener("command", function() {
      safeBrowsingPhishingPref.value = enableSafeBrowsing.checked;
      safeBrowsingMalwarePref.value = enableSafeBrowsing.checked;

      if (enableSafeBrowsing.checked) {
        blockDownloads.removeAttribute("disabled");
        if (blockDownloads.checked) {
          blockUncommonUnwanted.removeAttribute("disabled");
        }
      } else {
        blockDownloads.setAttribute("disabled", "true");
        blockUncommonUnwanted.setAttribute("disabled", "true");
      }
    });

    blockDownloads.addEventListener("command", function() {
      blockDownloadsPref.value = blockDownloads.checked;
      if (blockDownloads.checked) {
        blockUncommonUnwanted.removeAttribute("disabled");
      } else {
        blockUncommonUnwanted.setAttribute("disabled", "true");
      }
    });

    blockUncommonUnwanted.addEventListener("command", function() {
      blockUnwantedPref.value = blockUncommonUnwanted.checked;
      blockUncommonPref.value = blockUncommonUnwanted.checked;

      let malware = malwareTable.value
        .split(",")
        .filter(x => x !== "goog-unwanted-shavar" && x !== "test-unwanted-simple");

      if (blockUncommonUnwanted.checked) {
        malware.push("goog-unwanted-shavar");
        malware.push("test-unwanted-simple");
      }

      // sort alphabetically to keep the pref consistent
      malware.sort();

      malwareTable.value = malware.join(",");
    });

    // set initial values

    enableSafeBrowsing.checked = safeBrowsingPhishingPref.value && safeBrowsingMalwarePref.value;
    if (!enableSafeBrowsing.checked) {
      blockDownloads.setAttribute("disabled", "true");
      blockUncommonUnwanted.setAttribute("disabled", "true");
    }

    blockDownloads.checked = blockDownloadsPref.value;
    if (!blockDownloadsPref.value) {
      blockUncommonUnwanted.setAttribute("disabled", "true");
    }

    blockUncommonUnwanted.checked = blockUnwantedPref.value && blockUncommonPref.value;
  },

  /**
   * Enables/disables the master password button depending on the state of the
   * "use master password" checkbox, and prompts for master password removal if
   * one is set.
   */
  updateMasterPasswordButton()
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
  _removeMasterPassword()
  {
    var secmodDB = Cc["@mozilla.org/security/pkcs11moduledb;1"].
                   getService(Ci.nsIPKCS11ModuleDB);
    if (secmodDB.isFIPSEnabled) {
      var promptService = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                          getService(Ci.nsIPromptService);
      var bundle = document.getElementById("bundlePreferences");
      promptService.alert(window,
                          bundle.getString("pw_change_failed_title"),
                          bundle.getString("pw_change2empty_in_fips_mode"));
      this._initMasterPasswordUI();
    }
    else {
      gSubDialog.open("chrome://mozapps/content/preferences/removemp.xul",
                      null, null, this._initMasterPasswordUI.bind(this));
    }
  },

  /**
   * Displays a dialog in which the master password may be changed.
   */
  changeMasterPassword()
  {
    gSubDialog.open("chrome://mozapps/content/preferences/changemp.xul",
                    "resizable=no", null, this._initMasterPasswordUI.bind(this));
  },

  /**
   * Shows the sites where the user has saved passwords and the associated login
   * information.
   */
  showPasswords()
  {
    gSubDialog.open("chrome://passwordmgr/content/passwordManager.xul");
  }

};

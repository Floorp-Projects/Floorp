/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The installer prefs component provides a way to get a specific set of prefs
 * from a profile into a place where the installer can read them. The primary
 * reason for wanting to do this is so we can run things like Shield studies
 * on installer features; normally those are enabled by setting a pref, but
 * the installer runs outside of any profile and so has no access to prefs.
 * So we need to do something else to allow it to read these prefs.
 *
 * The mechanism we use here is to reflect the values of a list of relevant
 * prefs into registry values. One registry value is created for each pref
 * that is set. Each installation of the product gets its own registry key
 * (based on the path hash). This is obviously a somewhat wider scope than a
 * single profile, but it should be close enough in enough cases to suit our
 * purposes here.
 *
 * Currently this module only supports bool prefs. Other types could likely
 * be added if needed, but it doesn't seem necessary for the primary use case.
 */

// All prefs processed through this component must be in this branch.
const INSTALLER_PREFS_BRANCH = "installer.";

// This is the list of prefs that will be reflected to the registry. It should
// be kept up to date so that it reflects the list of prefs that are in
// current use (e.g., currently active experiments).
// Only add prefs to this list which are in INSTALLER_PREFS_BRANCH;
// any others will be ignored.
const INSTALLER_PREFS_LIST = ["installer.taskbarpin.win10.enabled"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

// This constructor can take a list of prefs to override the default one,
// but this is really only intended for tests to use. Normal usage should be
// to leave the parameter omitted/undefined.
function InstallerPrefs(prefsList) {
  this.prefsList = prefsList || INSTALLER_PREFS_LIST;

  // Each pref to be reflected will get a value created under this key, in HKCU.
  // The path will look something like:
  // "Software\Mozilla\Firefox\Installer\71AE18FE3142402B\".
  XPCOMUtils.defineLazyGetter(this, "_registryKeyPath", function() {
    const app = AppConstants.MOZ_APP_NAME;
    const vendor = Services.appinfo.vendor || "Mozilla";
    const xreDirProvider = Cc[
      "@mozilla.org/xre/directory-provider;1"
    ].getService(Ci.nsIXREDirProvider);
    const installHash = xreDirProvider.getInstallHash();
    return `Software\\${vendor}\\${app}\\Installer\\${installHash}`;
  });
}

InstallerPrefs.prototype = {
  classID: Components.ID("{cd8a6995-1f19-4cdd-9ed1-d6263302f594}"),
  contractID: "@mozilla.org/installerprefs;1",

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  observe(subject, topic, data) {
    switch (topic) {
      case "profile-after-change": {
        if (
          AppConstants.platform != "win" ||
          !this.prefsList ||
          !this.prefsList.length
        ) {
          // This module has no work to do.
          break;
        }
        const regKey = this._openRegKey();
        this._reflectPrefsToRegistry(regKey);
        this._registerPrefListeners();
        regKey.close();
        break;
      }
      case "nsPref:changed": {
        const regKey = this._openRegKey();
        if (this.prefsList.includes(data)) {
          this._reflectOnePrefToRegistry(regKey, data);
        }
        regKey.close();
        break;
      }
    }
  },

  _registerPrefListeners() {
    Services.prefs.addObserver(INSTALLER_PREFS_BRANCH, this);
  },

  _cleanRegistryKey(regKey) {
    for (let i = regKey.valueCount - 1; i >= 0; --i) {
      const name = regKey.getValueName(i);
      if (name.startsWith(INSTALLER_PREFS_BRANCH)) {
        regKey.removeValue(name);
      }
    }
  },

  _reflectPrefsToRegistry(regKey) {
    this._cleanRegistryKey(regKey);
    this.prefsList.forEach(pref =>
      this._reflectOnePrefToRegistry(regKey, pref)
    );
  },

  _reflectOnePrefToRegistry(regKey, pref) {
    if (!pref.startsWith(INSTALLER_PREFS_BRANCH)) {
      return;
    }

    const value = Services.prefs.getBoolPref(pref, false);
    if (value) {
      regKey.writeIntValue(pref, 1);
    } else {
      try {
        regKey.removeValue(pref);
      } catch (ex) {
        // This removeValue call is prone to failing because the value we
        // tried to remove didn't exist. Obviously that isn't really an error
        // that we need to handle.
      }
    }
  },

  _openRegKey() {
    const key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    key.create(
      key.ROOT_KEY_CURRENT_USER,
      this._registryKeyPath,
      key.ACCESS_READ | key.ACCESS_WRITE | key.WOW64_64
    );
    return key;
  },
};

var EXPORTED_SYMBOLS = ["InstallerPrefs"];

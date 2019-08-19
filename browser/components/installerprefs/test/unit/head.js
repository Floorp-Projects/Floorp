/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const { InstallerPrefs } = ChromeUtils.import(
  "resource:///modules/InstallerPrefs.jsm"
);

let gRegistryKeyPath = "";

function startModule(prefsList) {
  // Construct an InstallerPrefs object and simulate a profile-after-change
  // event on it, so that it performs its full startup procedure.
  const prefsModule = new InstallerPrefs(prefsList);
  prefsModule.observe(null, "profile-after-change", "");

  gRegistryKeyPath = prefsModule._registryKeyPath;

  registerCleanupFunction(() => cleanupReflectedPrefs(prefsList));
}

function getRegistryKey() {
  const key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  key.open(
    key.ROOT_KEY_CURRENT_USER,
    gRegistryKeyPath,
    key.ACCESS_READ | key.WOW64_64
  );
  return key;
}

function verifyReflectedPrefs(prefsList) {
  let key;
  try {
    key = getRegistryKey();
  } catch (ex) {
    Assert.ok(false, `Failed to open registry key: ${ex}`);
    return;
  }

  for (const pref of prefsList) {
    if (pref.startsWith("installer.")) {
      if (Services.prefs.getPrefType(pref) != Services.prefs.PREF_BOOL) {
        Assert.ok(
          !key.hasValue(pref),
          `Pref ${pref} should not be in the registry because its type is not bool`
        );
      } else if (Services.prefs.getBoolPref(pref, false)) {
        Assert.ok(key.hasValue(pref), `Pref ${pref} should be in the registry`);
        Assert.equal(
          key.getValueType(pref),
          key.TYPE_INT,
          `Pref ${pref} should be type DWORD`
        );
        Assert.equal(
          key.readIntValue(pref),
          1,
          `Pref ${pref} should have value 1`
        );
      } else {
        Assert.ok(
          !key.hasValue(pref),
          `Pref ${pref} should not be in the registry because it is false`
        );
      }
    } else {
      Assert.ok(
        !key.hasValue(pref),
        `Pref ${pref} should not be in the registry because its name is invalid`
      );
    }
  }

  key.close();
}

function cleanupReflectedPrefs(prefsList) {
  // Clear out the prefs themselves.
  prefsList.forEach(pref => Services.prefs.clearUserPref(pref));

  // Get the registry key path without the path hash at the end,
  // then delete the subkey with the path hash.
  const app = AppConstants.MOZ_APP_NAME;
  const vendor = Services.appinfo.vendor || "Mozilla";
  const xreDirProvider = Cc["@mozilla.org/xre/directory-provider;1"].getService(
    Ci.nsIXREDirProvider
  );

  const path = `Software\\${vendor}\\${app}\\Installer`;

  const key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
    Ci.nsIWindowsRegKey
  );
  try {
    key.open(
      key.ROOT_KEY_CURRENT_USER,
      path,
      key.ACCESS_READ | key.ACCESS_WRITE | key.WOW64_64
    );
    const installHash = xreDirProvider.getInstallHash();
    key.removeChild(installHash);
  } catch (ex) {
    // Nothing left to clean up.
    return;
  }

  // If the Installer key is now empty, we need to clean it up also, because
  // that would mean that this test created it.
  if (key.childCount == 0) {
    // Unfortunately we can't delete the actual open key, so we'll have to
    // open its parent and delete the one we're after as a child.
    key.close();
    const parentKey = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
      Ci.nsIWindowsRegKey
    );
    try {
      parentKey.open(
        parentKey.ROOT_KEY_CURRENT_USER,
        `Software\\${vendor}\\${app}`,
        parentKey.ACCESS_READ | parentKey.ACCESS_WRITE | parentKey.WOW64_64
      );
      parentKey.removeChild("Installer");
      parentKey.close();
    } catch (ex) {
      // Nothing we can do, and this isn't worth failing the test over.
    }
  } else {
    key.close();
  }
}

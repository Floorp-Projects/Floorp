/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unsafe-finally: "off"*/

function run_test() {
  let prefs = Services.prefs.getBranch(null);

  let greD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let defaultPrefD = Services.dirsvc.get("PrfDef", Ci.nsIFile);
  let testDir = do_get_cwd();

  try {
    let autoConfigJS = testDir.clone();
    autoConfigJS.append("autoconfig-no-sandbox.js");
    autoConfigJS.copyTo(defaultPrefD, "autoconfig.js");

    // Make sure nsReadConfig is initialized.
    Cc["@mozilla.org/readconfig;1"].getService(Ci.nsISupports);
    Services.prefs.resetPrefs();

    let autoConfigCfg = testDir.clone();
    autoConfigCfg.append("autoconfig-no-sandbox-check.cfg");
    autoConfigCfg.copyTo(greD, "autoconfig.cfg");

    Services.obs.notifyObservers(
      Services.prefs,
      "prefservice:before-read-userprefs"
    );

    equal("object", prefs.getStringPref("_test.typeof_Components"));
    equal("object", prefs.getStringPref("_test.typeof_ChromeUtils"));
    equal("object", prefs.getStringPref("_test.typeof_Services"));

    Services.prefs.resetPrefs();
  } finally {
    try {
      let autoConfigJS = defaultPrefD.clone();
      autoConfigJS.append("autoconfig.js");
      autoConfigJS.remove(false);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw e;
      }
    }

    try {
      let autoConfigCfg = greD.clone();
      autoConfigCfg.append("autoconfig.cfg");
      autoConfigCfg.remove(false);
    } catch (e) {
      if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
        throw e;
      }
    }

    Services.prefs.resetPrefs();
  }
}

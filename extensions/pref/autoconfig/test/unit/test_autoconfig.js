/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unsafe-finally: "off"*/
/* Turning off this rule to allow control flow operations in finally block
 * http://eslint.org/docs/rules/no-unsafe-finally  */

function run_test() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  let prefs = Services.prefs.getBranch(null);
  let defPrefs = Services.prefs.getDefaultBranch(null);

  let greD = Services.dirsvc.get("GreD", Ci.nsIFile);
  let defaultPrefD = Services.dirsvc.get("PrfDef", Ci.nsIFile);
  let testDir = do_get_cwd();

  try {
    let autoConfigJS = testDir.clone();
    autoConfigJS.append("autoconfig.js");
    autoConfigJS.copyTo(defaultPrefD, "autoconfig.js");

    // Make sure nsReadConfig is initialized.
    Cc["@mozilla.org/readconfig;1"].getService(Ci.nsISupports);
    Services.prefs.resetPrefs();

    let autoConfigCfg = testDir.clone();
    autoConfigCfg.append("autoconfig-all.cfg");
    autoConfigCfg.copyTo(greD, "autoconfig.cfg");

    env.set("AUTOCONFIG_TEST_GETENV", "getenv");

    Services.obs.notifyObservers(
      Services.prefs,
      "prefservice:before-read-userprefs"
    );

    ok(prefs.prefHasUserValue("_autoconfig_.test.userpref"));
    equal("userpref", prefs.getStringPref("_autoconfig_.test.userpref"));

    equal(
      "defaultpref",
      defPrefs.getStringPref("_autoconfig_.test.defaultpref")
    );
    equal("defaultpref", prefs.getStringPref("_autoconfig_.test.defaultpref"));

    ok(prefs.prefIsLocked("_autoconfig_.test.lockpref"));
    equal("lockpref", prefs.getStringPref("_autoconfig_.test.lockpref"));

    ok(!prefs.prefIsLocked("_autoconfig_.test.unlockpref"));
    equal("unlockpref", prefs.getStringPref("_autoconfig_.test.unlockpref"));

    ok(!prefs.prefHasUserValue("_autoconfig_.test.clearpref"));

    equal("getpref", prefs.getStringPref("_autoconfig_.test.getpref"));

    equal("getenv", prefs.getStringPref("_autoconfig_.test.getenv"));

    equal("function", prefs.getStringPref("_autoconfig_.test.displayerror"));

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

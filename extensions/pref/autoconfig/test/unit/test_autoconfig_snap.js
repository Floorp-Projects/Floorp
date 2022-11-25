/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* eslint no-unsafe-finally: "off"*/
/* Turning off this rule to allow control flow operations in finally block
 * http://eslint.org/docs/rules/no-unsafe-finally  */

const { updateAppInfo } = ChromeUtils.importESModule(
  "resource://testing-common/AppInfo.sys.mjs"
);

function ensureRemove(file) {
  try {
    file.remove(false);
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FILE_NOT_FOUND) {
      throw e;
    }
  }
}

async function run_test() {
  let prefs = Services.prefs.getBranch(null);

  let testDir = do_get_cwd();
  let confDir = testDir.clone();
  confDir.append("MozSystemConfigDir");
  Services.env.set("MOZ_SYSTEM_CONFIG_DIR", confDir.path);
  Services.env.set("SNAP_INSTANCE_NAME", "xpcshell");

  updateAppInfo();

  let sysConfD = Services.dirsvc.get("SysConfD", Ci.nsIFile);

  let defaultPrefDExtra = sysConfD.clone();
  defaultPrefDExtra.append("defaults");
  defaultPrefDExtra.append("pref");

  await IOUtils.makeDirectory(defaultPrefDExtra.path);

  const kAutoConfigFile = defaultPrefDExtra.clone();
  kAutoConfigFile.append("autoconfig_snap.js");
  const kAutoConfigCfg = sysConfD.clone();
  kAutoConfigCfg.append("autoconfig-snap.cfg");

  let autoConfigJS = testDir.clone();
  autoConfigJS.append(kAutoConfigFile.leafName);

  let autoConfigCfg = testDir.clone();
  autoConfigCfg.append(kAutoConfigCfg.leafName);

  try {
    autoConfigJS.copyTo(kAutoConfigFile.parent, kAutoConfigFile.leafName);
    autoConfigCfg.copyTo(kAutoConfigCfg.parent, kAutoConfigCfg.leafName);

    // Make sure nsReadConfig is initialized.
    Cc["@mozilla.org/readconfig;1"].getService(Ci.nsISupports);
    Services.prefs.resetPrefs();

    Services.obs.notifyObservers(
      Services.prefs,
      "prefservice:before-read-userprefs"
    );

    ok(prefs.prefHasUserValue("_autoconfig_.test.userpref-snap"));
    equal(
      "userpref-snap",
      prefs.getStringPref("_autoconfig_.test.userpref-snap")
    );

    Services.prefs.resetPrefs();
  } finally {
    ensureRemove(kAutoConfigFile);
    ensureRemove(kAutoConfigCfg);
    Services.prefs.resetPrefs();
  }
}

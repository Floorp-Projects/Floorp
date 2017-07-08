/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

var {classes: Cc, interfaces: Ci, results: Cr} = Components;

/* eslint no-unsafe-finally: "off"*/
/* Turning off this rule to allow control flow operations in finally block
 * http://eslint.org/docs/rules/no-unsafe-finally  */
function run_test() {
  let dirSvc = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties);
  let obsvc = Cc["@mozilla.org/observer-service;1"].
              getService(Ci.nsIObserverService);
  let ps = Cc["@mozilla.org/preferences-service;1"].
           getService(Ci.nsIPrefService);
  let prefs = ps.getBranch(null);

  let greD = dirSvc.get("GreD", Ci.nsIFile);
  let defaultPrefD = dirSvc.get("PrfDef", Ci.nsIFile);
  let testDir = do_get_cwd();

  try {
    let autoConfigJS = testDir.clone();
    autoConfigJS.append("autoconfig.js");
    autoConfigJS.copyTo(defaultPrefD, "autoconfig.js");

    // Make sure nsReadConfig is initialized.
    Cc["@mozilla.org/readconfig;1"].getService(Ci.nsISupports);
    ps.resetPrefs();

    var tests = [{
      filename: "autoconfig-utf8.cfg",
      prefs: {
        "_test.string.ASCII": "UTF-8",
        "_test.string.non-ASCII": "日本語",
        "_test.string.getPref": "日本語",
        "_test.string.gIsUTF8": "true"
      }
    }, {
      filename: "autoconfig-latin1.cfg",
      prefs: {
        "_test.string.ASCII": "ASCII",
        "_test.string.non-ASCII": "日本語",
        "_test.string.getPref": "日本語",
        "_test.string.gIsUTF8": "false",
      }
    }];

    function testAutoConfig(test) {
      // Make sure pref values are unset.
      for (let prefName in test.prefs) {
        do_check_eq(Ci.nsIPrefBranch.PREF_INVALID, prefs.getPrefType(prefName));
      }

      let autoConfigCfg = testDir.clone();
      autoConfigCfg.append(test.filename);
      autoConfigCfg.copyTo(greD, "autoconfig.cfg");

      obsvc.notifyObservers(ps, "prefservice:before-read-userprefs");

      for (let prefName in test.prefs) {
        do_check_eq(test.prefs[prefName],
                    prefs.getStringPref(prefName));
      }

      ps.resetPrefs();
      // Make sure pref values are reset.
      for (let prefName in test.prefs) {
        do_check_eq(Ci.nsIPrefBranch.PREF_INVALID, prefs.getPrefType(prefName));
      }
    }

    tests.forEach(testAutoConfig);

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

    ps.resetPrefs();
  }
}

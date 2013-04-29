/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug848297.manifest")
];

// Stub in the locale service so we can control what gets returned as the OS locale setting
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);
chromeReg.checkForNewChrome();

var prefService = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService)
                  .QueryInterface(Ci.nsIPrefBranch);

function enum_to_array(strings) {
  let rv = [];
  while (strings.hasMore()) {
    rv.push(strings.getNext());
  }
  rv.sort();
  return rv;
}

function run_test() {

  // without override
  prefService.setCharPref("general.useragent.locale", "de");
  do_check_eq(chromeReg.getSelectedLocale("basepack"), "en-US");
  do_check_eq(chromeReg.getSelectedLocale("overpack"), "de");
  do_check_matches(enum_to_array(chromeReg.getLocalesForPackage("basepack")),
                   ['en-US', 'fr']);

  // with override
  prefService.setCharPref("chrome.override_package.basepack", "overpack");
  do_check_eq(chromeReg.getSelectedLocale("basepack"), "de");
  do_check_matches(enum_to_array(chromeReg.getLocalesForPackage("basepack")),
                   ['de', 'en-US']);

}

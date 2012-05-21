/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug519468.manifest")
];

do_test_pending()
registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);
chromeReg.checkForNewChrome();

var localeService = Cc["@mozilla.org/intl/nslocaleservice;1"]
                    .getService(Ci.nsILocaleService);

var prefService = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService)
                  .QueryInterface(Ci.nsIPrefBranch);
var os = Cc["@mozilla.org/observer-service;1"]
         .getService(Ci.nsIObserverService);

var systemLocale = localeService.getLocaleComponentForUserAgent();
var count = 0;
var testsLocale = [
  {matchOS: false, selected: null, locale: "en-US"},
  {matchOS: true, selected: null, locale: systemLocale},
  {matchOS: true, selected: "fr-FR", locale: systemLocale},
  {matchOS: false, selected: "fr-FR", locale: "fr-FR"},
  {matchOS: false, selected: "de-DE", locale: "de-DE"},
  {matchOS: true, selected: null, locale: systemLocale}
];

function test_locale(aTest) {
  prefService.setBoolPref("intl.locale.matchOS", aTest.matchOS);
  prefService.setCharPref("general.useragent.locale", aTest.selected || "en-US");

  chromeReg.reloadChrome();
}

function checkValidity() {
  var selectedLocale = chromeReg.getSelectedLocale("testmatchos");
  do_check_eq(selectedLocale, testsLocale[count].locale);

  count++;
  if (count >= testsLocale.length) {
    os.removeObserver(checkValidity, "selected-locale-has-changed");
    do_test_finished();
  }
  else {
    test_locale(testsLocale[count]);
  }
}

function run_test() {
  os.addObserver(checkValidity, "selected-locale-has-changed", false);
  test_locale(testsLocale[count]);
}

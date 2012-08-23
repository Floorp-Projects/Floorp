/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

var MANIFESTS = [
  do_get_file("data/test_bug519468.manifest")
];

// Stub in the locale service so we can control what gets returned as the OS locale setting
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

let stubOSLocale = null;

stubID = Components.ID("9d09d686-d913-414c-a1e6-4be8652d7d93");
localeContractID = "@mozilla.org/intl/nslocaleservice;1";

StubLocaleService = {
  classDescription: "Stub version of Locale service for testing",
  classID:          stubID,
  contractID:       localeContractID,
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsILocaleService, Ci.nsISupports, Ci.nsIFactory]),

  createInstance: function (outer, iid) {
    if (outer)
      throw Components.results.NS_ERROR_NO_AGGREGATION;
    return this.QueryInterface(iid);
  },
  lockFactory: function (lock) {
    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
  },

  getLocaleComponentForUserAgent: function SLS_getLocaleComponentForUserAgent()
  {
    return stubOSLocale;
  }
}

let registrar = Components.manager.nsIComponentRegistrar;
// Save original factory.
let localeCID = registrar.contractIDToCID(localeContractID)
let originalFactory =
      Components.manager.getClassObject(Components.classes[localeContractID],
                                        Components.interfaces.nsIFactory);

registrar.registerFactory(stubID, "Unit test Locale Service", localeContractID, StubLocaleService);

// Now fire up the test
do_test_pending()
registerManifests(MANIFESTS);

var chromeReg = Cc["@mozilla.org/chrome/chrome-registry;1"]
                .getService(Ci.nsIXULChromeRegistry)
                .QueryInterface(Ci.nsIToolkitChromeRegistry);
chromeReg.checkForNewChrome();

var prefService = Cc["@mozilla.org/preferences-service;1"]
                  .getService(Ci.nsIPrefService)
                  .QueryInterface(Ci.nsIPrefBranch);
var os = Cc["@mozilla.org/observer-service;1"]
         .getService(Ci.nsIObserverService);

var testsLocale = [
  // These tests cover when the OS local is included in our manifest
  {matchOS: false, selected: "en-US", osLocale: "xx-AA", locale: "en-US"},
  {matchOS: true, selected: "en-US", osLocale: "xx-AA", locale: "xx-AA"},
  {matchOS: false, selected: "fr-FR", osLocale: "xx-AA", locale: "fr-FR"},
  {matchOS: true, selected: "fr-FR", osLocale: "xx-AA", locale: "xx-AA"},
  {matchOS: false, selected: "de-DE", osLocale: "xx-AA", locale: "de-DE"},
  {matchOS: true, selected: "de-DE", osLocale: "xx-AA", locale: "xx-AA"},
  // these tests cover the case where the OS locale is not available in our manifest, but the
  // base language is (ie, substitute xx-AA which we have for xx-BB which we don't)
  {matchOS: false, selected: "en-US", osLocale: "xx-BB", locale: "en-US"},
  {matchOS: true, selected: "en-US", osLocale: "xx-BB", locale: "xx-AA"},
  {matchOS: false, selected: "fr-FR", osLocale: "xx-BB", locale: "fr-FR"},
  {matchOS: true, selected: "fr-FR", osLocale: "xx-BB", locale: "xx-AA"},
  // These tests cover where the language is not available
  {matchOS: false, selected: "en-US", osLocale: "xy-BB", locale: "en-US"},
  {matchOS: true, selected: "en-US", osLocale: "xy-BB", locale: "en-US"},
  {matchOS: false, selected: "fr-FR", osLocale: "xy-BB", locale: "fr-FR"},
  {matchOS: true, selected: "fr-FR", osLocale: "xy-BB", locale: "en-US"},
];

var observedLocale = null;

function test_locale(aTest) {
  observedLocale = null;

  stubOSLocale = aTest.osLocale;
  prefService.setBoolPref("intl.locale.matchOS", aTest.matchOS);
  prefService.setCharPref("general.useragent.locale", aTest.selected);

  chromeReg.reloadChrome();

  do_check_eq(observedLocale, aTest.locale);
}

// Callback function for observing locale change. May be called more than once
// per test iteration.
function checkValidity() {
  observedLocale = chromeReg.getSelectedLocale("testmatchos");
  dump("checkValidity called back with locale = " + observedLocale + "\n");
}

function run_test() {
  os.addObserver(checkValidity, "selected-locale-has-changed", false);

  for (let count = 0 ; count < testsLocale.length ; count++) {
    dump("count = " + count + " " + testsLocale[count].toSource() + "\n");
    test_locale(testsLocale[count]);
  }

  os.removeObserver(checkValidity, "selected-locale-has-changed");
  do_test_finished();
}

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that preferences are properly set by distribution.ini
 */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LoadContextInfo.jsm");

// Import common head.
var commonFile = do_get_file("../../../../toolkit/components/places/tests/head_common.js", false);
if (commonFile) {
  let uri = Services.io.newFileURI(commonFile);
  Services.scriptloader.loadSubScript(uri.spec, this);
}

const TOPICDATA_DISTRIBUTION_CUSTOMIZATION = "force-distribution-customization";
const TOPIC_BROWSERGLUE_TEST = "browser-glue-test";

function run_test() {
  // Set special pref to load distribution.ini from the profile folder.
  Services.prefs.setBoolPref("distribution.testing.loadFromProfile", true);

  // Copy distribution.ini file to the profile dir.
  let distroDir = gProfD.clone();
  distroDir.leafName = "distribution";
  let iniFile = distroDir.clone();
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
    print("distribution.ini already exists, did some test forget to cleanup?");
  }

  let testDistributionFile = gTestDir.clone();
  testDistributionFile.append("distribution.ini");
  testDistributionFile.copyTo(distroDir, "distribution.ini");
  Assert.ok(testDistributionFile.exists());

  run_next_test();
}

do_register_cleanup(function () {
  // Remove the distribution file, even if the test failed, otherwise all
  // next tests will import it.
  let iniFile = gProfD.clone();
  iniFile.leafName = "distribution";
  iniFile.append("distribution.ini");
  if (iniFile.exists()) {
    iniFile.remove(false);
  }
  Assert.ok(!iniFile.exists());
});

add_task(function* () {
  // Force distribution.
  let glue = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver)
  glue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_DISTRIBUTION_CUSTOMIZATION);

  Assert.equal(Services.prefs.getCharPref("distribution.test.string"), "Test String");
  Assert.throws(() => Services.prefs.getCharPref("distribution.test.string.noquotes"));
  Assert.equal(Services.prefs.getIntPref("distribution.test.int"), 777);
  Assert.equal(Services.prefs.getBoolPref("distribution.test.bool.true"), true);
  Assert.equal(Services.prefs.getBoolPref("distribution.test.bool.false"), false);
  Assert.equal(Services.prefs.getComplexValue("distribution.test.locale", Ci.nsIPrefLocalizedString).data, "en-US");
  Assert.equal(Services.prefs.getComplexValue("distribution.test.language.en", Ci.nsIPrefLocalizedString).data, "en");
  Assert.equal(Services.prefs.getComplexValue("distribution.test.locale.en-US", Ci.nsIPrefLocalizedString).data, "en-US");
  Assert.throws(() => Services.prefs.getComplexValue("distribution.test.locale.de", Ci.nsIPrefLocalizedString));
  // This value was never set because of the empty locale specific pref
  // This testcase currently fails - the value is set to "undefined" - it should not be set at all (throw)
  // Assert.throws(() => Services.prefs.getComplexValue("distribution.test.reset", Ci.nsIPrefLocalizedString));
  // This value was overriden by a locale specific setting
  Assert.equal(Services.prefs.getComplexValue("distribution.test.locale.set", Ci.nsIPrefLocalizedString).data, "Second Set");
  // This value was overriden by a language specific setting
  Assert.equal(Services.prefs.getComplexValue("distribution.test.language.set", Ci.nsIPrefLocalizedString).data, "Second Set");
});

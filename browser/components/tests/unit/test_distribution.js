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

/**
 * Copy the engine-distribution.xml engine to a fake distribution
 * created in the profile, and registered with the directory service.
 * Create an empty en-US directory to make sure it isn't used.
 */
function installDistributionEngine() {
  const XRE_APP_DISTRIBUTION_DIR = "XREAppDist";

  const gProfD = do_get_profile().QueryInterface(Ci.nsILocalFile);

  let dir = gProfD.clone();
  dir.append("distribution");
  let distDir = dir.clone();

  dir.append("searchplugins");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  dir.append("locale");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);
  let localeDir = dir.clone();

  dir.append("en-US");
  dir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  localeDir.append("de-DE");
  localeDir.create(dir.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine-de-DE.xml").copyTo(localeDir, "engine-de-DE.xml");

  Services.dirsvc.registerProvider({
    getFile: function(aProp, aPersistent) {
      aPersistent.value = true;
      if (aProp == XRE_APP_DISTRIBUTION_DIR)
        return distDir.clone();
      return null;
    }
  });
}

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

  installDistributionEngine();

  run_next_test();
}

do_register_cleanup(function () {
  // Remove the distribution dir, even if the test failed, otherwise all
  // next tests will use it.
  let distDir = gProfD.clone();
  distDir.append("distribution");
  distDir.remove(true);
  Assert.ok(!distDir.exists());
});

add_task(function* () {
  // Force distribution.
  let glue = Cc["@mozilla.org/browser/browserglue;1"].getService(Ci.nsIObserver)
  glue.observe(null, TOPIC_BROWSERGLUE_TEST, TOPICDATA_DISTRIBUTION_CUSTOMIZATION);

  var defaultBranch = Services.prefs.getDefaultBranch(null);

  Assert.equal(defaultBranch.getCharPref("distribution.id"), "disttest");
  Assert.equal(defaultBranch.getCharPref("distribution.version"), "1.0");
  Assert.equal(defaultBranch.getComplexValue("distribution.about", Ci.nsISupportsString).data, "Tèƨƭ δïƨƭřïβúƭïôñ ƒïℓè");

  Assert.equal(defaultBranch.getCharPref("distribution.test.string"), "Test String");
  Assert.equal(defaultBranch.getCharPref("distribution.test.string.noquotes"), "Test String");
  Assert.equal(defaultBranch.getIntPref("distribution.test.int"), 777);
  Assert.equal(defaultBranch.getBoolPref("distribution.test.bool.true"), true);
  Assert.equal(defaultBranch.getBoolPref("distribution.test.bool.false"), false);

  Assert.throws(() => defaultBranch.getCharPref("distribution.test.empty"));
  Assert.throws(() => defaultBranch.getIntPref("distribution.test.empty"));
  Assert.throws(() => defaultBranch.getBoolPref("distribution.test.empty"));

  Assert.equal(defaultBranch.getCharPref("distribution.test.pref.locale"), "en-US");
  Assert.equal(defaultBranch.getCharPref("distribution.test.pref.language.en"), "en");
  Assert.equal(defaultBranch.getCharPref("distribution.test.pref.locale.en-US"), "en-US");
  Assert.throws(() => defaultBranch.getCharPref("distribution.test.pref.language.de"));
  // This value was never set because of the empty language specific pref
  Assert.throws(() => defaultBranch.getCharPref("distribution.test.pref.language.reset"));
  // This value was never set because of the empty locale specific pref
  Assert.throws(() => defaultBranch.getCharPref("distribution.test.pref.locale.reset"));
  // This value was overridden by a locale specific setting
  Assert.equal(defaultBranch.getCharPref("distribution.test.pref.locale.set"), "Locale Set");
  // This value was overridden by a language specific setting
  Assert.equal(defaultBranch.getCharPref("distribution.test.pref.language.set"), "Language Set");
  // Language should not override locale
  Assert.notEqual(defaultBranch.getCharPref("distribution.test.pref.locale.set"), "Language Set");

  Assert.equal(defaultBranch.getComplexValue("distribution.test.locale", Ci.nsIPrefLocalizedString).data, "en-US");
  Assert.equal(defaultBranch.getComplexValue("distribution.test.language.en", Ci.nsIPrefLocalizedString).data, "en");
  Assert.equal(defaultBranch.getComplexValue("distribution.test.locale.en-US", Ci.nsIPrefLocalizedString).data, "en-US");
  Assert.throws(() => defaultBranch.getComplexValue("distribution.test.language.de", Ci.nsIPrefLocalizedString));
  // This value was never set because of the empty language specific pref
  Assert.throws(() => defaultBranch.getComplexValue("distribution.test.language.reset", Ci.nsIPrefLocalizedString));
  // This value was never set because of the empty locale specific pref
  Assert.throws(() => defaultBranch.getComplexValue("distribution.test.locale.reset", Ci.nsIPrefLocalizedString));
  // This value was overridden by a locale specific setting
  Assert.equal(defaultBranch.getComplexValue("distribution.test.locale.set", Ci.nsIPrefLocalizedString).data, "Locale Set");
  // This value was overridden by a language specific setting
  Assert.equal(defaultBranch.getComplexValue("distribution.test.language.set", Ci.nsIPrefLocalizedString).data, "Language Set");
  // Language should not override locale
  Assert.notEqual(defaultBranch.getComplexValue("distribution.test.locale.set", Ci.nsIPrefLocalizedString).data, "Language Set");

  do_test_pending();

  Services.prefs.setCharPref("distribution.searchplugins.defaultLocale", "de-DE");

  Services.search.init(function() {
    Assert.equal(Services.search.isInitialized, true);
    var engine = Services.search.getEngineByName("Google");
    Assert.equal(engine.description, "override-de-DE");
    do_test_finished();
  });
});

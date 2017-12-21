/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that DistributionCustomizer correctly caches the existence
 * of the distribution.ini file and just rechecks it after a version
 * update.
 */

const PREF_CACHED_FILE_EXISTENCE  = "distribution.iniFile.exists.value";
const PREF_CACHED_FILE_APPVERSION = "distribution.iniFile.exists.appversion";
const PREF_LOAD_FROM_PROFILE      = "distribution.testing.loadFromProfile";

const gTestDir = do_get_cwd();

Cu.import("resource://gre/modules/AppConstants.jsm");

add_task(async function() {
  // Start with a clean slate of the prefs that control this feature.
  Services.prefs.clearUserPref(PREF_CACHED_FILE_APPVERSION);
  Services.prefs.clearUserPref(PREF_CACHED_FILE_EXISTENCE);
  setupTest();

  let {DistributionCustomizer} = Cu.import("resource:///modules/distribution.js", {});
  let distribution = new DistributionCustomizer();

  copyDistributionToProfile();

  // Check that checking for distribution.ini returns the right value and sets up
  // the cached prefs.
  let exists = distribution._hasDistributionIni;

  Assert.ok(exists);
  Assert.equal(Services.prefs.getBoolPref(PREF_CACHED_FILE_EXISTENCE, undefined),
               true);
  Assert.equal(Services.prefs.getStringPref(PREF_CACHED_FILE_APPVERSION, "unknown"),
               AppConstants.MOZ_APP_VERSION);

  // Check that calling _hasDistributionIni again will use the cached value. We do
  // this by deleting the file and expecting it to still return true instead of false.
  // Also, we need to delete _hasDistributionIni from the object because the getter
  // was replaced with a stored value.
  deleteDistribution();
  delete distribution._hasDistributionIni;

  exists = distribution._hasDistributionIni;
  Assert.ok(exists);

  // Now let's invalidate the PREF_CACHED_FILE_EXISTENCE pref to make sure the
  // value gets recomputed correctly.
  Services.prefs.clearUserPref(PREF_CACHED_FILE_EXISTENCE);
  delete distribution._hasDistributionIni;
  exists = distribution._hasDistributionIni;

  // It now should return false, as well as storing false in the pref.
  Assert.ok(!exists);
  Assert.equal(Services.prefs.getBoolPref(PREF_CACHED_FILE_EXISTENCE, undefined),
               false);

  // Check now that it will use the new cached value instead of returning true in
  // the presence of the file.
  copyDistributionToProfile();
  delete distribution._hasDistributionIni;
  exists = distribution._hasDistributionIni;

  Assert.ok(!exists);

  // Now let's do the same, but invalidating the App Version, as if a version
  // update occurred.
  Services.prefs.setStringPref(PREF_CACHED_FILE_APPVERSION, "older version");
  delete distribution._hasDistributionIni;
  exists = distribution._hasDistributionIni;

  Assert.ok(exists);
  Assert.equal(Services.prefs.getBoolPref(PREF_CACHED_FILE_EXISTENCE, undefined),
               true);
  Assert.equal(Services.prefs.getStringPref(PREF_CACHED_FILE_APPVERSION, "unknown"),
               AppConstants.MOZ_APP_VERSION);
});


/*
 * Helper functions
 */
function copyDistributionToProfile() {
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
}

function deleteDistribution() {
  let distroDir = gProfD.clone();
  distroDir.leafName = "distribution";
  let iniFile = distroDir.clone();
  iniFile.append("distribution.ini");
  iniFile.remove(false);
}

function setupTest() {
  // Set special pref to load distribution.ini from the profile folder.
  Services.prefs.setBoolPref(PREF_LOAD_FROM_PROFILE, true);
}

registerCleanupFunction(function() {
  deleteDistribution();
  Services.prefs.clearUserPref(PREF_LOAD_FROM_PROFILE);
});

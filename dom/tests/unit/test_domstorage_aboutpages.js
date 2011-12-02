/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

Components.utils.import("resource://gre/modules/Services.jsm");

function run_test()
{
  // Needs a profile folder for the database.
  do_get_profile();
  testURI(Services.io.newURI("about:mozilla", null, null));
  testURI(Services.io.newURI("moz-safe-about:rights", null, null));
}

function sum(a)
{
  return a.reduce(function(prev, current, index, array) {
    return prev + current;
  });
}

function testURI(aURI)
{
  print("Testing: " + aURI.spec);
  let storage = getStorageForURI(aURI);
  let Telemetry = Components.classes["@mozilla.org/base/telemetry;1"].
                  getService(Components.interfaces.nsITelemetry);
  let key_histogram = Telemetry.getHistogramById("LOCALDOMSTORAGE_KEY_SIZE_BYTES");
  let value_histogram = Telemetry.getHistogramById("LOCALDOMSTORAGE_VALUE_SIZE_BYTES");
  let before_key_snapshot = key_histogram.snapshot();
  let before_value_snapshot = value_histogram.snapshot();
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  let after_key_snapshot = key_histogram.snapshot();
  let after_value_snapshot = value_histogram.snapshot();
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");
  do_check_eq(sum(after_key_snapshot.counts),
	      sum(before_key_snapshot.counts)+1);
  do_check_eq(sum(after_value_snapshot.counts),
	      sum(before_value_snapshot.counts)+1);

  print("Check that our value is correctly removed.");
  storage.removeItem("test-item");
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);

  testURIWithPrivateBrowsing(aURI);

  testURIWithClearCookies(aURI);

  testURIWithRejectCookies(aURI);

  testURIWithCasing(aURI);
}

function testURIWithPrivateBrowsing(aURI) {
  print("Testing with private browsing: " + aURI.spec);
  // Skip test if PB mode is not supported.
  if (!("@mozilla.org/privatebrowsing;1" in Components.classes)) {
    print("Skipped.");
    return;
  }

  let storage = getStorageForURI(aURI);
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");
  togglePBMode(true);
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");

  print("Check that our value is correctly removed.");
  storage.removeItem("test-item");
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);
  togglePBMode(false);
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);
}

function testURIWithClearCookies(aURI) {
  let storage = getStorageForURI(aURI);
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");

  let dsm = Components.classes["@mozilla.org/dom/storagemanager;1"].
        getService(Components.interfaces.nsIObserver);
  dsm.observe(null, "cookie-changed", "cleared");

  print("Check that our value is still stored.");
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");

  print("Check that we can explicitly clear value.");
  storage.clear();
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);
}

function testURIWithRejectCookies(aURI) {
  // This test acts with chrome privileges, so it's not enough to test content.
  function test_storage() {
    let storage = getStorageForURI(aURI);
    storage.setItem("test-item", "test-value");
    print("Check that our value has been correctly stored.");
    do_check_eq(storage.length, 1);
    do_check_eq(storage.key(0), "test-item");
    do_check_eq(storage.getItem("test-item"), "test-value");
    storage.clear();
    do_check_eq(storage.length, 0);
    do_check_eq(storage.getItem("test-item"), null);
  }

  // Ask every time.
  Services.prefs.setIntPref("network.cookie.lifetimePolicy", 1);
  test_storage();

  // Reject.
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 2);
  test_storage();
}

function testURIWithCasing(aURI) {
  print("Testing: " + aURI.spec);
  let storage = getStorageForURI(aURI);
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");

  let ucSpec = aURI.spec.toUpperCase();
  print("Testing: " + ucSpec);
  let ucStorage = getStorageForURI(Services.io.newURI(ucSpec, null, null));
  print("Check that our value is accessible in a case-insensitive way.");
  do_check_eq(ucStorage.length, 1);
  do_check_eq(ucStorage.key(0), "test-item");
  do_check_eq(ucStorage.getItem("test-item"), "test-value");

  print("Check that our value is correctly removed.");
  storage.removeItem("test-item");
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);
}

function getStorageForURI(aURI)
{
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"].
                  getService(Components.interfaces.nsIScriptSecurityManager).
                  getCodebasePrincipal(aURI);
  let dsm = Components.classes["@mozilla.org/dom/storagemanager;1"].
            getService(Components.interfaces.nsIDOMStorageManager);
  return dsm.getLocalStorageForPrincipal(principal, "");
}

function togglePBMode(aEnable)
{
  let pb = Components.classes["@mozilla.org/privatebrowsing;1"].
           getService(Components.interfaces.nsIPrivateBrowsingService);
  if (aEnable) {
    Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session",
                               true);
    pb.privateBrowsingEnabled = true;
  } else {
    try {
      prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
    } catch (ex) {}
    pb.privateBrowsingEnabled = false;
  }
}

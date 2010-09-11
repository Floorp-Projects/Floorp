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

function testURI(aURI)
{
  print("Testing: " + aURI.spec);
  let storage = getStorageForURI(aURI);
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  do_check_eq(storage.length, 1);
  do_check_eq(storage.key(0), "test-item");
  do_check_eq(storage.getItem("test-item"), "test-value");

  print("Check that our value is correctly removed.");
  storage.removeItem("test-item");
  do_check_eq(storage.length, 0);
  do_check_eq(storage.getItem("test-item"), null);

  testURIWithPrivateBrowsing(aURI);

  testURIWithClearCookies(aURI);
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

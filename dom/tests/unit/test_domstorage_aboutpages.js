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

function testURI(aURI) {
  print("Testing: " + aURI.spec);
  do_check_true(/about$/.test(aURI.scheme));
  let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"].
                  getService(Components.interfaces.nsIScriptSecurityManager).
                  getCodebasePrincipal(aURI);
  let dsm = Components.classes["@mozilla.org/dom/storagemanager;1"].
            getService(Components.interfaces.nsIDOMStorageManager);
  let storage = dsm.getLocalStorageForPrincipal(principal, "");
  storage.setItem("test-item", "test-value");
  print("Check that our value has been correctly stored.");
  do_check_eq(storage.getItem("test-item"), "test-value");
  storage.removeItem("test-item");
  print("Check that our value has been correctly removed.");
  do_check_eq(storage.getItem("test-item"), null);
}


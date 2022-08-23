/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Because this is an xpcshell global, it does not have an associated client id.
 * We turn on client validation for LocalStorage and ensure that we don't have
 * access to LocalStorage.
 */
add_task(async function testSteps() {
  const principal = getPrincipal("http://example.com");

  info("Setting prefs");

  Services.prefs.setBoolPref(
    "dom.storage.enable_unsupported_legacy_implementation",
    false
  );
  Services.prefs.setBoolPref("dom.storage.client_validation", true);

  info("Getting storage");

  try {
    getLocalStorage(principal);
    ok(false, "Should have thrown");
  } catch (ex) {
    ok(true, "Did throw");
    is(ex.name, "NS_ERROR_FAILURE", "Threw right Exception");
    is(ex.result, Cr.NS_ERROR_FAILURE, "Threw with right result");
  }
});

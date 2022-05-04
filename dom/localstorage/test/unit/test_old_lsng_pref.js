/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that the old pref for switching LS
 * implementations has no effect anymore.
 */

// eslint-disable-next-line no-unused-vars
async function testSteps() {
  info("Setting pref");

  Services.prefs.setBoolPref("dom.storage.next_gen", false);

  ok(Services.domStorageManager.nextGenLocalStorageEnabled, "LSNG enabled");
}

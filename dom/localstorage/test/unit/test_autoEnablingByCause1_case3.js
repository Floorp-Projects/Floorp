/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This test is mainly to verify that network.cookie.lifetimePolicy=2 doesn't
 * cause auto enabling of LSNG if it was already auto enabled and then manually
 * disabled.
 *
 * Prefs for the test are defined in xpcshell.ini because they have to be set
 * before the test is actually launched.
 */

async function testSteps() {
  info("Checking prefs");

  is(
    Services.prefs.getIntPref("network.cookie.lifetimePolicy"),
    2,
    "Correct value"
  );

  is(
    Services.prefs.getBoolPref("dom.storage.next_gen"),
    false,
    "Correct value"
  );

  is(
    Services.prefs.getBoolPref("dom.storage.next_gen_auto_enabled_by_cause1"),
    true,
    "Correct value"
  );
}

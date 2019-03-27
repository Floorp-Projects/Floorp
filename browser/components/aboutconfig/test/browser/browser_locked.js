/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This is a temporary workaround to
 * be resolved in bug 1539000.
 */
ChromeUtils.import("resource://testing-common/PromiseTestUtils.jsm", this);
PromiseTestUtils.whitelistRejectionsGlobally(/Too many characters in placeable/);

const PREF_STRING_NO_DEFAULT = "test.aboutconfig.a";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [PREF_STRING_NO_DEFAULT, "some value"],
    ],
  });
});

add_task(async function test_locked() {
  registerCleanupFunction(() => {
    Services.prefs.unlockPref(PREF_STRING_DEFAULT_NOTEMPTY);
    Services.prefs.unlockPref(PREF_BOOLEAN_DEFAULT_TRUE);
    Services.prefs.unlockPref(PREF_STRING_NO_DEFAULT);
  });

  Services.prefs.lockPref(PREF_STRING_DEFAULT_NOTEMPTY);
  Services.prefs.lockPref(PREF_BOOLEAN_DEFAULT_TRUE);
  Services.prefs.lockPref(PREF_STRING_NO_DEFAULT);

  await AboutConfigTest.withNewTab(async function() {
    // Test locked default string pref.
    let lockedPref = this.getRow(PREF_STRING_DEFAULT_NOTEMPTY);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, PREF_STRING_DEFAULT_NOTEMPTY_VALUE);
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test locked default boolean pref.
    lockedPref = this.getRow(PREF_BOOLEAN_DEFAULT_TRUE);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "true");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-toggle"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test locked user added pref.
    lockedPref = this.getRow(PREF_STRING_NO_DEFAULT);
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test pref not locked.
    let unlockedPref = this.getRow(PREF_BOOLEAN_USERVALUE_TRUE);
    Assert.ok(!unlockedPref.hasClass("locked"));
    Assert.equal(unlockedPref.value, "true");
    Assert.ok(unlockedPref.editColumnButton.classList.contains("button-toggle"));
    Assert.ok(!unlockedPref.editColumnButton.disabled);
  });
});

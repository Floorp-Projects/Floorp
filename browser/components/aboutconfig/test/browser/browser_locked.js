/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["test.aboutconfig.a", "some value"],
    ],
  });
});

add_task(async function test_locked() {
  registerCleanupFunction(() => {
    Services.prefs.unlockPref("browser.search.searchEnginesURL");
    Services.prefs.unlockPref("test.aboutconfig.a");
    Services.prefs.unlockPref("accessibility.AOM.enabled");
  });

  Services.prefs.lockPref("browser.search.searchEnginesURL");
  Services.prefs.lockPref("test.aboutconfig.a");
  Services.prefs.lockPref("accessibility.AOM.enabled");
  await AboutConfigTest.withNewTab(async function() {
    // Test locked default string pref.
    let lockedPref = this.getRow("browser.search.searchEnginesURL");
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "https://addons.mozilla.org/%LOCALE%/firefox/search-engines/");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test locked default boolean pref.
    lockedPref = this.getRow("accessibility.AOM.enabled");
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "false");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-toggle"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test locked user added pref.
    lockedPref = this.getRow("test.aboutconfig.a");
    Assert.ok(lockedPref.hasClass("locked"));
    Assert.equal(lockedPref.value, "");
    Assert.ok(lockedPref.editColumnButton.classList.contains("button-edit"));
    Assert.ok(lockedPref.editColumnButton.disabled);

    // Test pref not locked
    let unlockedPref = this.getRow("accessibility.indicator.enabled");
    Assert.ok(!unlockedPref.hasClass("locked"));
    Assert.equal(unlockedPref.value, "false");
    Assert.ok(unlockedPref.editColumnButton.classList.contains("button-toggle"));
    Assert.ok(!unlockedPref.editColumnButton.disabled);
  });
});

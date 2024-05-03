/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

async function launchPreferences() {
  let prefs = await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  Assert.equal(prefs.selectedPane, "paneGeneral", "General pane was selected");
}

add_task(async function () {
  await launchPreferences();
  let checkbox = gBrowser.contentDocument.querySelector(
    "#useFullKeyboardNavigation"
  );
  Assert.ok(
    !Services.prefs.getIntPref("accessibility.tabfocus", undefined),
    "no pref value should exist"
  );
  Assert.ok(
    !checkbox.checked,
    "checkbox should be unchecked before clicking on checkbox"
  );

  checkbox.click();

  Assert.equal(
    Services.prefs.getIntPref("accessibility.tabfocus"),
    7,
    "Prefstore should reflect checkbox's associated numeric value"
  );
  Assert.ok(
    checkbox.checked,
    "checkbox should be checked after clicking on checkbox"
  );

  checkbox.click();

  Assert.ok(
    !checkbox.checked,
    "checkbox should be unchecked after clicking on checkbox"
  );
  Assert.ok(
    !Services.prefs.getIntPref("accessibility.tabfocus", undefined),
    "No pref value should exist"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 4]] });
  await launchPreferences();
  checkbox = gBrowser.contentDocument.querySelector(
    "#useFullKeyboardNavigation"
  );

  Assert.ok(
    !checkbox.checked,
    "checkbox should stay unchecked after setting non-7 pref value"
  );
  Assert.equal(
    Services.prefs.getIntPref("accessibility.tabfocus", 0),
    4,
    "pref should have value in store"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  SpecialPowers.pushPrefEnv({ set: [["accessibility.tabfocus", 7]] });
  await launchPreferences();
  checkbox = gBrowser.contentDocument.querySelector(
    "#useFullKeyboardNavigation"
  );

  Assert.equal(
    Services.prefs.getIntPref("accessibility.tabfocus", 0),
    7,
    "Pref value should update after modification"
  );
  Assert.ok(checkbox.checked, "checkbox should be checked");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

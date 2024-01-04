/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This tests the Privacy pane's Firefox QuickActions UI.

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  UrlbarProviderQuickActions:
    "resource:///modules/UrlbarProviderQuickActions.sys.mjs",
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.sys.mjs",
});

add_setup(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.suggest.quickactions", true],
      ["browser.urlbar.quickactions.enabled", true],
    ],
  });

  UrlbarProviderQuickActions.addAction("testaction", {
    commands: ["testaction"],
    label: "quickactions-downloads2",
  });

  registerCleanupFunction(() => {
    UrlbarProviderQuickActions.removeAction("testaction");
  });
});

async function isGroupHidden(tab) {
  return SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async () => content.document.getElementById("quickActionsBox").hidden
  );
}

add_task(async function test_show_prefs() {
  Services.prefs.setBoolPref("browser.urlbar.quickactions.showPrefs", false);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#search"
  );

  Assert.ok(
    await isGroupHidden(tab),
    "The preferences are hidden when pref disabled"
  );

  Services.prefs.setBoolPref("browser.urlbar.quickactions.showPrefs", true);

  Assert.ok(
    !(await isGroupHidden(tab)),
    "The preferences are shown when pref enabled"
  );

  Services.prefs.clearUserPref("browser.urlbar.quickactions.showPrefs");
  await BrowserTestUtils.removeTab(tab);
});

async function testActionIsShown(window) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "testact",
    waitForFocus: SimpleTest.waitForFocus,
  });
  try {
    let { result } = await UrlbarTestUtils.getDetailsOfResultAt(window, 1);
    return result.providerName == "quickactions";
  } catch (e) {
    return false;
  }
}

add_task(async function test_prefs() {
  Services.prefs.setBoolPref("browser.urlbar.quickactions.showPrefs", true);
  Services.prefs.setBoolPref("browser.urlbar.suggest.quickactions", false);

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:preferences#search"
  );

  Assert.ok(
    !(await testActionIsShown(window)),
    "Actions are not shown while pref disabled"
  );

  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    let checkbox = content.document.getElementById("enableQuickActions");
    is(
      checkbox.checked,
      false,
      "Checkbox is not checked while feature is disabled"
    );
    checkbox.click();
  });

  Assert.ok(
    await testActionIsShown(window),
    "Actions are shown after user clicks checkbox"
  );

  Services.prefs.clearUserPref("browser.urlbar.quickactions.showPrefs");
  Services.prefs.clearUserPref("browser.urlbar.suggest.quickactions");
  await BrowserTestUtils.removeTab(tab);
});

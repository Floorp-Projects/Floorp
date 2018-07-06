add_task(async function testRestoreDefaultsBtn_visible() {
  const before = SpecialPowers.Services.prefs.getStringPref("browser.newtabpage.activity-stream.feeds.section.topstories.options", "");

  await SpecialPowers.pushPrefEnv({set: [
    // Hide Pocket pref so we don't trigger network requests when we reset all preferences
    ["browser.newtabpage.activity-stream.feeds.section.topstories.options", JSON.stringify(Object.assign({}, JSON.parse(before), {hidden: true}))],
    // Set a user pref to false to force the Restore Defaults button to be visible
    ["browser.newtabpage.activity-stream.feeds.topsites", false]
  ]});

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences#home", false);
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.getElementById("restoreDefaultHomePageBtn") !== null),
    "Wait for the button to be added to the page");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.querySelector("[data-subcategory='topsites'] checkbox") !== null),
    "Wait for the preference checkbox to load");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.getElementById("restoreDefaultHomePageBtn").hidden === false),
    "Should show the Restore Defaults btn because pref is changed");

  await ContentTask.spawn(browser, {}, () => content.document.getElementById("restoreDefaultHomePageBtn").click());

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.querySelector("[data-subcategory='topsites'] checkbox").checked),
    "Should have checked preference");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.getElementById("restoreDefaultHomePageBtn").style.visibility === "hidden"),
    "Should not show the Restore Defaults btn if prefs were reset");

  const topsitesPref = await SpecialPowers.Services.prefs.getBoolPref("browser.newtabpage.activity-stream.feeds.topsites");
  Assert.ok(topsitesPref, "Topsites pref should have the default value");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testRestoreDefaultsBtn_hidden() {
  const before = SpecialPowers.Services.prefs.getStringPref("browser.newtabpage.activity-stream.feeds.section.topstories.options", "");

  await SpecialPowers.pushPrefEnv({set: [
    // Hide Pocket pref so we don't trigger network requests when we reset all preferences
    ["browser.newtabpage.activity-stream.feeds.section.topstories.options", JSON.stringify(Object.assign({}, JSON.parse(before), {hidden: true}))],
  ]});

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:preferences#home", false);
  let browser = tab.linkedBrowser;

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.getElementById("restoreDefaultHomePageBtn") !== null),
    "Wait for the button to be added to the page");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.querySelector("[data-subcategory='topsites'] checkbox") !== null),
    "Wait for the preference checkbox to load");

  const btnDefault = await ContentTask.spawn(browser, {}, () => content.document.getElementById("restoreDefaultHomePageBtn").style.visibility);
  Assert.equal(btnDefault, "hidden", "When no prefs are changed button should not show up");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.querySelector("[data-subcategory='topsites'] checkbox").checked),
    "Should have checked preference");

  // Uncheck a pref
  await ContentTask.spawn(browser, {}, () => content.document.querySelector("[data-subcategory='topsites'] checkbox").click());

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => !content.document.querySelector("[data-subcategory='topsites'] checkbox").checked),
    "Should have unchecked preference");

  await BrowserTestUtils.waitForCondition(() => ContentTask.spawn(browser, {},
    () => content.document.getElementById("restoreDefaultHomePageBtn").style.visibility === "visible"),
    "Should show the Restore Defaults btn if prefs were changed");

  // Reset the pref
  await SpecialPowers.Services.prefs.clearUserPref("browser.newtabpage.activity-stream.feeds.topsites");

  await SpecialPowers.popPrefEnv();
  BrowserTestUtils.removeTab(tab);
});

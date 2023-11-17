/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that the Report Broken Site menu items are disabled
 * when the active tab is not on a reportable URL, and is hidden
 * when the feature is disabled via pref.
 */

"use strict";

add_common_setup();

async function testDisabledForInvalidURL(menu) {
  ensureReportBrokenSitePreffedOn();
  ok(gBrowser.currentURI.spec == "about:blank");
  await menu.open();
  menu.isReportBrokenSiteDisabled();
  await menu.close();
}

async function testEnabledForValidURL(menu) {
  ensureReportBrokenSitePreffedOn();
  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      await menu.open();
      menu.isReportBrokenSiteEnabled();
      await menu.close();
    }
  );
}

async function testHiddenWhenPreffedOff(menu) {
  ensureReportBrokenSitePreffedOff();
  await menu.open();
  menu.isReportBrokenSiteHidden();
  menu.close();

  await BrowserTestUtils.withNewTab(REPORTABLE_PAGE_URL, async function () {
    await menu.open();
    menu.isReportBrokenSiteHidden();
    await menu.close();
  });
}

add_task(async function test() {
  //  AppMenu
  await testHiddenWhenPreffedOff(AppMenu());
  await testDisabledForInvalidURL(AppMenu());
  await testEnabledForValidURL(AppMenu());

  // AppMenu help sub-menu
  await testHiddenWhenPreffedOff(AppMenuHelpSubmenu());
  await testDisabledForInvalidURL(AppMenuHelpSubmenu());
  await testEnabledForValidURL(AppMenuHelpSubmenu());

  // Protections Panel
  await testHiddenWhenPreffedOff(ProtectionsPanel());
  await testDisabledForInvalidURL(ProtectionsPanel());
  await testEnabledForValidURL(ProtectionsPanel());

  // Help menu
  await testHiddenWhenPreffedOff(HelpMenu());
  await testDisabledForInvalidURL(HelpMenu());
  await testEnabledForValidURL(HelpMenu());
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests that when Report Broken Site is active,
 * Report Site Issue is hidden.
 */

"use strict";

add_common_setup();

async function testDisabledByReportBrokenSite(menu) {
  ensureReportBrokenSitePreffedOn();
  ensureReportSiteIssuePreffedOn();

  await menu.open();
  menu.isReportSiteIssueHidden();
  await menu.close();
}

async function testDisabledByPref(menu) {
  ensureReportBrokenSitePreffedOff();
  ensureReportSiteIssuePreffedOff();

  await menu.open();
  menu.isReportSiteIssueHidden();
  await menu.close();
}

async function testDisabledForInvalidURLs(menu) {
  ensureReportBrokenSitePreffedOff();
  ensureReportSiteIssuePreffedOn();

  await menu.open();
  menu.isReportSiteIssueDisabled();
  await menu.close();
}

async function testEnabledForValidURLs(menu) {
  ensureReportBrokenSitePreffedOff();
  ensureReportSiteIssuePreffedOn();

  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      await menu.open();
      menu.isReportSiteIssueEnabled();
      await menu.close();
    }
  );
}

// AppMenu help sub-menu

add_task(async function testDisabledByReportBrokenSiteAppMenuHelpSubmenu() {
  await testDisabledByReportBrokenSite(AppMenuHelpSubmenu());
});

// disabled for now due to bug 1775402
//add_task(async function testDisabledByPrefAppMenuHelpSubmenu() {
//  await testDisabledByPref(AppMenuHelpSubmenu());
//});

add_task(async function testDisabledForInvalidURLsAppMenuHelpSubmenu() {
  await testDisabledForInvalidURLs(AppMenuHelpSubmenu());
});

add_task(async function testEnabledForValidURLsAppMenuHelpSubmenu() {
  await testEnabledForValidURLs(AppMenuHelpSubmenu());
});

// Help menu

add_task(async function testDisabledByReportBrokenSiteHelpMenu() {
  await testDisabledByReportBrokenSite(HelpMenu());
});

// disabled for now due to bug 1775402
//add_task(async function testDisabledByPrefHelpMenu() {
//  await testDisabledByPref(HelpMenu());
//});

add_task(async function testDisabledForInvalidURLsHelpMenu() {
  await testDisabledForInvalidURLs(HelpMenu());
});

add_task(async function testEnabledForValidURLsHelpMenu() {
  await testEnabledForValidURLs(HelpMenu());
});

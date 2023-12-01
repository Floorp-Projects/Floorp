/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that the Report Broken Site menu items are disabled
 * when the active tab is not on a reportable URL, and is hidden
 * when the feature is disabled via pref. Also make sure that
 * it does not appear on the App menu's Help sub-view.
 */

"use strict";

add_common_setup();

requestLongerTimeout(10);

add_task(async function test() {
  ensureReportBrokenSitePreffedOff();

  const menus = [
    [AppMenu(), false],
    [AppMenuHelpSubmenu(), true],
    [ProtectionsPanel(), false],
    [HelpMenu(), false],
  ];

  await BrowserTestUtils.withNewTab("about:blank", async function () {
    for (const [menu] of menus) {
      await menu.open();
      isMenuItemHidden(
        menu.reportBrokenSite,
        `${menu.menuDescription} option hidden on invalid page when preffed off`
      );
      await menu.close();
    }
  });

  await BrowserTestUtils.withNewTab(REPORTABLE_PAGE_URL, async function () {
    for (const [menu] of menus) {
      await menu.open();
      isMenuItemHidden(
        menu.reportBrokenSite,
        `${menu.menuDescription} option hidden on valid page when preffed off`
      );
      await menu.close();
    }
  });

  ensureReportBrokenSitePreffedOn();

  await BrowserTestUtils.withNewTab("about:blank", async function () {
    for (const [menu, alwaysHidden] of menus) {
      await menu.open();
      if (alwaysHidden) {
        isMenuItemHidden(
          menu.reportBrokenSite,
          `${menu.menuDescription} option hidden on invalid page when preffed on`
        );
      } else {
        isMenuItemDisabled(
          menu.reportBrokenSite,
          `${menu.menuDescription} option disabled on invalid page when preffed on`
        );
      }
      await menu.close();
    }
  });

  await BrowserTestUtils.withNewTab(REPORTABLE_PAGE_URL, async function () {
    for (const [menu, alwaysHidden] of menus) {
      await menu.open();
      if (alwaysHidden) {
        isMenuItemHidden(
          menu.reportBrokenSite,
          `${menu.menuDescription} option hidden on valid page when preffed on`
        );
      } else {
        isMenuItemEnabled(
          menu.reportBrokenSite,
          `${menu.menuDescription} option enabled on valid page when preffed on`
        );
      }
      await menu.close();
    }
  });

  ensureReportBrokenSitePreffedOff();

  await BrowserTestUtils.withNewTab(REPORTABLE_PAGE_URL, async function () {
    for (const [menu] of menus) {
      await menu.open();
      isMenuItemHidden(
        menu.reportBrokenSite,
        `${menu.menuDescription} option hidden again when pref toggled back off`
      );
      await menu.close();
    }
  });
});

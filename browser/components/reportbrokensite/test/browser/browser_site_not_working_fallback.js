/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests that when Report Broken Site is active,
 * "Site not working?" is hidden on the protections panel.
 */

"use strict";

add_common_setup();

const TP_PREF = "privacy.trackingprotection.enabled";

const TRACKING_PAGE =
  "https://tracking.example.org/browser/browser/base/content/test/protectionsUI/trackingPage.html";

const SITE_NOT_WORKING = "protections-popup-tp-switch-section-footer";

add_task(async function testSiteNotWorking() {
  await SpecialPowers.pushPrefEnv({ set: [[TP_PREF, true]] });
  await BrowserTestUtils.withNewTab(TRACKING_PAGE, async function () {
    const menu = ProtectionsPanel();

    ensureReportBrokenSitePreffedOn();
    await menu.open();
    const siteNotWorking = document.getElementById(SITE_NOT_WORKING);
    isMenuItemHidden(siteNotWorking, "Site not working is hidden");
    await menu.close();

    ensureReportBrokenSitePreffedOff();
    await menu.open();
    isMenuItemEnabled(siteNotWorking, "Site not working is shown");
    await menu.close();
  });
});

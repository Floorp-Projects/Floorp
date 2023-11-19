/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests that the send more info link appears only when its pref
 * is set to true, and that when clicked it will open a tab to
 * the webcompat.com endpoint and send the right data.
 */

"use strict";

add_common_setup();

add_task(async function testSendMoreInfoPref() {
  ensureReportBrokenSitePreffedOn();

  await BrowserTestUtils.withNewTab(
    REPORTABLE_PAGE_URL,
    async function (browser) {
      await changeTab(gBrowser.selectedTab, REPORTABLE_PAGE_URL);

      ensureSendMoreInfoDisabled();
      let rbs = await AppMenu().openReportBrokenSite();
      await rbs.isSendMoreInfoHidden();
      await rbs.close();

      ensureSendMoreInfoEnabled();
      rbs = await AppMenu().openReportBrokenSite();
      await rbs.isSendMoreInfoShown();
      await rbs.close();
    }
  );
});

async function testSendMoreInfo(menu, url, description = "any") {
  let rbs = await menu.openAndPrefillReportBrokenSite(url, description);

  if (!url) {
    url = menu.win.gBrowser.currentURI.spec;
  }

  const receivedData = await rbs.clickSendMoreInfo();
  const { message } = receivedData;

  is(message.url, url, "sent correct URL");
  is(message.description, description, "sent correct description");

  is(message.src, "desktop-reporter", "sent correct src");
  is(message.utm_campaign, "report-broken-site", "sent correct utm_campaign");
  is(message.utm_source, "desktop-reporter", "sent correct utm_source");

  ok(typeof message.details == "object", "sent extra details");

  // re-opening the panel, the url and description should be reset
  rbs = await menu.openReportBrokenSite();
  rbs.isMainViewResetToCurrentTab();
  rbs.close();
}

add_task(async function testSendingMoreInfo() {
  ensureReportBrokenSitePreffedOn();
  ensureSendMoreInfoEnabled();

  const tab = await openTab(REPORTABLE_PAGE_URL);

  await testSendMoreInfo(AppMenu());

  await changeTab(tab, REPORTABLE_PAGE_URL2);

  await testSendMoreInfo(ProtectionsPanel(), "https://override.com", "another");

  closeTab(tab);
});

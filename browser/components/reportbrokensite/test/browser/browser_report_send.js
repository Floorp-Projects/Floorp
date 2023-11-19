/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* Tests to ensure that sending or canceling reports with
 * the Send and Cancel buttons work (as well as the Okay button)
 */

"use strict";

add_common_setup();

requestLongerTimeout(10);

async function testSend(menu, url, description = "any") {
  let rbs = await menu.openAndPrefillReportBrokenSite(url, description);

  if (!url) {
    url = menu.win.gBrowser.currentURI.spec;
  }

  const pingCheck = new Promise(resolve => {
    GleanPings.brokenSiteReport.testBeforeNextSubmit(() => {
      Assert.equal(Glean.brokenSiteReport.url.testGetValue(), url);
      Assert.equal(
        Glean.brokenSiteReport.description.testGetValue(),
        description
      );
      resolve();
    });
  });

  await rbs.clickSend();
  await pingCheck;
  await rbs.clickOkay();

  // re-opening the panel, the url and description should be reset
  rbs = await menu.openReportBrokenSite();
  rbs.isMainViewResetToCurrentTab();
  rbs.close();
}

async function testCancel(menu, url, description) {
  let rbs = await menu.openAndPrefillReportBrokenSite(url, description);
  await rbs.clickCancel();
  ok(!rbs.opened, "clicking Cancel closes Report Broken Site");

  // re-opening the panel, the url and description should be reset
  rbs = await menu.openReportBrokenSite();
  rbs.isMainViewResetToCurrentTab();
  rbs.close();
}

add_task(async function testSendButton() {
  ensureReportBrokenSitePreffedOn();

  const tab1 = await openTab(REPORTABLE_PAGE_URL);

  await testSend(AppMenu());

  const tab2 = await openTab(REPORTABLE_PAGE_URL);

  await testSend(
    ProtectionsPanel(),
    "https://test.org/test/#fake",
    "test description"
  );

  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const tab3 = await openTab(REPORTABLE_PAGE_URL2, win2);

  await testSend(AppMenu(win2), null, "another test description");

  closeTab(tab3);
  await BrowserTestUtils.closeWindow(win2);

  closeTab(tab1);
  closeTab(tab2);
});

add_task(async function testCancelButton() {
  ensureReportBrokenSitePreffedOn();

  const tab1 = await openTab(REPORTABLE_PAGE_URL);

  await testCancel(AppMenu());
  await testCancel(ProtectionsPanel());
  await testCancel(HelpMenu());

  const tab2 = await openTab(REPORTABLE_PAGE_URL);

  await testCancel(AppMenu());
  await testCancel(ProtectionsPanel());
  await testCancel(HelpMenu());

  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  const tab3 = await openTab(REPORTABLE_PAGE_URL2, win2);

  await testCancel(AppMenu(win2));
  await testCancel(ProtectionsPanel(win2));
  await testCancel(HelpMenu(win2));

  closeTab(tab3);
  await BrowserTestUtils.closeWindow(win2);

  closeTab(tab1);
  closeTab(tab2);
});

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Verify that clicking the menu button opens an ssb.
add_task(async () => {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: gHttpsTestRoot + "test_page.html",
  });

  let ssbwin = await openSSBFromBrowserWindow();
  Assert.equal(
    getBrowser(ssbwin).currentURI.spec,
    gHttpsTestRoot + "test_page.html"
  );

  Assert.equal(tab.parentNode, null, "The tab should have been closed");
  await getSSB(ssbwin).uninstall();
  await BrowserTestUtils.closeWindow(ssbwin);
});

/**
 * This test is mainly to verify that datastores are cleared when the last
 * private browsing context exited.
 */

async function lsCheckFunc() {
  let storage = content.localStorage;

  if (storage.length) {
    return false;
  }

  // Store non-ASCII value to verify bug 1552428.
  storage.setItem("foo", "úžasné");

  return true;
}

function checkTabWindowLS(tab) {
  return ContentTask.spawn(tab.linkedBrowser, null, lsCheckFunc);
}

add_task(async function() {
  const pageUrl =
    "http://example.com/browser/dom/localstorage/test/page_private_ls.html";

  for (let i = 0; i < 2; i++) {
    let privateWin =
      await BrowserTestUtils.openNewBrowserWindow({private: true});

    let privateTab =
      await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, pageUrl);

    ok(await checkTabWindowLS(privateTab),
       "LS works correctly in a private-browsing page.");

    await BrowserTestUtils.closeWindow(privateWin);
  }
});

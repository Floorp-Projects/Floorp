/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests find bar auto-close behavior

const TEST_PATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

add_task(async function findbar_test() {
  let newTab = BrowserTestUtils.addTab(gBrowser, "about:blank");
  gBrowser.selectedTab = newTab;

  let url = TEST_PATH + "test_bug628179.html";
  let promise = BrowserTestUtils.browserLoaded(
    newTab.linkedBrowser,
    false,
    url
  );
  BrowserTestUtils.startLoadingURIString(newTab.linkedBrowser, url);
  await promise;

  await gFindBarPromise;
  gFindBar.open();

  await new ContentTask.spawn(newTab.linkedBrowser, null, async function () {
    let iframe = content.document.getElementById("iframe");
    let awaitLoad = ContentTaskUtils.waitForEvent(iframe, "load", false);
    iframe.src = "https://example.org/";
    await awaitLoad;
  });

  ok(
    !gFindBar.hidden,
    "the Find bar isn't hidden after the location of a subdocument changes"
  );

  let findBarClosePromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "findbarclose"
  );
  gFindBar.close();
  await findBarClosePromise;

  gBrowser.removeTab(newTab);
});

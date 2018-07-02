const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

add_task(async () => {
  const URL = ROOT + "file_favicon_redirect.html";
  const EXPECTED_ICON = ROOT + "file_favicon_redirect.ico";

  let promise = waitForFaviconMessage(true, EXPECTED_ICON);

  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let tabIcon = await promise;

  is(tabIcon.iconURL, EXPECTED_ICON, "should use the redirected icon for the tab");

  BrowserTestUtils.removeTab(tab);
});

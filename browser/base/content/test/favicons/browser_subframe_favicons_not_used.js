/* Make sure <link rel="..."> isn't respected in sub-frames. */

add_task(async function() {
  const ROOT = "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
  const URL = ROOT + "file_bug970276_popup1.html";

  let promiseIcon = waitForFaviconMessage(true, ROOT + "file_bug970276_favicon1.ico");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let icon = await promiseIcon;

  Assert.equal(icon.iconURL, ROOT + "file_bug970276_favicon1.ico", "The expected icon has been set");

  BrowserTestUtils.removeTab(tab);
});

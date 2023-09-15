"use strict";

add_task(async () => {
  const TEST_PATH = getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    // eslint-disable-next-line @microsoft/sdl/no-insecure-url
    "http://example.com"
  );

  const HTML_URI = TEST_PATH + "file_bug1691153.html";

  // Opening the page that contains the iframe
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let browser = tab.linkedBrowser;
  let browserLoaded = BrowserTestUtils.browserLoaded(
    browser,
    true,
    HTML_URI,
    true
  );
  info("new tab loaded");

  BrowserTestUtils.startLoadingURIString(browser, HTML_URI);
  await browserLoaded;
  info("The test page has loaded!");

  let first_message_promise = SpecialPowers.spawn(
    browser,
    [],
    async function () {
      let blobPromise = new Promise((resolve, reject) => {
        content.addEventListener("message", event => {
          if (event.data.bloburl) {
            info("Sanity check: recvd blob URL as " + event.data.bloburl);
            resolve(event.data.bloburl);
          }
        });
      });
      content.postMessage("getblob", "*");
      return blobPromise;
    }
  );
  info("The test page has loaded!");
  let blob_url = await first_message_promise;

  Assert.ok(blob_url.startsWith("blob:"), "Sanity check: recvd blob");
  info(`Received blob URL message from content: ${blob_url}`);
  // try to open the blob in a new tab, manually created by the user
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    blob_url,
    true,
    false,
    true
  );

  let principal = gBrowser.selectedTab.linkedBrowser._contentPrincipal;
  Assert.ok(
    !principal.isSystemPrincipal,
    "Newly opened blob shouldn't be Systemprincipal"
  );
  Assert.ok(
    !principal.isExpandedPrincipal,
    "Newly opened blob shouldn't be ExpandedPrincipal"
  );
  Assert.ok(
    principal.isContentPrincipal,
    "Newly opened blob tab should be ContentPrincipal"
  );

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

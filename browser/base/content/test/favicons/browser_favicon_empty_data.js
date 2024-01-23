/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT =
  "http://mochi.test:8888/browser/browser/base/content/test/favicons/";

const PAGE_URL = TEST_ROOT + "blank.html";
const ICON_URL = TEST_ROOT + "file_bug970276_favicon1.ico";
const ICON_DATAURI_START = "data:image/x-icon;base64,AAABAAEAEBAAAAAAAABoBQAA";

const EMPTY_PAGE_URL = TEST_ROOT + "file_favicon_empty.html";
const EMPTY_ICON_URL = "data:image/x-icon";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_URL },
    async browser => {
      let iconBox = gBrowser
        .getTabForBrowser(browser)
        .querySelector(".tab-icon-image");
      await addContentLinkForIconUrl(ICON_URL, browser);
      Assert.ok(
        browser.mIconURL.startsWith(ICON_DATAURI_START),
        "Favicon is correctly set."
      );

      // Give some time to ensure the icon is rendered.
      /* eslint-disable mozilla/no-arbitrary-setTimeout */
      await new Promise(resolve => setTimeout(resolve, 200));
      let firstIconShotDataURL = TestUtils.screenshotArea(iconBox, window);

      let browserLoaded = BrowserTestUtils.browserLoaded(
        browser,
        false,
        EMPTY_PAGE_URL
      );
      BrowserTestUtils.startLoadingURIString(browser, EMPTY_PAGE_URL);
      let iconChanged = waitForFavicon(browser, EMPTY_ICON_URL);
      await Promise.all([browserLoaded, iconChanged]);
      Assert.equal(browser.mIconURL, EMPTY_ICON_URL, "Favicon was changed.");

      // Give some time to ensure the icon is rendered.
      /* eslint-disable mozilla/no-arbitrary-setTimeout */
      await new Promise(resolve => setTimeout(resolve, 200));
      let secondIconShotDataURL = TestUtils.screenshotArea(iconBox, window);

      Assert.notEqual(
        firstIconShotDataURL,
        secondIconShotDataURL,
        "Check the first icon didn't persist as the second one is invalid"
      );
    }
  );
});

async function addContentLinkForIconUrl(url, browser) {
  let iconChanged = waitForFavicon(browser, url);
  info("Adding <link> to: " + url);
  await ContentTask.spawn(browser, url, href => {
    let doc = content.document;
    let head = doc.head;
    let link = doc.createElement("link");
    link.rel = "icon";
    link.href = href;
    link.type = "image/png";
    head.appendChild(link);
  });
  info("Awaiting icon change event for:" + url);
  await iconChanged;
}

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_ROOT =
  "http://mochi.test:8888/browser/browser/base/content/test/favicons/";
const ICON_URL = TEST_ROOT + "file_bug970276_favicon1.ico";
const EMPTY_URL = "data:image/x-icon";
const PAGE_URL = TEST_ROOT + "blank.html";

add_task(async function () {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: PAGE_URL },
    async browser => {
      let iconBox = gBrowser
        .getTabForBrowser(browser)
        .querySelector(".tab-icon-image");

      await addContentLinkForIconUrl(ICON_URL, browser);
      Assert.ok(
        browser.mIconURL.startsWith(
          "data:image/x-icon;base64,AAABAAEAEBAAAAAAAABoBQAA"
        ),
        "Favicon is correctly set."
      );
      let firstIconShotDataURL = TestUtils.screenshotArea(iconBox, window);

      await addContentLinkForIconUrl(EMPTY_URL, browser);
      Assert.equal(browser.mIconURL, EMPTY_URL, "Favicon is correctly set.");
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

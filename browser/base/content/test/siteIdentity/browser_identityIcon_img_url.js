/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/**
 * Test Bug 1562881 - Ensuring the identity icon loads correct img in different
 *                    circumstances.
 */

const kBaseURI = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const TEST_CASES = [
  {
    type: "http",
    testURL: "http://example.com",
    img_url: `url("chrome://browser/skin/connection-mixed-active-loaded.svg")`,
  },
  {
    type: "https",
    testURL: "https://example.com",
    img_url: `url("chrome://browser/skin/connection-secure.svg")`,
  },
  {
    type: "non-chrome about page",
    testURL: "about:about",
    img_url: `url("chrome://browser/skin/identity-icon.svg")`,
  },
  {
    type: "chrome about page",
    testURL: "about:preferences",
    img_url: `url("chrome://branding/content/identity-icons-brand.svg")`,
  },
  {
    type: "file",
    testURL: "dummy_page.html",
    img_url: `url("chrome://browser/skin/identity-icon.svg")`,
  },
  {
    type: "mixedPassiveContent",
    testURL: kBaseURI + "file_mixedPassiveContent.html",
    img_url: `url("chrome://browser/skin/connection-mixed-passive-loaded.svg")`,
  },
  {
    type: "mixedActiveContent",
    testURL: kBaseURI + "file_csp_block_all_mixedcontent.html",
    img_url: `url("chrome://browser/skin/connection-secure.svg")`,
  },
  {
    type: "certificateError",
    testURL: "https://self-signed.example.com",
    // We still show a default identity icon for cert error pages. We will
    // change it to show a not secure lock icon in Bug 1566813.
    img_url: `url("chrome://browser/skin/identity-icon.svg")`,
  },
];

add_task(async function test() {
  for (let testData of TEST_CASES) {
    info(`Testing for ${testData.type}`);
    // Open the page for testing.
    let testURL = testData.testURL;

    // Overwrite the url if it is testing the file url.
    if (testData.type === "file") {
      let dir = getChromeDir(getResolvedURI(gTestPath));
      dir.append(testURL);
      dir.normalize();
      testURL = Services.io.newFileURI(dir).spec;
    }

    let pageLoaded;
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      () => {
        gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, testURL);
        let browser = gBrowser.selectedBrowser;
        if (testData.type === "certificateError") {
          pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
        } else {
          pageLoaded = BrowserTestUtils.browserLoaded(browser);
        }
      },
      false
    );
    await pageLoaded;

    let identityIcon = document.getElementById("identity-icon");

    // Get the image url from the identity icon.
    let identityIconImageURL = gBrowser.ownerGlobal
      .getComputedStyle(identityIcon)
      .getPropertyValue("list-style-image");

    is(
      identityIconImageURL,
      testData.img_url,
      "The identity icon has a correct image url."
    );

    BrowserTestUtils.removeTab(tab);
  }
});

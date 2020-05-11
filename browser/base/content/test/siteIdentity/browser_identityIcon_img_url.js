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

const kBaseURILocalhost = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://127.0.0.1"
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
    img_url: `url("chrome://global/skin/icons/identity-icon.svg")`,
  },
  {
    type: "chrome about page",
    testURL: "about:preferences",
    img_url: `url("chrome://branding/content/identity-icons-brand.svg")`,
  },
  {
    type: "file",
    testURL: "dummy_page.html",
    img_url: `url("chrome://global/skin/icons/identity-icon.svg")`,
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
    img_url: `url("chrome://browser/skin/connection-mixed-passive-loaded.svg")`,
  },
  {
    type: "localhost",
    testURL: "http://127.0.0.1",
    img_url: `url("chrome://global/skin/icons/identity-icon.svg")`,
  },
  {
    type: "localhost + http frame",
    testURL: kBaseURILocalhost + "file_csp_block_all_mixedcontent.html",
    img_url: `url("chrome://global/skin/icons/identity-icon.svg")`,
  },
  {
    type: "data URI",
    testURL: "data:text/html,<div>",
    img_url: `url("chrome://browser/skin/connection-mixed-active-loaded.svg")`,
  },
  {
    type: "view-source HTTP",
    testURL: "view-source:http://example.com/",
    img_url: `url("chrome://browser/skin/connection-mixed-active-loaded.svg")`,
  },
  {
    type: "view-source HTTPS",
    testURL: "view-source:https://example.com/",
    // TODO this will get a secure treatment with bug 1496844.
    img_url: `url("chrome://global/skin/icons/identity-icon.svg")`,
  },
];

add_task(async function test() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // By default, proxies don't apply to 127.0.0.1. We need them to for this test, though:
      ["network.proxy.allow_hijacking_localhost", true],
    ],
  });

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

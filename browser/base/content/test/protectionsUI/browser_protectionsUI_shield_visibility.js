/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* This test checks pages of different URL variants (mostly differing in scheme)
 * and verifies that the shield is only shown when content blocking can deal
 * with the specific variant. */

const TEST_CASES = [
  {
    type: "http",
    testURL: "http://example.com",
    hidden: false,
  },
  {
    type: "https",
    testURL: "https://example.com",
    hidden: false,
  },
  {
    type: "non-chrome about page",
    testURL: "about:about",
    hidden: true,
  },
  {
    type: "chrome about page",
    testURL: "about:preferences",
    hidden: true,
  },
  {
    type: "file",
    testURL: "benignPage.html",
    hidden: true,
  },
  {
    type: "certificateError",
    testURL: "https://self-signed.example.com",
    hidden: false,
  },
  {
    type: "localhost",
    testURL: "http://127.0.0.1",
    hidden: false,
  },
  {
    type: "data URI",
    testURL: "data:text/html,<div>",
    hidden: true,
  },
  {
    type: "view-source HTTP",
    testURL: "view-source:http://example.com/",
    hidden: true,
  },
  {
    type: "view-source HTTPS",
    testURL: "view-source:https://example.com/",
    hidden: true,
  },
];

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      // By default, proxies don't apply to 127.0.0.1. We need them to for this test, though:
      ["network.proxy.allow_hijacking_localhost", true],
    ],
  });

  for (let testData of TEST_CASES) {
    info(`Testing for ${testData.type}`);
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

    is(
      BrowserTestUtils.is_hidden(
        gProtectionsHandler._trackingProtectionIconContainer
      ),
      testData.hidden,
      "tracking protection icon container is correctly displayed"
    );

    BrowserTestUtils.removeTab(tab);
  }
});

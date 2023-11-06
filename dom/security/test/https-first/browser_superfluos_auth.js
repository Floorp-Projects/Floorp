/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test checks the superfluos auth prompt when HTTPS-First is enabled (Bug 1858565).

const TEST_URI = "https://www.mozilla.org@example.com/";

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

let respondMockPromptWithYes = false;

const gMockPromptService = {
  firstTimeCalled: false,
  confirmExBC() {
    return respondMockPromptWithYes ? 0 : 1;
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
};

var gMockPromptServiceCID = MockRegistrar.register(
  "@mozilla.org/prompter;1",
  gMockPromptService
);

registerCleanupFunction(() => {
  MockRegistrar.unregister(gMockPromptServiceCID);
});

function checkBrowserLoad(browser) {
  return new Promise(resolve => {
    BrowserTestUtils.browserLoaded(browser, false, null, true).then(() => {
      resolve(true);
    });
    BrowserTestUtils.browserStopped(browser, false, null, true).then(() => {
      resolve(false);
    });
  });
}

add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  respondMockPromptWithYes = false;
  let didBrowserLoadPromise = checkBrowserLoad(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, TEST_URI);
  let didBrowserLoad = await didBrowserLoadPromise;
  ok(
    !didBrowserLoad,
    "The browser should stop the load when the user refuses to load a page with superfluos authentication"
  );

  respondMockPromptWithYes = true;
  didBrowserLoadPromise = checkBrowserLoad(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, TEST_URI);
  didBrowserLoad = await didBrowserLoadPromise;
  ok(
    didBrowserLoad,
    "The browser should load when the user agrees to load a page with superfluos authentication"
  );
});

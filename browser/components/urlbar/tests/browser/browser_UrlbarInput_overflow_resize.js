/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

async function testVal(win, url) {
  info(`Testing ${url}`);
  win.URLBarSetURI(makeURI(url));

  let urlbar = win.gURLBar;
  urlbar.blur();

  for (let width of [1000, 800]) {
    win.resizeTo(width, 500);
    await win.promiseDocumentFlushed(() => {});
    Assert.greater(urlbar.inputField.scrollWidth, urlbar.inputField.clientWidth,
                   "Check The input field overflows");
    // Resize is handled on a timer, so we must wait for it.
    await TestUtils.waitForCondition(
      () => urlbar.inputField.scrollLeft == urlbar.inputField.scrollLeftMax,
      "The urlbar input field is completely scrolled to the end");
    await TestUtils.waitForCondition(
      () => urlbar.getAttribute("textoverflow") == "start",
      "Wait for the textoverflow attribute");
  }
}

add_task(async function() {
  // We use a new tab for the test to be sure all the tab switching and loading
  // is complete before starting, otherwise onLocationChange for this tab could
  // override the value we set with an empty value.
  let win = await BrowserTestUtils.openNewBrowserWindow();
  registerCleanupFunction(() => BrowserTestUtils.closeWindow(win));

  let lotsOfSpaces = "%20".repeat(200);

  // اسماء.شبكة
  let rtlDomain = "\u0627\u0633\u0645\u0627\u0621\u002e\u0634\u0628\u0643\u0629";

  // Mix the direction of the tests to cover more cases, and to ensure the
  // textoverflow attribute changes every time, because tewtVal waits for that.
  await testVal(win, `https://${rtlDomain}/${lotsOfSpaces}/test/`);

  info("Test with formatting and trimurl disabled");
  await SpecialPowers.pushPrefEnv({set: [
    ["browser.urlbar.formatting.enabled", false],
    ["browser.urlbar.trimURLs", false],
  ]});

  await testVal(win, `https://${rtlDomain}/${lotsOfSpaces}/test/`);
  await testVal(win, `http://${rtlDomain}/${lotsOfSpaces}/test/`);
});

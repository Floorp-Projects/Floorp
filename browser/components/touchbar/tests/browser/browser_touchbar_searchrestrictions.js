/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.defineESModuleGetters(this, {
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.sys.mjs",
});

ChromeUtils.defineLazyGetter(this, "UrlbarTestUtils", () => {
  const { UrlbarTestUtils: module } = ChromeUtils.importESModule(
    "resource://testing-common/UrlbarTestUtils.sys.mjs"
  );
  module.init(this);
  return module;
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);

/**
 * Tests the search restriction buttons in the Touch Bar.
 */

/**
 * @param {object} options
 * @param {string} options.input
 *   The value to be inserted in the Urlbar.
 * @param {UrlbarTokenizer.RESTRICT} options.token
 *   A restriction token corresponding to a Touch Bar button.
 */
async function searchAndCheckState({ input, token }) {
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: input,
  });
  input = input.trimStart();
  if (Object.values(UrlbarTokenizer.RESTRICT).includes(input[0])) {
    input = input.slice(1).trimStart();
  }
  let searchMode = UrlbarUtils.searchModeForToken(token);
  let expectedValue = searchMode ? input : `${token} ${input}`;
  TouchBarHelper.insertRestrictionInUrlbar(token);

  if (searchMode) {
    searchMode.entry = "touchbar";
    await UrlbarTestUtils.assertSearchMode(window, searchMode);
  }
  Assert.equal(
    gURLBar.value,
    expectedValue,
    "The search restriction token should have been entered."
  );

  await UrlbarTestUtils.promisePopupClose(window);
}

add_task(async function insertTokens() {
  const tests = [
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.OPENPAGE,
    },
  ];
  for (let test of tests) {
    await searchAndCheckState(test);
  }
});

add_task(async function existingTokens() {
  const tests = [
    {
      input: "* mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
    },
    {
      input: "+ mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
    },
    {
      input: "( $ ^ mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
    },
    {
      input: "^*+%?#$ mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
    },
  ];
  for (let test of tests) {
    await searchAndCheckState(test);
  }
});

add_task(async function stripSpaces() {
  const tests = [
    {
      input: "     ^     mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
    },
    {
      input: "     +         mozilla   ",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
    },
    {
      input: "  moz    illa  ",
      token: UrlbarTokenizer.RESTRICT.TAG,
    },
  ];
  for (let test of tests) {
    await searchAndCheckState(test);
  }
});

add_task(async function clearURLs() {
  const tests = [
    {
      loadUrl: "http://example.com/",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
    },
    {
      loadUrl: "about:mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  await UrlbarTestUtils.promisePopupClose(win);
  for (let { loadUrl, token } of tests) {
    let browser = win.gBrowser.selectedBrowser;
    let loadedPromise = BrowserTestUtils.browserLoaded(browser, false, loadUrl);
    BrowserTestUtils.startLoadingURIString(browser, loadUrl);
    await loadedPromise;
    if (win.gURLBar.getAttribute("pageproxystate") != "valid") {
      await TestUtils.waitForCondition(
        () => win.gURLBar.getAttribute("pageproxystate") == "valid"
      );
    }
    await searchAndCheckState({ input: "", token });
  }
});

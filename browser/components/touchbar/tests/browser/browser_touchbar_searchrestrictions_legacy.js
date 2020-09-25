/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTestUtils: "resource://testing-common/UrlbarTestUtils.jsm",
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);

/**
 * Tests the search restriction buttons in the Touch Bar.
 * This test can be removed after the browser.urlbar.update2 pref is removed.
 */

add_task(async function init() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.urlbar.update2", false]],
  });
});

add_task(async function insertTokens() {
  const tests = [
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ mozilla",
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expected: "* mozilla",
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
      expected: "+ mozilla",
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.OPENPAGE,
      expected: "% mozilla",
    },
    {
      input: "mozilla",
      token: UrlbarTokenizer.RESTRICT.TITLE,
      expected: "# mozilla",
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  for (let { input, token, expected } of tests) {
    win.gURLBar.search(input);
    await UrlbarTestUtils.promiseSearchComplete(win);
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been entered."
    );
    await UrlbarTestUtils.promisePopupClose(win);
  }
});

add_task(async function existingTokens() {
  const tests = [
    {
      input: "* mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ mozilla",
    },
    {
      input: "+ mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expected: "* mozilla",
    },
    {
      input: "( $ ^ mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
      expected: "+ ( $ ^ mozilla",
    },
    {
      input: "^*+%?#$ mozilla",
      token: UrlbarTokenizer.RESTRICT.TAG,
      expected: "+ *+%?#$ mozilla",
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  for (let { input, token, expected } of tests) {
    win.gURLBar.search(input);
    await UrlbarTestUtils.promiseSearchComplete(win);
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been replaced."
    );
    await UrlbarTestUtils.promisePopupClose(win);
  }
});

add_task(async function stripSpaces() {
  const tests = [
    {
      input: "     ^     mozilla",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ mozilla",
    },
    {
      input: "     +         mozilla   ",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expected: "* mozilla   ",
    },
    {
      input: "  moz    illa  ",
      token: UrlbarTokenizer.RESTRICT.TAG,
      expected: "+ moz    illa  ",
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  for (let { input, token, expected } of tests) {
    win.gURLBar.search(input);
    await UrlbarTestUtils.promiseSearchComplete(win);
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been entered " +
        "with stripped whitespace."
    );
    await UrlbarTestUtils.promisePopupClose(win);
  }
});

add_task(async function clearURLs() {
  const tests = [
    {
      loadUrl: "http://example.com/",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ ",
    },
    {
      loadUrl: "about:mozilla",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expected: "* ",
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  await UrlbarTestUtils.promisePopupClose(win);
  for (let { loadUrl, token, expected } of tests) {
    let browser = win.gBrowser.selectedBrowser;
    let loadedPromise = BrowserTestUtils.browserLoaded(browser, false, loadUrl);
    BrowserTestUtils.loadURI(browser, loadUrl);
    await loadedPromise;
    await TestUtils.waitForCondition(
      () => win.gURLBar.getAttribute("pageproxystate") == "valid"
    );
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have cleared out the URL."
    );
  }
});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  UrlbarTokenizer: "resource:///modules/UrlbarTokenizer.jsm",
});

XPCOMUtils.defineLazyServiceGetter(
  this,
  "TouchBarHelper",
  "@mozilla.org/widget/touchbarhelper;1",
  "nsITouchBarHelper"
);

add_task(async function insertTokens() {
  const tests = [
    {
      input: "",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ ",
    },
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
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been entered."
    );
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
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been replaced."
    );
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
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have been entered " +
        "with stripped whitespace."
    );
  }
});

add_task(async function clearURLs() {
  const tests = [
    {
      loadUrl: "https://example.com",
      token: UrlbarTokenizer.RESTRICT.HISTORY,
      expected: "^ ",
    },
    {
      loadUrl: "about:blank",
      token: UrlbarTokenizer.RESTRICT.BOOKMARK,
      expected: "* ",
    },
  ];
  let win = BrowserWindowTracker.getTopWindow();
  for (let { loadUrl, token, expected } of tests) {
    let loadedPromise = BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser
    );
    BrowserTestUtils.loadURI(gBrowser.selectedBrowser, loadUrl);
    await loadedPromise;
    TouchBarHelper.insertRestrictionInUrlbar(token);
    Assert.equal(
      win.gURLBar.value,
      expected,
      "The search restriction token should have cleared out the URL."
    );
  }
});

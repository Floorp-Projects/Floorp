/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const searchclipboardforPref = "browser.tabs.searchclipboardfor.middleclick";

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      [searchclipboardforPref, true],
      // set preloading to false so we can await the new tab being opened.
      ["browser.newtab.preload", false],
    ],
  });
  NewTabPagePreloading.removePreloadedBrowser(window);
  // Create an engine to use for the test.
  SearchTestUtils.init(this);
  await SearchTestUtils.installSearchExtension(
    {
      name: "MozSearch",
      search_url: "https://example.org/",
      search_url_get_params: "q={searchTerms}",
    },
    { setAsDefaultPrivate: true }
  );
  // We overflow tabs, close all the extra ones.
  registerCleanupFunction(() => {
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.selectedTab);
    }
  });
});

add_task(async function middleclick_tabs_newtab_button_with_url_in_clipboard() {
  var previousTabsLength = gBrowser.tabs.length;
  info("Previous tabs count is " + previousTabsLength);

  let url = "javascript:https://www.example.com/";
  let safeUrl = "https://www.example.com/";
  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      url,
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString(url);
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });

  info("Middle clicking 'new tab' button");
  let promiseTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    safeUrl,
    true
  );
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("tabs-newtab-button"),
    { button: 1 }
  );

  await promiseTabLoaded;
  is(gBrowser.tabs.length, previousTabsLength + 1, "We created a tab");
  is(
    gBrowser.currentURI.spec,
    safeUrl,
    "New Tab URL is the safe content of the clipboard"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(
  async function middleclick_tabs_newtab_button_with_word_in_clipboard() {
    var previousTabsLength = gBrowser.tabs.length;
    info("Previous tabs count is " + previousTabsLength);

    let word = "word";
    let searchUrl = "https://example.org/?q=word";
    await new Promise((resolve, reject) => {
      SimpleTest.waitForClipboard(
        word,
        () => {
          Cc["@mozilla.org/widget/clipboardhelper;1"]
            .getService(Ci.nsIClipboardHelper)
            .copyString(word);
        },
        resolve,
        () => {
          ok(false, "Clipboard copy failed");
          reject();
        }
      );
    });

    info("Middle clicking 'new tab' button");
    let promiseTabLoaded = BrowserTestUtils.waitForNewTab(
      gBrowser,
      searchUrl,
      true
    );
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("tabs-newtab-button"),
      { button: 1 }
    );

    await promiseTabLoaded;
    is(gBrowser.tabs.length, previousTabsLength + 1, "We created a tab");
    is(
      gBrowser.currentURI.spec,
      searchUrl,
      "New Tab URL is the search engine with the content of the clipboard"
    );

    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  }
);

add_task(async function middleclick_new_tab_button_with_url_in_clipboard() {
  await BrowserTestUtils.overflowTabs(registerCleanupFunction, window);
  await BrowserTestUtils.waitForCondition(() => {
    return Array.from(gBrowser.tabs).every(tab => tab._fullyOpen);
  });

  var previousTabsLength = gBrowser.tabs.length;
  info("Previous tabs count is " + previousTabsLength);

  let url = "javascript:https://www.example.com/";
  let safeUrl = "https://www.example.com/";
  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      url,
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString(url);
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });

  info("Middle clicking 'new tab' button");
  let promiseTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    safeUrl,
    true
  );
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("new-tab-button"),
    { button: 1 }
  );

  await promiseTabLoaded;
  is(gBrowser.tabs.length, previousTabsLength + 1, "We created a tab");
  is(
    gBrowser.currentURI.spec,
    safeUrl,
    "New Tab URL is the safe content of the clipboard"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function middleclick_new_tab_button_with_word_in_clipboard() {
  var previousTabsLength = gBrowser.tabs.length;
  info("Previous tabs count is " + previousTabsLength);

  let word = "word";
  let searchUrl = "https://example.org/?q=word";
  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      word,
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString(word);
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });

  info("Middle clicking 'new tab' button");
  let promiseTabLoaded = BrowserTestUtils.waitForNewTab(
    gBrowser,
    searchUrl,
    true
  );
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("new-tab-button"),
    { button: 1 }
  );

  await promiseTabLoaded;
  is(gBrowser.tabs.length, previousTabsLength + 1, "We created a tab");
  is(
    gBrowser.currentURI.spec,
    searchUrl,
    "New Tab URL is the search engine with the content of the clipboard"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function middleclick_new_tab_button_with_spaces_in_clipboard() {
  let spaces = "    \n    ";
  await new Promise((resolve, reject) => {
    SimpleTest.waitForClipboard(
      spaces,
      () => {
        Cc["@mozilla.org/widget/clipboardhelper;1"]
          .getService(Ci.nsIClipboardHelper)
          .copyString(spaces);
      },
      resolve,
      () => {
        ok(false, "Clipboard copy failed");
        reject();
      }
    );
  });

  info("Middle clicking 'new tab' button");
  let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser);
  EventUtils.synthesizeMouseAtCenter(
    document.getElementById("new-tab-button"),
    { button: 1 }
  );

  await promiseTabOpened;
  is(
    gBrowser.currentURI.spec,
    "about:newtab",
    "New Tab URL is the regular new tab page."
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

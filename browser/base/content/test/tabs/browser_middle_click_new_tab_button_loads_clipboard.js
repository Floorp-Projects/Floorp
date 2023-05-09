/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const searchclipboardforPref = "browser.tabs.searchclipboardfor.middleclick";

const { SearchTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/SearchTestUtils.sys.mjs"
);

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({ set: [[searchclipboardforPref, true]] });
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
  try {
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("tabs-newtab-button"),
      { button: 1 }
    );
  } catch (error) {
    info(error);
  }

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
    try {
      EventUtils.synthesizeMouseAtCenter(
        document.getElementById("tabs-newtab-button"),
        { button: 1 }
      );
    } catch (error) {
      info(error);
    }

    await promiseTabLoaded;
    info(gBrowser.currentURI.spec);
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
  try {
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("new-tab-button"),
      { button: 1 }
    );
  } catch (error) {
    info(error);
  }

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
  try {
    EventUtils.synthesizeMouseAtCenter(
      document.getElementById("new-tab-button"),
      { button: 1 }
    );
  } catch (error) {
    info(error);
  }

  await promiseTabLoaded;
  info(gBrowser.currentURI.spec);
  is(gBrowser.tabs.length, previousTabsLength + 1, "We created a tab");
  is(
    gBrowser.currentURI.spec,
    searchUrl,
    "New Tab URL is the search engine with the content of the clipboard"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

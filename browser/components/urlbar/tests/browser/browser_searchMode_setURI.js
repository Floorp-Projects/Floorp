/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that search mode remains active or is exited when setURI is called,
 * depending on the situation.  For example, loading a page in the current tab
 * should exit search mode, due to calling setURI.
 */

"use strict";

const BOOKMARK_URL = "http://www.example.com/browser_searchMode_setURI.js";
const LINK_PAGE_URL =
  "http://mochi.test:8888/browser/browser/components/urlbar/tests/browser/dummy_page.html";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.urlbar.update2", true],
      ["browser.urlbar.update2.localOneOffs", true],
      ["browser.urlbar.update2.oneOffsRefresh", true],
    ],
  });

  // Add a bookmark so we can enter bookmarks search mode and pick it.
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    url: BOOKMARK_URL,
  });
  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });

  if (gURLBar.getAttribute("pageproxystate") == "invalid") {
    gURLBar.handleRevert();
  }
});

// Opens a new tab, enters search mode, does a search for our test bookmark, and
// picks it.  Uses a variety of initial URLs and search strings in order to hit
// different branches in setURI.  Search mode should be exited in all cases.
add_task(async function pickResult() {
  for (let test of [
    // initialURL, searchString
    ["about:blank", BOOKMARK_URL],
    ["about:blank", new URL(BOOKMARK_URL).origin],
    ["about:blank", new URL(BOOKMARK_URL).pathname],
    [BOOKMARK_URL, BOOKMARK_URL],
    [BOOKMARK_URL, new URL(BOOKMARK_URL).origin],
    [BOOKMARK_URL, new URL(BOOKMARK_URL).pathname],
  ]) {
    await doPickResultTest(...test);
  }
});

async function doPickResultTest(initialURL, searchString) {
  info(
    "doPickResultTest with args: " +
      JSON.stringify({
        initialURL,
        searchString,
      })
  );

  await BrowserTestUtils.withNewTab(initialURL, async () => {
    await UrlbarTestUtils.promiseAutocompleteResultPopup({
      window,
      value: searchString,
      fireInputEvent: true,
    });

    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    // Arrow down to the bookmark result.
    let foundResult = false;
    for (let i = 0; i < UrlbarTestUtils.getResultCount(window); i++) {
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
      if (
        result.source == UrlbarUtils.RESULT_SOURCE.BOOKMARKS &&
        result.url == BOOKMARK_URL
      ) {
        foundResult = true;
        break;
      }
      EventUtils.synthesizeKey("KEY_ArrowDown");
    }
    Assert.ok(foundResult, "The bookmark result should have been found");

    // Press enter.
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await loadPromise;
    Assert.equal(
      gBrowser.currentURI.spec,
      BOOKMARK_URL,
      "Should have loaded the bookmarked URL"
    );

    UrlbarTestUtils.assertSearchMode(window, null);
  });
}

// Opens a new tab containing a link, enters search mode, and clicks the link.
// Uses a variety of search strings and link hrefs in order to hit different
// branches in setURI.  Search mode should be exited in all cases, and the href
// in the link should be opened.
add_task(async function clickLink() {
  for (let test of [
    // searchString, href to use in the link
    [LINK_PAGE_URL, LINK_PAGE_URL],
    [LINK_PAGE_URL, "http://www.example.com/"],
    ["test", LINK_PAGE_URL],
    ["test", "http://www.example.com/"],
    [null, LINK_PAGE_URL],
    [null, "http://www.example.com/"],
  ]) {
    await doClickLinkTest(...test);
  }
});

async function doClickLinkTest(searchString, href) {
  info(
    "doClickLinkTest with args: " +
      JSON.stringify({
        searchString,
        href,
      })
  );

  await BrowserTestUtils.withNewTab(LINK_PAGE_URL, async () => {
    if (searchString) {
      // Do a search with the search string.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: searchString,
        fireInputEvent: true,
      });
      Assert.ok(
        gBrowser.selectedBrowser.userTypedValue,
        "userTypedValue should be defined"
      );
    } else {
      // Open top sites.
      await UrlbarTestUtils.promisePopupOpen(window, () => {
        document.getElementById("Browser:OpenLocation").doCommand();
      });
      Assert.strictEqual(
        gBrowser.selectedBrowser.userTypedValue,
        null,
        "userTypedValue should be null"
      );
    }

    // Enter search mode and then close the popup so we can click the link in
    // the page.
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    await UrlbarTestUtils.promisePopupClose(window);
    UrlbarTestUtils.assertSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });

    // Add a link to the page and click it.
    let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
    await ContentTask.spawn(gBrowser.selectedBrowser, href, async cHref => {
      let link = this.content.document.createElement("a");
      link.textContent = "Click me";
      link.href = cHref;
      this.content.document.body.append(link);
      link.click();
    });
    await loadPromise;
    Assert.equal(
      gBrowser.currentURI.spec,
      href,
      "Should have loaded the href URL"
    );

    UrlbarTestUtils.assertSearchMode(window, null);
  });
}

// Opens a new tab, does a search, enters search mode, and then manually calls
// setURI.  Uses a variety of initial URLs, search strings, and setURI arguments
// in order to hit different branches in setURI.  Search mode should remain
// active or be exited as appropriate.
add_task(async function setURI() {
  for (let test of [
    // initialURL, searchString, url, expectSearchMode

    ["about:blank", null, null, true],
    ["about:blank", null, "about:blank", true],
    ["about:blank", null, "http://www.example.com/", false],

    ["about:blank", "about:blank", null, false],
    ["about:blank", "about:blank", "about:blank", false],
    ["about:blank", "about:blank", "http://www.example.com/", false],

    ["about:blank", "http://www.example.com/", null, true],
    ["about:blank", "http://www.example.com/", "about:blank", true],
    ["about:blank", "http://www.example.com/", "http://www.example.com/", true],

    ["about:blank", "not a URL", null, true],
    ["about:blank", "not a URL", "about:blank", true],
    ["about:blank", "not a URL", "http://www.example.com/", true],

    ["http://www.example.com/", null, null, false],
    ["http://www.example.com/", null, "about:blank", true],
    ["http://www.example.com/", null, "http://www.example.com/", false],

    ["http://www.example.com/", "about:blank", null, false],
    ["http://www.example.com/", "about:blank", "about:blank", false],
    [
      "http://www.example.com/",
      "about:blank",
      "http://www.example.com/",
      false,
    ],

    ["http://www.example.com/", "http://www.example.com/", null, true],
    ["http://www.example.com/", "http://www.example.com/", "about:blank", true],
    [
      "http://www.example.com/",
      "http://www.example.com/",
      "http://www.example.com/",
      true,
    ],

    ["http://www.example.com/", "not a URL", null, true],
    ["http://www.example.com/", "not a URL", "about:blank", true],
    ["http://www.example.com/", "not a URL", "http://www.example.com/", true],
  ]) {
    await doSetURITest(...test);
  }
});

async function doSetURITest(initialURL, searchString, url, expectSearchMode) {
  info(
    "doSetURITest with args: " +
      JSON.stringify({
        initialURL,
        searchString,
        url,
        expectSearchMode,
      })
  );

  await BrowserTestUtils.withNewTab(initialURL, async () => {
    if (searchString) {
      // Do a search with the search string.
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: searchString,
        fireInputEvent: true,
      });
    } else {
      // Open top sites.
      await UrlbarTestUtils.promisePopupOpen(window, () => {
        document.getElementById("Browser:OpenLocation").doCommand();
      });
    }

    // Enter search mode and close the view.
    await UrlbarTestUtils.enterSearchMode(window, {
      source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
    });
    await UrlbarTestUtils.promisePopupClose(window);
    Assert.strictEqual(
      gBrowser.selectedBrowser.userTypedValue,
      searchString || null,
      `userTypedValue should be ${searchString || null}`
    );

    // Call setURI.
    let uri = url ? Services.io.newURI(url) : null;
    gURLBar.setURI(uri);

    UrlbarTestUtils.assertSearchMode(
      window,
      !expectSearchMode
        ? null
        : {
            source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
          }
    );

    gURLBar.handleRevert();
    UrlbarTestUtils.assertSearchMode(window, null);
  });
}

// Tests that search mode is stored per tab and restored when switching tabs.
// Restoration is handled in setURI, which is why this task is in this file.
add_task(async function tabSwitch() {
  // Open three tabs.  We'll enter search mode in tabs 0 and 2.
  let tabs = [];
  for (let i = 0; i < 3; i++) {
    let tab = await BrowserTestUtils.openNewForegroundTab({
      gBrowser,
      url: "http://example.com/" + i,
    });
    tabs.push(tab);
  }

  // Switch to tab 0.
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);

  // Do a search and enter search mode.  Pass fireInputEvent so that
  // userTypedValue is set and restored when we switch back to this tab.  This
  // isn't really necessary but it simulates the user's typing, and it also
  // means that we'll start a search when we switch back to this tab.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search (for "test") and re-enter
  // search mode.
  let searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 2.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Do another search (in tab 2) and enter search mode.  Use a different source
  // from tab 0 just to use something different.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "test tab 2",
    fireInputEvent: true,
  });
  await UrlbarTestUtils.enterSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
  });

  // Switch back to tab 0.  We should do a search and still be in search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch to tab 1.  Search mode should be exited.
  await BrowserTestUtils.switchTab(gBrowser, tabs[1]);
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 2.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.TABS,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search and re-enter search mode.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, {
    source: UrlbarUtils.RESULT_SOURCE.BOOKMARKS,
  });

  // Exit search mode.
  await UrlbarTestUtils.exitSearchMode(window, { clickClose: true });

  // Switch back to tab 2.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[2]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  // Switch back to tab 0.  We should do a search but search mode should be
  // inactive.
  searchPromise = UrlbarTestUtils.promiseSearchComplete(window);
  await BrowserTestUtils.switchTab(gBrowser, tabs[0]);
  await searchPromise;
  UrlbarTestUtils.assertSearchMode(window, null);

  await UrlbarTestUtils.promisePopupClose(window);
  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
});

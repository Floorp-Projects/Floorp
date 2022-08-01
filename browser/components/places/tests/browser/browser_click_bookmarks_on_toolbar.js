/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_LOAD_BOOKMARKS_IN_TABS = "browser.tabs.loadBookmarksInTabs";
const EXAMPLE_PAGE = "http://example.com/";
const TEST_PAGES = [
  "about:mozilla",
  "about:robots",
  "javascript:window.location=%22" + EXAMPLE_PAGE + "%22",
];

var gBookmarkElements = [];

function waitForBookmarkElements(expectedCount) {
  let container = document.getElementById("PlacesToolbarItems");
  if (container.childElementCount == expectedCount) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    info("Waiting for bookmarks");
    let mut = new MutationObserver(mutations => {
      info("Elements appeared");
      if (container.childElementCount == expectedCount) {
        resolve();
        mut.disconnect();
      }
    });

    mut.observe(container, { childList: true });
  });
}

function getToolbarNodeForItemGuid(aItemGuid) {
  var children = document.getElementById("PlacesToolbarItems").children;
  for (let child of children) {
    if (aItemGuid == child._placesNode.bookmarkGuid) {
      return child;
    }
  }
  return null;
}

function waitForLoad(browser, url) {
  return BrowserTestUtils.browserLoaded(browser, false, url);
}

function waitForNewTab(url, inBackground) {
  return BrowserTestUtils.waitForNewTab(gBrowser, url).then(tab => {
    if (inBackground) {
      Assert.notEqual(
        tab,
        gBrowser.selectedTab,
        `The new tab is in the background`
      );
    } else {
      Assert.equal(
        tab,
        gBrowser.selectedTab,
        `The new tab is in the foreground`
      );
    }

    BrowserTestUtils.removeTab(tab);
  });
}

add_setup(async function() {
  await PlacesUtils.bookmarks.eraseEverything();
  let bookmarks = await Promise.all(
    TEST_PAGES.map((url, index) => {
      return PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.toolbarGuid,
        title: `Title ${index}`,
        url,
      });
    })
  );

  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  await waitForBookmarkElements(TEST_PAGES.length);
  for (let bookmark of bookmarks) {
    let element = getToolbarNodeForItemGuid(bookmark.guid);
    Assert.notEqual(element, null, "Found node on toolbar");

    gBookmarkElements.push(element);
  }

  registerCleanupFunction(async () => {
    gBookmarkElements = [];

    await Promise.all(
      bookmarks.map(bookmark => {
        return PlacesUtils.bookmarks.remove(bookmark);
      })
    );

    // Note: hiding the toolbar before removing the bookmarks triggers
    // bug 1766284.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }
  });
});

add_task(async function click() {
  let promise = waitForLoad(gBrowser.selectedBrowser, TEST_PAGES[0]);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 0,
  });
  await promise;

  promise = waitForNewTab(TEST_PAGES[1], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 0,
    accelKey: true,
  });
  await promise;

  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:blank",
  });
  promise = waitForLoad(gBrowser.selectedBrowser, EXAMPLE_PAGE);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[2], {});
  await promise;
  BrowserTestUtils.removeTab(tab);
});

add_task(async function middleclick() {
  let promise = waitForNewTab(TEST_PAGES[0], true);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 1,
    shiftKey: true,
  });
  await promise;

  promise = waitForNewTab(TEST_PAGES[1], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 1,
  });
  await promise;
});

add_task(async function clickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_LOAD_BOOKMARKS_IN_TABS, true]],
  });

  let promise = waitForNewTab(TEST_PAGES[0], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 0,
  });
  await promise;

  // With loadBookmarksInTabs, reuse current tab if blank
  for (let button of [0, 1]) {
    await BrowserTestUtils.withNewTab({ gBrowser }, async tab => {
      promise = waitForLoad(gBrowser.selectedBrowser, TEST_PAGES[1]);
      EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
        button,
      });
      await promise;
    });
  }

  await SpecialPowers.popPrefEnv();
});

add_task(async function openInSameTabWithPrefSet() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_LOAD_BOOKMARKS_IN_TABS, true]],
  });

  let placesContext = document.getElementById("placesContext");
  let popupPromise = BrowserTestUtils.waitForEvent(placesContext, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 2,
    type: "contextmenu",
  });
  info("Waiting for context menu");
  await popupPromise;

  let openItem = document.getElementById("placesContext_open");
  ok(BrowserTestUtils.is_visible(openItem), "Open item should be visible");

  info("Waiting for page to load");
  let promise = waitForLoad(gBrowser.selectedBrowser, TEST_PAGES[0]);
  openItem.doCommand();
  placesContext.hidePopup();
  await promise;

  await SpecialPowers.popPrefEnv();
});

// Open a tab, then quickly open the context menu to ensure that the command
// enabled state of the menuitems is updated properly.
add_task(async function quickContextMenu() {
  await SpecialPowers.pushPrefEnv({
    set: [[PREF_LOAD_BOOKMARKS_IN_TABS, true]],
  });

  let tabPromise = BrowserTestUtils.waitForNewTab(gBrowser, TEST_PAGES[0]);

  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 0,
  });
  let newTab = await tabPromise;

  let placesContext = document.getElementById("placesContext");
  let promise = BrowserTestUtils.waitForEvent(placesContext, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 2,
    type: "contextmenu",
  });
  await promise;

  Assert.ok(
    !document.getElementById("placesContext_open:newtab").disabled,
    "Commands in context menu are enabled"
  );

  promise = BrowserTestUtils.waitForEvent(placesContext, "popuphidden");
  placesContext.hidePopup();
  await promise;
  BrowserTestUtils.removeTab(newTab);

  await SpecialPowers.popPrefEnv();
});

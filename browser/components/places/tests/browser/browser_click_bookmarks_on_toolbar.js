/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PREF_LOAD_BOOKMARKS_IN_TABS = "browser.tabs.loadBookmarksInTabs";
const TEST_PAGES = ["about:mozilla", "about:robots"];

var gBookmarkElements = [];

function getToolbarNodeForItemGuid(aItemGuid) {
  var children = document.getElementById("PlacesToolbarItems").childNodes;
  for (let child of children) {
    if (aItemGuid == child._placesNode.bookmarkGuid) {
      return child;
    }
  }
  return null;
}

function waitForLoad(browser, url) {
  return BrowserTestUtils.browserLoaded(browser, false, url).then(() => {
    return BrowserTestUtils.loadURI(browser, "about:blank");
  });
}

function waitForNewTab(url, inBackground) {
  return BrowserTestUtils.waitForNewTab(gBrowser, url).then(tab => {
    if (inBackground) {
      Assert.notEqual(tab,
        gBrowser.selectedTab, `The new tab is in the background`);
    } else {
      Assert.equal(tab,
        gBrowser.selectedTab, `The new tab is in the foreground`);
    }

    return BrowserTestUtils.removeTab(tab);
  });
}

add_task(async function setup() {
  let bookmarks = await Promise.all(TEST_PAGES.map((url, index) => {
    return PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.toolbarGuid,
      title: `Title ${index}`,
      url
    });
  }));

  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  for (let bookmark of bookmarks) {
    let element = getToolbarNodeForItemGuid(bookmark.guid);
    Assert.notEqual(element, null, "Found node on toolbar");

    gBookmarkElements.push(element);
  }

  registerCleanupFunction(async () => {
    gBookmarkElements = [];

    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }

    await Promise.all(bookmarks.map(bookmark => {
      return PlacesUtils.bookmarks.remove(bookmark);
    }));
  });
});

add_task(async function click() {
  let promise = waitForLoad(gBrowser.selectedBrowser, TEST_PAGES[0]);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 0
  });
  await promise;

  promise = waitForNewTab(TEST_PAGES[1], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 0, accelKey: true
  });
  await promise;
});

add_task(async function middleclick() {
  let promise = waitForNewTab(TEST_PAGES[0], true);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 1, shiftKey: true
  });
  await promise;

  promise = waitForNewTab(TEST_PAGES[1], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 1
  });
  await promise;
});

add_task(async function clickWithPrefSet() {
  await SpecialPowers.pushPrefEnv({set: [
    [PREF_LOAD_BOOKMARKS_IN_TABS, true]
  ]});

  let promise = waitForNewTab(TEST_PAGES[0], false);
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[0], {
    button: 0
  });
  await promise;

  let placesContext = document.getElementById("placesContext");
  promise = BrowserTestUtils.waitForEvent(placesContext, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBookmarkElements[1], {
    button: 2,
    type: "contextmenu"
  });
  await promise;

  promise = waitForLoad(gBrowser.selectedBrowser, TEST_PAGES[1]);
  let open = document.getElementById("placesContext_open");
  EventUtils.synthesizeMouseAtCenter(open, {
    button: 0
  });
  await promise;

  await SpecialPowers.popPrefEnv();
});

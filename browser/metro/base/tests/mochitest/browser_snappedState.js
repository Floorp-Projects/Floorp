// -*- Mode: js2; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var gStartDoc = null;

function test() {
  if (!isLandscapeMode()) {
    todo(false, "browser_snapped_tests need landscape mode to run.");
    return;
  }

  runTests();
}

function showStartUI() {
  if (!BrowserUI.isStartTabVisible) {
    let tab = yield addTab("about:start");
    gStartDoc = tab.browser.contentWindow.document;
  } else {
    gStartDoc = Browser.selectedBrowser.contentWindow.document;
  }
}

function setUpSnapped() {
  yield showStartUI();
  yield setSnappedViewstate();
}

function setUpPortrait() {
  yield showStartUI();
  yield setPortraitViewstate();
}

function getNarrowTitle(aVboxId) {
  let vbox = gStartDoc.getElementById(aVboxId);
  return vbox.querySelector(".narrow-title");
}

function narrowTitleVisible(aVboxId) {
  let title = getNarrowTitle(aVboxId);
  let display = getComputedStyle(title).getPropertyValue("display");

  return display !== "none";
}

function wideTitleVisible(aVboxId) {
  let vbox = gStartDoc.getElementById(aVboxId);
  let title = vbox.querySelector(".wide-title");
  let display = getComputedStyle(title).getPropertyValue("display");

  return display !== "none";
}

gTests.push({
  desc: "Test Snapped titles",
  setUp: setUpSnapped,
  run: function() {
    ok(narrowTitleVisible("start-topsites"), "topsites narrow title is visible");
    ok(narrowTitleVisible("start-bookmarks"), "bookmarks narrow title is visible");
    ok(narrowTitleVisible("start-history"), "history narrow title is visible");

    ok(!wideTitleVisible("start-topsites"), "topsites wide title is not visible");
    ok(!wideTitleVisible("start-bookmarks"), "bookmarks wide title is not visible");
    ok(!wideTitleVisible("start-history"), "history wide title is not visible");
  },
  tearDown: restoreViewstate
});

gTests.push({
  desc: "Test Snapped titles",
  setUp: setUpSnapped,
  run: function() {
    let topsites = gStartDoc.getElementById("start-topsites");
    let bookmarks = gStartDoc.getElementById("start-bookmarks");
    let history = gStartDoc.getElementById("start-history");

    ok(topsites.hasAttribute("expanded"), "topsites is expanded");
    ok(!bookmarks.hasAttribute("expanded"), "bookmarks is collapsed");
    ok(!history.hasAttribute("expanded"), "history is collapsed");

    // Expand bookmarks
    sendElementTap(Browser.selectedBrowser.contentWindow, getNarrowTitle("start-bookmarks"));

    yield waitForCondition(() => bookmarks.hasAttribute("expanded"));

    ok(!topsites.hasAttribute("expanded"), "topsites is collapsed");
    ok(bookmarks.hasAttribute("expanded"), "bookmarks is expanded");
    ok(!history.hasAttribute("expanded"), "history is collapsed");
  },
  tearDown: restoreViewstate
});

gTests.push({
  desc: "Test Snapped scrolls vertically",
  setUp: function() {

    // Populate with mock data and expand bookmarks
    BookmarksTestHelper.setup();
    sendElementTap(Browser.selectedBrowser.contentWindow, getNarrowTitle("start-bookmarks"));

    yield waitForCondition(() => gStartDoc.getElementById("start-bookmarks").hasAttribute("expanded"));

    yield setUpSnapped();
  },
  run: function() {
    ok(Browser.selectedBrowser.contentWindow.scrollMaxY !== 0, "Snapped scrolls vertically");
    ok(Browser.selectedBrowser.contentWindow.scrollMaxX === 0, "Snapped does not scroll horizontally");
  },
  tearDown: function() {
    BookmarksTestHelper.restore();
    yield restoreViewstate();
  }
});
gTests.push({
  desc: "Test tile selection is cleared and disabled",
  setUp: function() {
    BookmarksTestHelper.setup();
    HistoryTestHelper.setup();
    showStartUI();
  },
  run: function() {
    // minimal event mocking to trigger context-click handlers
    function makeMockEvent(item) {
      return {
        stopPropagation: function() {},
        target: item
      };
    }
    let startWin = Browser.selectedBrowser.contentWindow;
    // make sure the bookmarks grid is showing
    startWin.StartUI.onNarrowTitleClick("start-bookmarks");
    let bookmarksGrid = startWin.document.querySelector("#start-bookmarks-grid");
    // sanity check
    ok(bookmarksGrid, "matched bookmarks grid");
    ok(bookmarksGrid.children[0], "bookmarks grid has items");
    // select a tile (balancing implementation leakage with test simplicity)
    let mockEvent = makeMockEvent(bookmarksGrid.children[0]);
    bookmarksGrid.handleItemContextMenu(bookmarksGrid.children[0], mockEvent);
    // check tile was selected
    is(bookmarksGrid.selectedItems.length, 1, "Tile got selected in landscape view");
    // switch to snapped view
    yield setSnappedViewstate();
    is(bookmarksGrid.selectedItems.length, 0, "grid items selection cleared in snapped view");
    // attempt to select a tile in snapped view
    mockEvent = makeMockEvent(bookmarksGrid.children[0]);
    bookmarksGrid.handleItemContextMenu(bookmarksGrid.children[0], mockEvent);
    is(bookmarksGrid.selectedItems.length, 0, "no grid item selections possible in snapped view");
  },
  tearDown: function() {
    BookmarksTestHelper.restore();
    HistoryTestHelper.restore();
    yield restoreViewstate();
  }
});

gTests.push({
  desc: "Navbar contextual buttons are not shown in snapped",
  setUp: setUpSnapped,
  run: function() {
    let toolbarContextual = document.getElementById("toolbar-contextual");
    let visibility = getComputedStyle(toolbarContextual).getPropertyValue("visibility");
    ok(visibility === "collapse" || visibility === "hidden", "Contextual buttons not shown in navbar");
  },
  tearDown: restoreViewstate
});

gTests.push({
  desc: "Test Portrait titles",
  setUp: setUpPortrait,
  run: function() {
    // Check title visibility
    ok(!narrowTitleVisible("start-topsites"), "topsites narrow title is not visible");
    ok(!narrowTitleVisible("start-bookmarks"), "bookmarks narrow title is not visible");
    ok(!narrowTitleVisible("start-history"), "history narrow title is not visible");

    ok(wideTitleVisible("start-topsites"), "topsites wide title is visible");
    ok(wideTitleVisible("start-bookmarks"), "bookmarks wide title is visible");
    ok(wideTitleVisible("start-history"), "history wide title is visible");
  },
  tearDown: restoreViewstate
});

gTests.push({
  desc: "Test portrait scrolls vertically",
  setUp: function() {
    // Populate with mock data
    BookmarksTestHelper.setup();
    HistoryTestHelper.setup();

    yield setUpPortrait();
  },
  run: function() {
    ok(Browser.selectedBrowser.contentWindow.scrollMaxY !== 0, "Portrait scrolls vertically");
    ok(Browser.selectedBrowser.contentWindow.scrollMaxX === 0, "Portrait does not scroll horizontally");
  },
  tearDown: function() {
    BookmarksTestHelper.restore();
    HistoryTestHelper.restore();
    yield restoreViewstate();
  }
});

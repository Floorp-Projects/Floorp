/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Test opening and closing the bookmarks panel.
 */

let bookmarkPanel = document.getElementById("editBookmarkPanel");
let bookmarkStar = document.getElementById("bookmarks-menu-button");
let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");

StarUI._closePanelQuickForTesting = true;

function* test_bookmarks_popup({isNewBookmark, popupShowFn, popupEditFn,
                                shouldAutoClose, popupHideFn, isBookmarkRemoved}) {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  try {
    if (!isNewBookmark) {
      yield PlacesUtils.bookmarks.insert({
        parentGuid: PlacesUtils.bookmarks.unfiledGuid,
        url: "about:home",
        title: "Home Page"
      });
    }

    is(bookmarkStar.hasAttribute("starred"), !isNewBookmark,
       "Page should only be starred prior to popupshown if editing bookmark");
    is(bookmarkPanel.state, "closed", "Panel should be 'closed' to start test");
    let shownPromise = promisePopupShown(bookmarkPanel);
    yield popupShowFn(tab.linkedBrowser);
    yield shownPromise;
    is(bookmarkPanel.state, "open", "Panel should be 'open' after shownPromise is resolved");

    if (popupEditFn) {
      yield popupEditFn();
    }
    let bookmarks = [];
    yield PlacesUtils.bookmarks.fetch({url: "about:home"}, bm => bookmarks.push(bm));
    is(bookmarks.length, 1, "Only one bookmark should exist");
    is(bookmarkStar.getAttribute("starred"), "true", "Page is starred");
    is(bookmarkPanelTitle.value,
      isNewBookmark ?
        gNavigatorBundle.getString("editBookmarkPanel.pageBookmarkedTitle") :
        gNavigatorBundle.getString("editBookmarkPanel.editBookmarkTitle"),
      "title should match isEditingBookmark state");

    if (!shouldAutoClose) {
      yield new Promise(resolve => setTimeout(resolve, 400));
      is(bookmarkPanel.state, "open", "Panel should still be 'open' for non-autoclose");
    }

    let hiddenPromise = promisePopupHidden(bookmarkPanel);
    if (popupHideFn) {
      yield popupHideFn();
    }
    yield hiddenPromise;
    is(bookmarkStar.hasAttribute("starred"), !isBookmarkRemoved,
       "Page is starred after closing");
  } finally {
    let bookmark = yield PlacesUtils.bookmarks.fetch({url: "about:home"});
    is(!!bookmark, !isBookmarkRemoved,
       "bookmark should not be present if a panel action should've removed it");
    if (bookmark) {
      yield PlacesUtils.bookmarks.remove(bookmark);
    }
    gBrowser.removeTab(tab);
  }
}

add_task(function* panel_shown_for_new_bookmarks_and_autocloses() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_once_for_doubleclick_on_new_bookmark_star_and_autocloses() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      EventUtils.synthesizeMouse(bookmarkStar, 10, 10, { clickCount: 2 },
                                 window);
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_once_for_slow_doubleclick_on_new_bookmark_star_and_autocloses() {
  todo(false, "bug 1250267, may need to add some tracking state to " +
              "browser-places.js for this.");
  return;

  yield test_bookmarks_popup({
    isNewBookmark: true,
    *popupShowFn() {
      EventUtils.synthesizeMouse(bookmarkStar, 10, 10, window);
      yield new Promise(resolve => setTimeout(resolve, 300));
      EventUtils.synthesizeMouse(bookmarkStar, 10, 10, window);
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_keyboardshortcut_on_new_bookmark_star_and_autocloses() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      EventUtils.synthesizeKey("D", {accelKey: true}, window);
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_new_bookmarks_mousemove_mouseout() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    *popupEditFn() {
      let mouseMovePromise = BrowserTestUtils.waitForEvent(bookmarkPanel, "mousemove");
      EventUtils.synthesizeMouseAtCenter(bookmarkPanel, {type: "mousemove"});
      info("Waiting for mousemove event");
      yield mouseMovePromise;
      info("Got mousemove event");

      yield new Promise(resolve => setTimeout(resolve, 400));
      is(bookmarkPanel.state, "open", "Panel should still be open on mousemove");
    },
    *popupHideFn() {
      let mouseOutPromise = BrowserTestUtils.waitForEvent(bookmarkPanel, "mouseout");
      EventUtils.synthesizeMouse(bookmarkPanel, 0, 0, {type: "mouseout"});
      EventUtils.synthesizeMouseAtCenter(document.documentElement, {type: "mousemove"});
      info("Waiting for mouseout event");
      yield mouseOutPromise;
      info("Got mouseout event, should autoclose now");
    },
    shouldAutoClose: false,
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_new_bookmark_no_autoclose_close_with_ESC() {
  yield test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("VK_ESCAPE", {accelKey: true}, window);
    },
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_editing_no_autoclose_close_with_ESC() {
  yield test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("VK_ESCAPE", {accelKey: true}, window);
    },
    isBookmarkRemoved: false,
  });
});

add_task(function* panel_shown_for_new_bookmark_keypress_no_autoclose() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    popupEditFn() {
      EventUtils.sendChar("VK_TAB", window);
    },
    shouldAutoClose: false,
    popupHideFn() {
      bookmarkPanel.hidePopup();
    },
    isBookmarkRemoved: false,
  });
});

add_task(function* contextmenu_new_bookmark_keypress_no_autoclose() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    *popupShowFn(browser) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu,
                                                          "popupshown");
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu,
                                                           "popuphidden");
      yield BrowserTestUtils.synthesizeMouseAtCenter("body", {
        type: "contextmenu",
        button: 2
      }, browser);
      yield awaitPopupShown;
      document.getElementById("context-bookmarkpage").click();
      contextMenu.hidePopup();
      yield awaitPopupHidden;
    },
    popupEditFn() {
      EventUtils.sendChar("VK_TAB", window);
    },
    shouldAutoClose: false,
    popupHideFn() {
      bookmarkPanel.hidePopup();
    },
    isBookmarkRemoved: false,
  });
});

add_task(function* bookmarks_menu_new_bookmark_remove_bookmark() {
  yield test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn(browser) {
      document.getElementById("menu_bookmarkThisPage").doCommand();
    },
    shouldAutoClose: true,
    popupHideFn() {
      document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(function* ctrl_d_edit_bookmark_remove_bookmark() {
  yield test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn(browser) {
      EventUtils.synthesizeKey("D", {accelKey: true}, window);
    },
    shouldAutoClose: true,
    popupHideFn() {
      document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

registerCleanupFunction(function() {
  delete StarUI._closePanelQuickForTesting;
})

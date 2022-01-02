/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

/**
 * Test opening and closing the bookmarks panel.
 */

StarUI._createPanelIfNeeded();
let bookmarkPanel = document.getElementById("editBookmarkPanel");
let bookmarkStar = BookmarkingUI.star;
let bookmarkPanelTitle = document.getElementById("editBookmarkPanelTitle");
let bookmarkRemoveButton = document.getElementById(
  "editBookmarkPanelRemoveButton"
);
let editBookmarkPanelRemoveButtonRect;

const TEST_URL = "data:text/html,<html><body></body></html>";

StarUI._closePanelQuickForTesting = true;

add_task(async function setup() {
  bookmarkPanel.setAttribute("animate", false);
  registerCleanupFunction(() => {
    bookmarkPanel.removeAttribute("animate");
  });
});

function mouseout() {
  let mouseOutPromise = BrowserTestUtils.waitForEvent(
    bookmarkPanel,
    "mouseout"
  );
  EventUtils.synthesizeMouse(bookmarkPanel, 0, 0, { type: "mouseout" });
  EventUtils.synthesizeMouseAtCenter(gURLBar.textbox, { type: "mousemove" });
  info("Waiting for mouseout event");
  return mouseOutPromise;
}

async function test_bookmarks_popup({
  isNewBookmark,
  popupShowFn,
  popupEditFn,
  shouldAutoClose,
  popupHideFn,
  isBookmarkRemoved,
}) {
  await BrowserTestUtils.withNewTab({ gBrowser, url: TEST_URL }, async function(
    browser
  ) {
    try {
      if (!isNewBookmark) {
        await PlacesUtils.bookmarks.insert({
          parentGuid: await PlacesUIUtils.defaultParentGuid,
          url: TEST_URL,
          title: "Home Page",
        });
      }

      info(`BookmarkingUI.status is ${BookmarkingUI.status}`);
      await TestUtils.waitForCondition(
        () => BookmarkingUI.status != BookmarkingUI.STATUS_UPDATING,
        "BookmarkingUI should not be updating"
      );

      Assert.equal(
        bookmarkStar.hasAttribute("starred"),
        !isNewBookmark,
        "Page should only be starred prior to popupshown if editing bookmark"
      );
      Assert.equal(
        bookmarkPanel.state,
        "closed",
        "Panel should be 'closed' to start test"
      );
      let shownPromise = promisePopupShown(bookmarkPanel);
      await popupShowFn(browser);
      await shownPromise;
      Assert.equal(
        bookmarkPanel.state,
        "open",
        "Panel should be 'open' after shownPromise is resolved"
      );

      editBookmarkPanelRemoveButtonRect = bookmarkRemoveButton.getBoundingClientRect();

      if (popupEditFn) {
        await popupEditFn();
      }
      let bookmarks = [];
      await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, bm =>
        bookmarks.push(bm)
      );
      Assert.equal(bookmarks.length, 1, "Only one bookmark should exist");
      Assert.equal(
        bookmarkStar.getAttribute("starred"),
        "true",
        "Page is starred"
      );
      Assert.equal(
        bookmarkPanelTitle.dataset.l10nId,
        isNewBookmark ? "bookmarks-add-bookmark" : "bookmarks-edit-bookmark",
        "title should match isEditingBookmark state"
      );
      Assert.equal(
        bookmarkRemoveButton.dataset.l10nId,
        isNewBookmark ? "bookmark-panel-cancel" : "bookmark-panel-remove",
        "remove/cancel button label should match isEditingBookmark state"
      );

      if (!shouldAutoClose) {
        await new Promise(resolve => setTimeout(resolve, 400));
        Assert.equal(
          bookmarkPanel.state,
          "open",
          "Panel should still be 'open' for non-autoclose"
        );
      }

      let defaultLocation = await PlacesUIUtils.defaultParentGuid;
      let bookmarkRemovedPromise = Promise.resolve();
      if (isBookmarkRemoved) {
        bookmarkRemovedPromise = PlacesTestUtils.waitForNotification(
          "bookmark-removed",
          events =>
            events.some(
              event =>
                event.parentGuid == defaultLocation && TEST_URL == event.url
            ),
          "places"
        );
      }

      let hiddenPromise = promisePopupHidden(bookmarkPanel);
      if (popupHideFn) {
        await popupHideFn();
      }
      await Promise.all([hiddenPromise, bookmarkRemovedPromise]);

      Assert.equal(
        bookmarkStar.hasAttribute("starred"),
        !isBookmarkRemoved,
        "Page is starred after closing"
      );
    } finally {
      let bookmark = await PlacesUtils.bookmarks.fetch({ url: TEST_URL });
      Assert.equal(
        !!bookmark,
        !isBookmarkRemoved,
        "bookmark should not be present if a panel action should've removed it"
      );
      if (bookmark) {
        await PlacesUtils.bookmarks.remove(bookmark);
      }
    }
  });
}

add_task(async function panel_shown_for_new_bookmarks_and_autocloses() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
});

add_task(
  async function panel_shown_once_for_doubleclick_on_new_bookmark_star_and_autocloses() {
    await test_bookmarks_popup({
      isNewBookmark: true,
      popupShowFn() {
        EventUtils.synthesizeMouse(
          bookmarkStar,
          10,
          10,
          { clickCount: 2 },
          window
        );
      },
      shouldAutoClose: true,
      isBookmarkRemoved: false,
    });
  }
);

add_task(
  async function panel_shown_once_for_slow_doubleclick_on_new_bookmark_star_and_autocloses() {
    todo(
      false,
      "bug 1250267, may need to add some tracking state to " +
        "browser-places.js for this."
    );

    /*
  await test_bookmarks_popup({
    isNewBookmark: true,
    *popupShowFn() {
      EventUtils.synthesizeMouse(bookmarkStar, 10, 10, window);
      await new Promise(resolve => setTimeout(resolve, 300));
      EventUtils.synthesizeMouse(bookmarkStar, 10, 10, window);
    },
    shouldAutoClose: true,
    isBookmarkRemoved: false,
  });
  */
  }
);

add_task(
  async function panel_shown_for_keyboardshortcut_on_new_bookmark_star_and_autocloses() {
    await test_bookmarks_popup({
      isNewBookmark: true,
      popupShowFn() {
        EventUtils.synthesizeKey("D", { accelKey: true }, window);
      },
      shouldAutoClose: true,
      isBookmarkRemoved: false,
    });
  }
);

add_task(async function panel_shown_for_new_bookmarks_mousemove_mouseout() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    async popupEditFn() {
      let mouseMovePromise = BrowserTestUtils.waitForEvent(
        bookmarkPanel,
        "mousemove"
      );
      EventUtils.synthesizeMouseAtCenter(bookmarkPanel, { type: "mousemove" });
      info("Waiting for mousemove event");
      await mouseMovePromise;
      info("Got mousemove event");

      await new Promise(resolve => setTimeout(resolve, 400));
      is(
        bookmarkPanel.state,
        "open",
        "Panel should still be open on mousemove"
      );
    },
    async popupHideFn() {
      await mouseout();
      info("Got mouseout event, should autoclose now");
    },
    shouldAutoClose: false,
    isBookmarkRemoved: false,
  });
});

add_task(async function panel_shown_for_new_bookmark_close_with_ESC() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: true,
    popupHideFn() {
      EventUtils.synthesizeKey("VK_ESCAPE", { accelKey: true }, window);
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function panel_shown_for_editing_no_autoclose_close_with_ESC() {
  await test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn() {
      bookmarkStar.click();
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("VK_ESCAPE", { accelKey: true }, window);
    },
    isBookmarkRemoved: false,
  });
});

add_task(async function panel_shown_for_new_bookmark_keypress_no_autoclose() {
  await test_bookmarks_popup({
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

add_task(
  async function panel_shown_for_new_bookmark_compositionstart_no_autoclose() {
    await test_bookmarks_popup({
      isNewBookmark: true,
      popupShowFn() {
        bookmarkStar.click();
      },
      async popupEditFn() {
        let compositionStartPromise = BrowserTestUtils.waitForEvent(
          bookmarkPanel,
          "compositionstart"
        );
        EventUtils.synthesizeComposition({ type: "compositionstart" }, window);
        info("Waiting for compositionstart event");
        await compositionStartPromise;
        info("Got compositionstart event");
      },
      shouldAutoClose: false,
      popupHideFn() {
        EventUtils.synthesizeComposition({ type: "compositioncommitasis" });
        bookmarkPanel.hidePopup();
      },
      isBookmarkRemoved: false,
    });
  }
);

add_task(
  async function panel_shown_for_new_bookmark_compositionstart_mouseout_no_autoclose() {
    await test_bookmarks_popup({
      isNewBookmark: true,
      popupShowFn() {
        bookmarkStar.click();
      },
      async popupEditFn() {
        let mouseMovePromise = BrowserTestUtils.waitForEvent(
          bookmarkPanel,
          "mousemove"
        );
        EventUtils.synthesizeMouseAtCenter(bookmarkPanel, {
          type: "mousemove",
        });
        info("Waiting for mousemove event");
        await mouseMovePromise;
        info("Got mousemove event");

        let compositionStartPromise = BrowserTestUtils.waitForEvent(
          bookmarkPanel,
          "compositionstart"
        );
        EventUtils.synthesizeComposition({ type: "compositionstart" }, window);
        info("Waiting for compositionstart event");
        await compositionStartPromise;
        info("Got compositionstart event");

        await mouseout();
        info("Got mouseout event, but shouldn't run autoclose");
      },
      shouldAutoClose: false,
      popupHideFn() {
        EventUtils.synthesizeComposition({ type: "compositioncommitasis" });
        bookmarkPanel.hidePopup();
      },
      isBookmarkRemoved: false,
    });
  }
);

add_task(
  async function panel_shown_for_new_bookmark_compositionend_no_autoclose() {
    await test_bookmarks_popup({
      isNewBookmark: true,
      popupShowFn() {
        bookmarkStar.click();
      },
      async popupEditFn() {
        let mouseMovePromise = BrowserTestUtils.waitForEvent(
          bookmarkPanel,
          "mousemove"
        );
        EventUtils.synthesizeMouseAtCenter(bookmarkPanel, {
          type: "mousemove",
        });
        info("Waiting for mousemove event");
        await mouseMovePromise;
        info("Got mousemove event");

        EventUtils.synthesizeComposition({
          type: "compositioncommit",
          data: "committed text",
        });
      },
      popupHideFn() {
        bookmarkPanel.hidePopup();
      },
      shouldAutoClose: false,
      isBookmarkRemoved: false,
    });
  }
);

add_task(async function contextmenu_new_bookmark_keypress_no_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    async popupShowFn(browser) {
      let contextMenu = document.getElementById("contentAreaContextMenu");
      let awaitPopupShown = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popupshown"
      );
      let awaitPopupHidden = BrowserTestUtils.waitForEvent(
        contextMenu,
        "popuphidden"
      );
      await BrowserTestUtils.synthesizeMouseAtCenter(
        "body",
        {
          type: "contextmenu",
          button: 2,
        },
        browser
      );
      await awaitPopupShown;
      document.getElementById("context-bookmarkpage").click();
      contextMenu.hidePopup();
      await awaitPopupHidden;
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

add_task(async function bookmarks_menu_new_bookmark_remove_bookmark() {
  await test_bookmarks_popup({
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

add_task(async function ctrl_d_edit_bookmark_remove_bookmark() {
  await test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn(browser) {
      EventUtils.synthesizeKey("D", { accelKey: true }, window);
    },
    shouldAutoClose: true,
    popupHideFn() {
      document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function enter_on_remove_bookmark_should_remove_bookmark() {
  if (AppConstants.platform == "macosx") {
    // "Full Keyboard Access" is disabled by default, and thus doesn't allow
    // keyboard navigation to the "Remove Bookmarks" button by default.
    return;
  }

  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn(browser) {
      EventUtils.synthesizeKey("D", { accelKey: true }, window);
    },
    shouldAutoClose: true,
    popupHideFn() {
      while (
        !document.activeElement ||
        document.activeElement.id != "editBookmarkPanelRemoveButton"
      ) {
        EventUtils.sendChar("VK_TAB", window);
      }
      EventUtils.sendChar("VK_RETURN", window);
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function mouse_hovering_panel_should_prevent_autoclose() {
  if (AppConstants.platform != "win") {
    // This test requires synthesizing native mouse movement which is
    // best supported on Windows.
    return;
  }
  await test_bookmarks_popup({
    isNewBookmark: true,
    async popupShowFn() {
      await EventUtils.promiseNativeMouseEvent({
        type: "mousemove",
        target: document.documentElement,
        offsetX: editBookmarkPanelRemoveButtonRect.left,
        offsetY: editBookmarkPanelRemoveButtonRect.top,
      });
      EventUtils.synthesizeKey("D", { accelKey: true }, window);
    },
    shouldAutoClose: false,
    popupHideFn() {
      document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function ctrl_d_new_bookmark_mousedown_mouseout_no_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn(browser) {
      EventUtils.synthesizeKey("D", { accelKey: true }, window);
    },
    async popupEditFn() {
      let mouseMovePromise = BrowserTestUtils.waitForEvent(
        bookmarkPanel,
        "mousemove"
      );
      EventUtils.synthesizeMouseAtCenter(bookmarkPanel, { type: "mousemove" });
      info("Waiting for mousemove event");
      await mouseMovePromise;
      info("Got mousemove event");

      await new Promise(resolve => setTimeout(resolve, 400));
      is(
        bookmarkPanel.state,
        "open",
        "Panel should still be open on mousemove"
      );

      EventUtils.synthesizeMouseAtCenter(bookmarkPanelTitle, {
        button: 1,
        type: "mousedown",
      });

      await mouseout();
    },
    shouldAutoClose: false,
    popupHideFn() {
      document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function enter_during_autocomplete_should_prevent_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    async popupShowFn(browser) {
      EventUtils.synthesizeKey("d", { accelKey: true }, window);
    },
    async popupEditFn() {
      let tagsField = document.getElementById("editBMPanel_tagsField");
      tagsField.value = "";
      tagsField.focus();

      // Register a tag into the DB.
      EventUtils.sendString("Abc", window);
      tagsField.blur();

      // Start autocomplete with the registered tag.
      tagsField.value = "";
      let popup = document.getElementById("editBMPanel_tagsAutocomplete");
      let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      tagsField.focus();
      EventUtils.sendString("a", window);
      await promiseShown;
      ok(promiseShown, "autocomplete shown");

      // Select first candidate.
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);

      // Type Enter key to choose the item.
      EventUtils.synthesizeKey("KEY_Enter", {}, window);

      Assert.equal(
        tagsField.value,
        "Abc",
        "Autocomplete should've inserted the selected item"
      );
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("KEY_Escape", {}, window);
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function escape_during_autocomplete_should_prevent_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    async popupShowFn(browser) {
      EventUtils.synthesizeKey("d", { accelKey: true }, window);
    },
    async popupEditFn() {
      let tagsField = document.getElementById("editBMPanel_tagsField");
      tagsField.value = "";
      tagsField.focus();

      // Register a tag into the DB.
      EventUtils.sendString("Abc", window);
      tagsField.blur();

      // Start autocomplete with the registered tag.
      tagsField.value = "";
      let popup = document.getElementById("editBMPanel_tagsAutocomplete");
      let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      tagsField.focus();
      EventUtils.sendString("a", window);
      await promiseShown;
      ok(promiseShown, "autocomplete shown");

      // Select first candidate.
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, window);

      // Type Escape key to close autocomplete.
      EventUtils.synthesizeKey("KEY_Escape", {}, window);

      // The text reverts to what was typed.
      // Note, it's important that this is different from the previously
      // inserted tag, since it will test an untag/tag undo condition.
      Assert.equal(
        tagsField.value,
        "a",
        "Autocomplete should revert to what was typed"
      );
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("KEY_Escape", {}, window);
    },
    isBookmarkRemoved: true,
  });
});

registerCleanupFunction(function() {
  delete StarUI._closePanelQuickForTesting;
});

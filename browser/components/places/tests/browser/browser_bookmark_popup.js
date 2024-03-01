/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";
requestLongerTimeout(2);

/**
 * Test opening and closing the bookmarks panel.
 */
let win;
let bookmarkPanel;
let bookmarkStar;
let bookmarkPanelTitle;
let bookmarkRemoveButton;
let editBookmarkPanelRemoveButtonRect;

const TEST_URL = "data:text/html,<html><body></body></html>";

add_setup(async function () {
  win = await BrowserTestUtils.openNewBrowserWindow();

  win.StarUI._createPanelIfNeeded();
  win.StarUI._closePanelQuickForTesting = true;
  bookmarkPanel = win.document.getElementById("editBookmarkPanel");
  bookmarkPanel.setAttribute("animate", false);
  bookmarkStar = win.BookmarkingUI.star;
  bookmarkPanelTitle = win.document.getElementById("editBookmarkPanelTitle");
  bookmarkRemoveButton = win.document.getElementById(
    "editBookmarkPanelRemoveButton"
  );

  registerCleanupFunction(async () => {
    delete win.StarUI._closePanelQuickForTesting;
    await BrowserTestUtils.closeWindow(win);
  });
});

function mouseout() {
  let mouseOutPromise = BrowserTestUtils.waitForEvent(
    bookmarkPanel,
    "mouseout"
  );
  EventUtils.synthesizeNativeMouseEvent({
    type: "mousemove",
    target: win.gURLBar.textbox,
    offsetX: 0,
    offsetY: 0,
    win,
  });
  EventUtils.synthesizeMouse(bookmarkPanel, 0, 0, { type: "mouseout" }, win);
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
  await BrowserTestUtils.withNewTab(
    { gBrowser: win.gBrowser, url: TEST_URL },
    async function (browser) {
      try {
        if (!isNewBookmark) {
          await PlacesUtils.bookmarks.insert({
            parentGuid: await PlacesUIUtils.defaultParentGuid,
            url: TEST_URL,
            title: "Home Page",
          });
        }

        info(`BookmarkingUI.status is ${win.BookmarkingUI.status}`);
        await TestUtils.waitForCondition(
          () => win.BookmarkingUI.status != win.BookmarkingUI.STATUS_UPDATING,
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

        editBookmarkPanelRemoveButtonRect =
          bookmarkRemoveButton.getBoundingClientRect();

        if (popupEditFn) {
          await popupEditFn();
        }
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
        const promises = [];
        if (isNewBookmark && !isBookmarkRemoved) {
          // Expect new bookmark to be created.
          promises.push(
            PlacesTestUtils.waitForNotification("bookmark-added", events =>
              events.some(
                ({ parentGuid, url }) =>
                  parentGuid == defaultLocation && TEST_URL == url
              )
            )
          );
        }
        if (!isNewBookmark && isBookmarkRemoved) {
          // Expect existing bookmark to be removed.
          promises.push(
            PlacesTestUtils.waitForNotification("bookmark-removed", events =>
              events.some(
                ({ parentGuid, url }) =>
                  parentGuid == defaultLocation && TEST_URL == url
              )
            )
          );
        }

        promises.push(promisePopupHidden(bookmarkPanel));
        if (popupHideFn) {
          await popupHideFn();
        } else {
          // Move the mouse out of the way so that the panel will auto-close.
          await mouseout();
        }
        await Promise.all(promises);

        Assert.equal(
          bookmarkStar.hasAttribute("starred"),
          !isBookmarkRemoved,
          "Page is starred after closing"
        );

        // Count number of bookmarks.
        let count = 0;
        await PlacesUtils.bookmarks.fetch({ url: TEST_URL }, () => count++);
        const message = isBookmarkRemoved
          ? "No bookmark should exist"
          : "Only one bookmark should exist";
        Assert.equal(count, isBookmarkRemoved ? 0 : 1, message);
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
    }
  );
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
          win
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
        EventUtils.synthesizeKey("D", { accelKey: true }, win);
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
      EventUtils.synthesizeMouseAtCenter(
        bookmarkPanel,
        { type: "mousemove" },
        win
      );
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
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
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
      EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
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
      EventUtils.sendChar("VK_TAB", win);
    },
    shouldAutoClose: false,
    popupHideFn() {
      bookmarkPanel.hidePopup();
    },
    isBookmarkRemoved: false,
  });
});

add_task(async function bookmark_with_invalid_default_folder() {
  await createAndRemoveDefaultFolder();

  await test_bookmarks_popup({
    isNewBookmark: true,
    shouldAutoClose: true,
    async popupShowFn() {
      EventUtils.synthesizeKey("d", { accelKey: true }, win);
    },
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
        EventUtils.synthesizeComposition({ type: "compositionstart" }, win);
        info("Waiting for compositionstart event");
        await compositionStartPromise;
        info("Got compositionstart event");
      },
      shouldAutoClose: false,
      popupHideFn() {
        EventUtils.synthesizeComposition(
          { type: "compositioncommitasis" },
          win
        );
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
        EventUtils.synthesizeMouseAtCenter(
          bookmarkPanel,
          {
            type: "mousemove",
          },
          win
        );
        info("Waiting for mousemove event");
        await mouseMovePromise;
        info("Got mousemove event");

        let compositionStartPromise = BrowserTestUtils.waitForEvent(
          bookmarkPanel,
          "compositionstart"
        );
        EventUtils.synthesizeComposition({ type: "compositionstart" }, win);
        info("Waiting for compositionstart event");
        await compositionStartPromise;
        info("Got compositionstart event");

        await mouseout();
        info("Got mouseout event, but shouldn't run autoclose");
      },
      shouldAutoClose: false,
      popupHideFn() {
        EventUtils.synthesizeComposition(
          { type: "compositioncommitasis" },
          win
        );
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
        EventUtils.synthesizeMouseAtCenter(
          bookmarkPanel,
          {
            type: "mousemove",
          },
          win
        );
        info("Waiting for mousemove event");
        await mouseMovePromise;
        info("Got mousemove event");

        EventUtils.synthesizeComposition(
          {
            type: "compositioncommit",
            data: "committed text",
          },
          win
        );
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
      let contextMenu = win.document.getElementById("contentAreaContextMenu");
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
      contextMenu.activateItem(
        win.document.getElementById("context-bookmarkpage")
      );
      await awaitPopupHidden;
    },
    popupEditFn() {
      EventUtils.sendChar("VK_TAB", win);
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
    popupShowFn() {
      win.document.getElementById("menu_bookmarkThisPage").doCommand();
    },
    shouldAutoClose: true,
    popupHideFn() {
      win.document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function ctrl_d_edit_bookmark_remove_bookmark() {
  await test_bookmarks_popup({
    isNewBookmark: false,
    popupShowFn() {
      EventUtils.synthesizeKey("D", { accelKey: true }, win);
    },
    shouldAutoClose: true,
    popupHideFn() {
      win.document.getElementById("editBookmarkPanelRemoveButton").click();
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
    popupShowFn() {
      EventUtils.synthesizeKey("D", { accelKey: true }, win);
    },
    shouldAutoClose: true,
    popupHideFn() {
      while (
        !win.document.activeElement ||
        win.document.activeElement.id != "editBookmarkPanelRemoveButton"
      ) {
        EventUtils.sendChar("VK_TAB", win);
      }
      EventUtils.sendChar("VK_RETURN", win);
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
        target: win.document.documentElement,
        offsetX: editBookmarkPanelRemoveButtonRect.left,
        offsetY: editBookmarkPanelRemoveButtonRect.top,
      });
      EventUtils.synthesizeKey("D", { accelKey: true }, win);
    },
    shouldAutoClose: false,
    popupHideFn() {
      win.document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function ctrl_d_new_bookmark_mousedown_mouseout_no_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: true,
    popupShowFn() {
      EventUtils.synthesizeKey("D", { accelKey: true }, win);
    },
    async popupEditFn() {
      let mouseMovePromise = BrowserTestUtils.waitForEvent(
        bookmarkPanel,
        "mousemove"
      );
      EventUtils.synthesizeMouseAtCenter(
        bookmarkPanel,
        { type: "mousemove" },
        win
      );
      info("Waiting for mousemove event");
      await mouseMovePromise;
      info("Got mousemove event");

      await new Promise(resolve => setTimeout(resolve, 400));
      is(
        bookmarkPanel.state,
        "open",
        "Panel should still be open on mousemove"
      );

      EventUtils.synthesizeMouseAtCenter(
        bookmarkPanelTitle,
        {
          button: 1,
          type: "mousedown",
        },
        win
      );

      await mouseout();
    },
    shouldAutoClose: false,
    popupHideFn() {
      win.document.getElementById("editBookmarkPanelRemoveButton").click();
    },
    isBookmarkRemoved: true,
  });
});

add_task(async function enter_during_autocomplete_should_prevent_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: false,
    async popupShowFn() {
      PlacesUtils.tagging.tagURI(makeURI(TEST_URL), ["Abc"]);
      EventUtils.synthesizeKey("d", { accelKey: true }, win);
    },
    async popupEditFn() {
      // Start autocomplete with the registered tag.
      let tagsField = win.document.getElementById("editBMPanel_tagsField");
      tagsField.value = "";
      let popup = win.document.getElementById("editBMPanel_tagsAutocomplete");
      let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      tagsField.focus();
      EventUtils.sendString("a", win);
      await promiseShown;
      ok(promiseShown, "autocomplete shown");

      // Select first candidate.
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);

      // Type Enter key to choose the item.
      EventUtils.synthesizeKey("KEY_Enter", {}, win);

      Assert.equal(
        tagsField.value,
        "Abc",
        "Autocomplete should've inserted the selected item"
      );
    },
    shouldAutoClose: false,
    popupHideFn() {
      EventUtils.synthesizeKey("KEY_Escape", {}, win);
    },
    isBookmarkRemoved: false,
  });
});

add_task(async function escape_during_autocomplete_should_prevent_autoclose() {
  await test_bookmarks_popup({
    isNewBookmark: false,
    async popupShowFn() {
      PlacesUtils.tagging.tagURI(makeURI(TEST_URL), ["Abc"]);
      EventUtils.synthesizeKey("d", { accelKey: true }, win);
    },
    async popupEditFn() {
      // Start autocomplete with the registered tag.
      let tagsField = win.document.getElementById("editBMPanel_tagsField");
      tagsField.value = "";
      let popup = win.document.getElementById("editBMPanel_tagsAutocomplete");
      let promiseShown = BrowserTestUtils.waitForEvent(popup, "popupshown");
      tagsField.focus();
      EventUtils.sendString("a", win);
      await promiseShown;
      ok(promiseShown, "autocomplete shown");

      // Select first candidate.
      EventUtils.synthesizeKey("KEY_ArrowDown", {}, win);

      // Type Escape key to close autocomplete.
      EventUtils.synthesizeKey("KEY_Escape", {}, win);

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
      EventUtils.synthesizeKey("KEY_Escape", {}, win);
    },
    isBookmarkRemoved: false,
  });
});

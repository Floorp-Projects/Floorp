"use strict";

/* global sinon */
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js");

const sandbox = sinon.sandbox.create();

registerCleanupFunction(async function() {
  sandbox.restore();
  delete window.sinon;
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesTestUtils.clearHistory();
});

let bookmarks; // Bookmarks added via insertTree.

add_task(async function setup() {
  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [{
      title: "bm1",
      url: "http://example.com",
    }, {
      title: "bm2",
      url: "http://example.com/2",
    }]
  });

  // Undo is called asynchronously - and not waited for. Since we're not
  // expecting undo to be called, we can only tell this by stubbing it.
  sandbox.stub(PlacesTransactions, "undo").returns(Promise.resolve());
});

// Tests for bug 1391393 - Ensures that if the user cancels the bookmark properties
// dialog without having done any changes, then no undo is called.
add_task(async function test_cancel_with_no_changes() {
  if (!PlacesUIUtils.useAsyncTransactions) {
    Assert.ok(true, "Skipping test as async transactions are turned off");
    return;
  }

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([bookmarks[0].guid]);

    // Delete the bookmark to put something in the undo history.
    // Rather than calling cmd_delete, we call the remove directly, so that we
    // can await on it finishing, and be guaranteed that there's something
    // in the history.
    await tree.controller.remove("Remove Selection");

    tree.selectItems([bookmarks[1].guid]);

    // Now open the bookmarks dialog and cancel it.
    await withBookmarksDialog(
      true,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        let acceptButton = dialogWin.document.documentElement.getButton("accept");
        await BrowserTestUtils.waitForCondition(() => !acceptButton.disabled,
          "The accept button should be enabled");
      }
    );

    // Check the bookmark is still removed.
    Assert.ok(!(await PlacesUtils.bookmarks.fetch(bookmarks[0].guid)),
      "The originally removed bookmark should not exist.");

    Assert.ok(await PlacesUtils.bookmarks.fetch(bookmarks[1].guid),
      "The second bookmark should still exist");

    Assert.ok(PlacesTransactions.undo.notCalled, "undo should not have been called");
  });
});

add_task(async function test_cancel_with_changes() {
  if (!PlacesUIUtils.useAsyncTransactions) {
    Assert.ok(true, "Skipping test as async transactions are turned off");
    return;
  }

  await withSidebarTree("bookmarks", async (tree) => {
    tree.selectItems([bookmarks[1].guid]);

    // Now open the bookmarks dialog and cancel it.
    await withBookmarksDialog(
      true,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        let acceptButton = dialogWin.document.documentElement.getButton("accept");
        await BrowserTestUtils.waitForCondition(() => !acceptButton.disabled,
          "The accept button should be enabled");

        let promiseTitleChangeNotification = PlacesTestUtils.waitForNotification(
          "onItemChanged", (itemId, prop, isAnno, val) => prop == "title" && val == "n");

        fillBookmarkTextField("editBMPanel_namePicker", "n", dialogWin);

        // The dialog is instant apply.
        await promiseTitleChangeNotification;

        // Ensure that the addition really is finished before we hit cancel.
        await PlacesTestUtils.promiseAsyncUpdates();
      }
    );

    await BrowserTestUtils.waitForCondition(() => PlacesTransactions.undo.calledOnce,
      "undo should have been called once.");
  });
});

"use strict";

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const sandbox = sinon.createSandbox();

registerCleanupFunction(async function () {
  sandbox.restore();
  await PlacesUtils.bookmarks.eraseEverything();
  await PlacesUtils.history.clear();
});

let bookmarks; // Bookmarks added via insertTree.

add_setup(async function () {
  bookmarks = await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.unfiledGuid,
    children: [
      {
        title: "bm1",
        url: "http://example.com",
      },
      {
        title: "bm2",
        url: "http://example.com/2",
      },
    ],
  });

  // Undo is called asynchronously - and not waited for. Since we're not
  // expecting undo to be called, we can only tell this by stubbing it.
  sandbox.stub(PlacesTransactions, "undo").returns(Promise.resolve());
});

// Tests for bug 1391393 - Ensures that if the user cancels the bookmark properties
// dialog without having done any changes, then no undo is called.
add_task(async function test_cancel_with_no_changes() {
  await withSidebarTree("bookmarks", async tree => {
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
        let acceptButton = dialogWin.document
          .getElementById("bookmarkpropertiesdialog")
          .getButton("accept");
        await TestUtils.waitForCondition(
          () => !acceptButton.disabled,
          "The accept button should be enabled"
        );
      }
    );

    // Check the bookmark is still removed.
    Assert.ok(
      !(await PlacesUtils.bookmarks.fetch(bookmarks[0].guid)),
      "The originally removed bookmark should not exist."
    );

    Assert.ok(
      await PlacesUtils.bookmarks.fetch(bookmarks[1].guid),
      "The second bookmark should still exist"
    );

    Assert.ok(
      PlacesTransactions.undo.notCalled,
      "undo should not have been called"
    );
  });
});

add_task(async function test_cancel_with_changes() {
  await withSidebarTree("bookmarks", async tree => {
    tree.selectItems([bookmarks[1].guid]);

    // Now open the bookmarks dialog and cancel it.
    await withBookmarksDialog(
      true,
      function openDialog() {
        tree.controller.doCommand("placesCmd_show:info");
      },
      async function test(dialogWin) {
        let acceptButton = dialogWin.document
          .getElementById("bookmarkpropertiesdialog")
          .getButton("accept");
        await TestUtils.waitForCondition(
          () => !acceptButton.disabled,
          "EditBookmark: The accept button should be enabled"
        );

        let namePicker = dialogWin.document.getElementById(
          "editBMPanel_namePicker"
        );
        fillBookmarkTextField("editBMPanel_namePicker", "new_n", dialogWin);

        // Ensure that value in field has changed
        Assert.equal(
          namePicker.value,
          "new_n",
          "EditBookmark: The title is the expected one."
        );
      }
    );

    let oldBookmark = await PlacesUtils.bookmarks.fetch(bookmarks[1].guid);
    Assert.equal(
      oldBookmark.title,
      "bm2",
      "EditBookmark: The title hasn't been changed"
    );
  });
});

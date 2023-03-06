/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

add_task(async function bookmark_leak_window() {
  // A library window has two trees after selecting a bookmark item:
  // A left tree (#placesList) and a right tree (#placeContent).
  // Upon closing the window, both trees are destructed, in an unspecified
  // order. In bug 1520047, a memory leak was observed when the left tree
  // was destroyed last.

  let library = await promiseLibrary("BookmarksToolbar");
  let tree = library.document.getElementById("placesList");
  tree.selectItems(["toolbar_____"]);

  await synthesizeClickOnSelectedTreeCell(tree);
  await promiseLibraryClosed(library);

  Assert.ok(
    true,
    "Closing a window after selecting a node in the tree should not cause a leak"
  );
});

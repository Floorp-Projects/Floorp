/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Bug 433231 - Places Library leaks the nsGlobalWindow when closed with a
 *               history entry selected.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=433231
 *
 * STRs: Open Library, select an history entry in History, close Library.
 * ISSUE: We were adding a bookmarks observer when editing a bookmark, when
 *        selecting an history entry the panel was not un-initialized, and
 *        since an history entry does not have an itemId, the observer was
 *        never removed.
 */

const TEST_URI = "http://www.mozilla.org/";

add_task(async function test_no_leak_closing_library_with_history_selected() {
  // Add an history entry.
  await PlacesTestUtils.addVisits(TEST_URI);

  let organizer = await promiseLibrary();

  let contentTree = organizer.document.getElementById("placeContent");
  Assert.notEqual(
    contentTree,
    null,
    "Sanity check: placeContent tree should exist"
  );
  Assert.notEqual(
    organizer.PlacesOrganizer,
    null,
    "Sanity check: PlacesOrganizer should exist"
  );
  Assert.notEqual(
    organizer.gEditItemOverlay,
    null,
    "Sanity check: gEditItemOverlay should exist"
  );

  Assert.ok(
    organizer.gEditItemOverlay.initialized,
    "gEditItemOverlay is initialized"
  );
  Assert.notEqual(
    organizer.gEditItemOverlay._paneInfo.itemGuid,
    "",
    "Editing a bookmark"
  );

  // Select History in the left pane.
  organizer.PlacesOrganizer.selectLeftPaneBuiltIn("History");
  // Select the first history entry.
  let selection = contentTree.view.selection;
  selection.clearSelection();
  selection.rangedSelect(0, 0, true);
  // Check the panel is editing the history entry.
  Assert.equal(
    organizer.gEditItemOverlay._paneInfo.itemGuid,
    "",
    "Editing an history entry"
  );
  // Close Library window.
  organizer.close();

  // Clean up history.
  await PlacesUtils.history.clear();
});

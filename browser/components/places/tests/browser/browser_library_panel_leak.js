/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *        since an histroy entry does not have an itemId, the observer was
 *        never removed.
 */

const TEST_URI = "http://www.mozilla.org/";

function test() {
  function onLibraryReady(organizer) {
    let contentTree = organizer.document.getElementById("placeContent");
    isnot(contentTree, null, "Sanity check: placeContent tree should exist");
    isnot(organizer.PlacesOrganizer, null, "Sanity check: PlacesOrganizer should exist");
    isnot(organizer.gEditItemOverlay, null, "Sanity check: gEditItemOverlay should exist");

    ok(organizer.gEditItemOverlay._initialized, "gEditItemOverlay is initialized");
    isnot(organizer.gEditItemOverlay.itemId, -1, "Editing a bookmark");

    // Select History in the left pane.
    organizer.PlacesOrganizer.selectLeftPaneQuery('History');
    // Select the first history entry.
    let selection = contentTree.view.selection;
    selection.clearSelection();
    selection.rangedSelect(0, 0, true);
    // Check the panel is editing the history entry.
    is(organizer.gEditItemOverlay.itemId, -1, "Editing an history entry");
    // Close Library window.
    organizer.close();
    // Clean up history.
    waitForClearHistory(finish);
  }

  waitForExplicitFinish();
  // Add an history entry.
  ok(PlacesUtils, "checking PlacesUtils, running in chrome context?");
  addVisits(
    {uri: PlacesUtils._uri(TEST_URI), visitDate: Date.now() * 1000,
      transition: PlacesUtils.history.TRANSITION_TYPED},
    window,
    function() {
      openLibrary(onLibraryReady);
    });
}

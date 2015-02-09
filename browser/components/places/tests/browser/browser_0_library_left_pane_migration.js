/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 *  Test we correctly migrate Library left pane to the latest version.
 *  Note: this test MUST be the first between browser chrome tests, or results
 *        of next tests could be unexpected due to PlacesUIUtils getters.
 */

const TEST_URI = "http://www.mozilla.org/";

add_task(function* () {
  // Sanity checks.
  ok(PlacesUtils, "PlacesUtils is running in chrome context");
  ok(PlacesUIUtils, "PlacesUIUtils is running in chrome context");
  ok(PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION > 0,
     "Left pane version in chrome context, current version is: " + PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION );

  // Check if we have any left pane folder already set, remove it eventually.
  let leftPaneItems = PlacesUtils.annotations
                                 .getItemsWithAnnotation(PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  if (leftPaneItems.length > 0) {
    // The left pane has already been created, touching it now would cause
    // next tests to rely on wrong values (and possibly crash)
    is(leftPaneItems.length, 1, "We correctly have only 1 left pane folder");
    // Check version.
    let version = PlacesUtils.annotations.getItemAnnotation(leftPaneItems[0],
                                                            PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
    is(version, PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION, "Left pane version is actual");
    ok(true, "left pane has already been created, skipping test");
    return;
  }

  // Create a fake left pane folder with an old version (current version - 1).
  let folder = yield PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.rootGuid,
    index: PlacesUtils.bookmarks.DEFAULT_INDEX,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
    title: ""
  });

  let folderId = yield PlacesUtils.promiseItemId(folder.guid);
  PlacesUtils.annotations.setItemAnnotation(folderId,
                                            PlacesUIUtils.ORGANIZER_FOLDER_ANNO,
                                            PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION - 1,
                                            0,
                                            PlacesUtils.annotations.EXPIRE_NEVER);

  // Check fake left pane root has been correctly created.
  leftPaneItems =
    PlacesUtils.annotations.getItemsWithAnnotation(PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  is(leftPaneItems.length, 1, "We correctly have only 1 left pane folder");
  is(leftPaneItems[0], folderId, "left pane root itemId is correct");

  // Check version.
  let version = PlacesUtils.annotations.getItemAnnotation(folderId,
                                                          PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  is(version, PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION - 1, "Left pane version correctly set");

  // Open Library, this will upgrade our left pane version.
  let organizer = yield promiseLibrary();

  // Check left pane.
  ok(PlacesUIUtils.leftPaneFolderId > 0, "Left pane folder correctly created");
  leftPaneItems =
    PlacesUtils.annotations.getItemsWithAnnotation(PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  is(leftPaneItems.length, 1, "We correctly have only 1 left pane folder");
  let leftPaneRoot = leftPaneItems[0];
  is(leftPaneRoot, PlacesUIUtils.leftPaneFolderId,
     "leftPaneFolderId getter has correct value");

  // Check version has been upgraded.
  version = PlacesUtils.annotations.getItemAnnotation(leftPaneRoot,
                                                      PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  is(version, PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION,
     "Left pane version has been correctly upgraded");

  // Check left pane is populated.
  organizer.PlacesOrganizer.selectLeftPaneQuery("History");
  is(organizer.PlacesOrganizer._places.selectedNode.itemId,
     PlacesUIUtils.leftPaneQueries["History"],
     "Library left pane is populated and working");

  yield promiseLibraryClosed(organizer);
});

/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 *  Test we correctly fix broken Library left pane queries names.
 */

// Array of left pane queries objects, each one has the following properties:
// name: query's identifier got from annotations,
// itemId: query's itemId,
// correctTitle: original and correct query's title.
var leftPaneQueries = [];

function onLibraryReady(organizer) {
      // Check titles have been fixed.
      for (var i = 0; i < leftPaneQueries.length; i++) {
        var query = leftPaneQueries[i];
        is(PlacesUtils.bookmarks.getItemTitle(query.itemId),
           query.correctTitle, "Title is correct for query " + query.name);
        if ("concreteId" in query) {
          is(PlacesUtils.bookmarks.getItemTitle(query.concreteId),
           query.concreteTitle, "Concrete title is correct for query " + query.name);
        }
      }

      // Close Library window.
      organizer.close();
      // No need to cleanup anything, we have a correct left pane now.
      finish();
}

function test() {
  waitForExplicitFinish();
  // Ensure left pane is initialized.
  ok(PlacesUIUtils.leftPaneFolderId > 0, "left pane folder is initialized");

  // Get the left pane folder.
  var leftPaneItems = PlacesUtils.annotations
                                 .getItemsWithAnnotation(PlacesUIUtils.ORGANIZER_FOLDER_ANNO);

  is(leftPaneItems.length, 1, "We correctly have only 1 left pane folder");
  // Check version.
  var version = PlacesUtils.annotations
                           .getItemAnnotation(leftPaneItems[0],
                                              PlacesUIUtils.ORGANIZER_FOLDER_ANNO);
  is(version, PlacesUIUtils.ORGANIZER_LEFTPANE_VERSION, "Left pane version is actual");

  // Get all left pane queries.
  var items = PlacesUtils.annotations
                         .getItemsWithAnnotation(PlacesUIUtils.ORGANIZER_QUERY_ANNO);
  // Get current queries names.
  for (var i = 0; i < items.length; i++) {
    var itemId = items[i];
    var queryName = PlacesUtils.annotations
                               .getItemAnnotation(items[i],
                                                  PlacesUIUtils.ORGANIZER_QUERY_ANNO);
    var query = { name: queryName,
                  itemId,
                  correctTitle: PlacesUtils.bookmarks.getItemTitle(itemId) };
    switch (queryName) {
      case "BookmarksToolbar":
        query.concreteId = PlacesUtils.toolbarFolderId;
        query.concreteTitle = PlacesUtils.bookmarks.getItemTitle(query.concreteId);
        break;
      case "BookmarksMenu":
        query.concreteId = PlacesUtils.bookmarksMenuFolderId;
        query.concreteTitle = PlacesUtils.bookmarks.getItemTitle(query.concreteId);
        break;
      case "UnfiledBookmarks":
        query.concreteId = PlacesUtils.unfiledBookmarksFolderId;
        query.concreteTitle = PlacesUtils.bookmarks.getItemTitle(query.concreteId);
        break;
    }
    leftPaneQueries.push(query);
    // Rename to a bad title.
    PlacesUtils.bookmarks.setItemTitle(query.itemId, "badName");
    if ("concreteId" in query)
      PlacesUtils.bookmarks.setItemTitle(query.concreteId, "badName");
  }

  restoreLeftPaneGetters();

  // Open Library, this will kick-off left pane code.
  openLibrary(onLibraryReady);
}

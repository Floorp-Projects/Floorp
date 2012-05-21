/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests bug 564900: Add folder specifically for downloads to Library left pane.
 * https://bugzilla.mozilla.org/show_bug.cgi?id=564900
 * This test visits various pages then opens the Library and ensures
 * that both the Downloads folder shows up and that the correct visits
 * are shown in it.
 */

let now = Date.now();

function test() {
  waitForExplicitFinish();

  function onLibraryReady(win) {
    // Add visits to compare contents with.
    fastAddVisit("http://mozilla.com",
                  PlacesUtils.history.TRANSITION_TYPED);
    fastAddVisit("http://google.com",
                  PlacesUtils.history.TRANSITION_DOWNLOAD);
    fastAddVisit("http://en.wikipedia.org",
                  PlacesUtils.history.TRANSITION_TYPED);
    fastAddVisit("http://ubuntu.org",
                  PlacesUtils.history.TRANSITION_DOWNLOAD);

    // Make sure Downloads is present.
    isnot(win.PlacesOrganizer._places.selectedNode, null,
          "Downloads is present and selected");

    // Make sure content in right pane exists.
    let tree = win.document.getElementById("placeContent");
    isnot(tree, null, "placeContent tree exists");

    // Check results.
    var contentRoot = tree.result.root;
    var len = contentRoot.childCount;
    var testUris = ["http://ubuntu.org/", "http://google.com/"];
    for (var i = 0; i < len; i++) {
      is(contentRoot.getChild(i).uri, testUris[i],
          "Comparing downloads shown at index " + i);
    }

    win.close();
    waitForClearHistory(finish);
  }

  openLibrary(onLibraryReady, "Downloads");
}

function fastAddVisit(uri, transition) {
  PlacesUtils.history.addVisit(PlacesUtils._uri(uri), now++ * 1000,
                               null, transition, false, 0);
}

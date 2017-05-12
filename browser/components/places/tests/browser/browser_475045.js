/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
// Instead of loading EventUtils.js into the test scope in browser-test.js for all tests,
// we only need EventUtils.js for a few files which is why we are using loadSubScript.
var EventUtils = {};
this._scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
this._scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

add_task(async function test() {
  // Make sure the bookmarks bar is visible and restore its state on cleanup.
  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function() {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }

  // Setup the node we will use to be dropped. The actual node used does not
  // matter because we will set its data, effect, and mimeType manually.
  let placesItems = document.getElementById("PlacesToolbarItems");
  ok(placesItems, "PlacesToolbarItems should not be null");
  ok(placesItems.localName == "scrollbox", "PlacesToolbarItems should not be null");
  ok(placesItems.childNodes[0], "PlacesToolbarItems must have at least one child");

  /**
   * Simulates a drop of a URI onto the bookmarks bar.
   *
   * @param aEffect
   *        The effect to use for the drop operation: move, copy, or link.
   * @param aMimeType
   *        The mime type to use for the drop operation.
   */
  let simulateDragDrop = function(aEffect, aMimeType) {
    const uriSpec = "http://www.mozilla.org/D1995729-A152-4e30-8329-469B01F30AA7";
    let uri = makeURI(uriSpec);
    EventUtils.synthesizeDrop(placesItems.childNodes[0],
                              placesItems,
                              [[{type: aMimeType,
                                data: uriSpec}]],
                              aEffect, window);

    // Verify that the drop produces exactly one bookmark.
    let bookmarkIds = PlacesUtils.bookmarks
                      .getBookmarkIdsForURI(uri);
    ok(bookmarkIds.length == 1, "There should be exactly one bookmark");

    PlacesUtils.bookmarks.removeItem(bookmarkIds[0]);

    // Verify that we removed the bookmark successfully.
    ok(!PlacesUtils.bookmarks.isBookmarked(uri), "URI should be removed");
  };

  /**
   * Simulates a drop of multiple URIs onto the bookmarks bar.
   *
   * @param aEffect
   *        The effect to use for the drop operation: move, copy, or link.
   * @param aMimeType
   *        The mime type to use for the drop operation.
   */
  let simulateDragDropMultiple = function(aEffect, aMimeType) {
    const uriSpecs = [
      "http://www.mozilla.org/C54263C6-A484-46CF-8E2B-FE131586348A",
      "http://www.mozilla.org/71381257-61E6-4376-AF7C-BF3C5FD8870D",
      "http://www.mozilla.org/091A88BD-5743-4C16-A005-3D2EA3A3B71E"
    ];
    let uris = uriSpecs.map(spec => makeURI(spec));
    let data;
    if (aMimeType == "text/x-moz-url")
      data = uriSpecs.map(spec => spec + "\n" + spec).join("\n");
    else
      data = uriSpecs.join("\n");
    EventUtils.synthesizeDrop(placesItems.childNodes[0],
                              placesItems,
                              [[{type: aMimeType,
                                 data}]],
                              aEffect, window);

    // Verify that the drop produces exactly one bookmark per each URL.
    for (let uri of uris) {
      let bookmarkIds = PlacesUtils.bookmarks
        .getBookmarkIdsForURI(uri);
      ok(bookmarkIds.length == 1, "There should be exactly one bookmark");

      PlacesUtils.bookmarks.removeItem(bookmarkIds[0]);

      // Verify that we removed the bookmark successfully.
      ok(!PlacesUtils.bookmarks.isBookmarked(uri), "URI should be removed");
    }
  };

  // Simulate a bookmark drop for all of the mime types and effects.
  let mimeTypes = ["text/plain", "text/unicode", "text/x-moz-url"];
  let effects = ["move", "copy", "link"];
  effects.forEach(function(effect) {
    mimeTypes.forEach(function(mimeType) {
      simulateDragDrop(effect, mimeType);
      simulateDragDropMultiple(effect, mimeType);
    });
  });
});

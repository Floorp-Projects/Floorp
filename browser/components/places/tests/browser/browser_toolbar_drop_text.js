/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  // Make sure the bookmarks bar is visible and restore its state on cleanup.
  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }

  // Setup the node we will use to be dropped. The actual node used does not
  // matter because we will set its data, effect, and mimeType manually.
  let placesItems = document.getElementById("PlacesToolbarItems");
  ok(placesItems, "PlacesToolbarItems should not be null");
  Assert.equal(
    placesItems.localName,
    "scrollbox",
    "PlacesToolbarItems should not be null"
  );

  /**
   * Simulates a drop of a URI onto the bookmarks bar.
   *
   * @param {string} aEffect
   *        The effect to use for the drop operation: move, copy, or link.
   * @param {string} aMimeType
   *        The mime type to use for the drop operation.
   */
  let simulateDragDrop = async function (aEffect, aMimeType) {
    const url = "http://www.mozilla.org/D1995729-A152-4e30-8329-469B01F30AA7";
    let promiseItemAddedNotification = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      events => events.some(({ url: eventUrl }) => eventUrl == url)
    );

    // We use the toolbar as the drag source, as we just need almost any node
    // to simulate the drag. The actual data for the drop is passed via the
    // drag data. Note: The toolbar is used rather than another bookmark node,
    // as we need something that is immovable from a places perspective, as this
    // forces the move into a copy.
    EventUtils.synthesizeDrop(
      toolbar,
      placesItems,
      [[{ type: aMimeType, data: url }]],
      aEffect,
      window
    );

    await promiseItemAddedNotification;

    // Verify that the drop produces exactly one bookmark.
    let bookmark = await PlacesUtils.bookmarks.fetch({ url });
    Assert.ok(bookmark, "There should be exactly one bookmark");

    await PlacesUtils.bookmarks.remove(bookmark.guid);

    // Verify that we removed the bookmark successfully.
    Assert.equal(
      await PlacesUtils.bookmarks.fetch({ url }),
      null,
      "URI should be removed"
    );
  };

  /**
   * Simulates a drop of multiple URIs onto the bookmarks bar.
   *
   * @param {string} aEffect
   *        The effect to use for the drop operation: move, copy, or link.
   * @param {string} aMimeType
   *        The mime type to use for the drop operation.
   */
  let simulateDragDropMultiple = async function (aEffect, aMimeType) {
    const urls = [
      "http://www.mozilla.org/C54263C6-A484-46CF-8E2B-FE131586348A",
      "http://www.mozilla.org/71381257-61E6-4376-AF7C-BF3C5FD8870D",
      "http://www.mozilla.org/091A88BD-5743-4C16-A005-3D2EA3A3B71E",
    ];
    let data;
    if (aMimeType == "text/x-moz-url") {
      data = urls.map(spec => spec + "\n" + spec).join("\n");
    } else {
      data = urls.join("\n");
    }

    let promiseItemAddedNotification = PlacesTestUtils.waitForNotification(
      "bookmark-added",
      events => events.some(({ url }) => url == urls[2])
    );

    // See notes for EventUtils.synthesizeDrop in simulateDragDrop().
    EventUtils.synthesizeDrop(
      toolbar,
      placesItems,
      [[{ type: aMimeType, data }]],
      aEffect,
      window
    );

    await promiseItemAddedNotification;

    // Verify that the drop produces exactly one bookmark per each URL.
    for (let url of urls) {
      let bookmark = await PlacesUtils.bookmarks.fetch({ url });
      Assert.equal(
        typeof bookmark,
        "object",
        "There should be exactly one bookmark"
      );

      await PlacesUtils.bookmarks.remove(bookmark.guid);

      // Verify that we removed the bookmark successfully.
      Assert.equal(
        await PlacesUtils.bookmarks.fetch({ url }),
        null,
        "URI should be removed"
      );
    }
  };

  // Simulate a bookmark drop for all of the mime types and effects.
  let mimeTypes = ["text/plain", "text/x-moz-url"];
  let effects = ["move", "copy", "link"];
  for (let effect of effects) {
    for (let mimeType of mimeTypes) {
      await simulateDragDrop(effect, mimeType);
      await simulateDragDropMultiple(effect, mimeType);
    }
  }
});

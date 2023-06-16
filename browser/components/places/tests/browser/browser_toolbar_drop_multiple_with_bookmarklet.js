/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function test() {
  // Make sure the bookmarks bar is visible and restore its state on cleanup.
  let toolbar = document.getElementById("PersonalToolbar");
  ok(toolbar, "PersonalToolbar should not be null");

  await PlacesUtils.bookmarks.eraseEverything();

  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }
  // Setup the node we will use to be dropped. The actual node used does not
  // matter because we will set its data, effect, and mimeType manually.
  let placesItems = document.getElementById("PlacesToolbarItems");
  Assert.ok(placesItems, "PlacesToolbarItems should not be null");
  let simulateDragDrop = async function (aEffect, aMimeType) {
    let urls = [
      "https://example.com/1/",
      `javascript: (() => {alert('Hello, World!');})();`,
      "https://example.com/2/",
    ];

    let data = urls.map(spec => spec + "\n" + spec).join("\n");

    EventUtils.synthesizeDrop(
      toolbar,
      placesItems,
      [[{ type: aMimeType, data }]],
      aEffect,
      window
    );
    await PlacesTestUtils.promiseAsyncUpdates();
    for (let url of urls) {
      let bookmark = await PlacesUtils.bookmarks.fetch({ url });
      Assert.ok(!bookmark, "There should be no bookmark");
    }
  };

  // Simulate a bookmark drop for all of the mime types and effects.
  let mimeType = ["text/x-moz-url"];
  let effects = ["copy", "link"];
  for (let effect of effects) {
    await simulateDragDrop(effect, mimeType);
  }
});

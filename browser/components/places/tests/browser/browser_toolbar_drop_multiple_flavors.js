/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check flavors priority when dropping a url/title tuple on the toolbar.

function getDataForType(url, title, type) {
  switch (type) {
    case "text/x-moz-url":
      return `${url}\n${title}`;
    case "text/plain":
      return url;
    case "text/html":
      return `<a href="${url}">${title}</a>`;
  }
  throw new Error("Unknown mime type");
}

async function testDragDrop(effect, mimeTypes) {
  const url = "https://www.mozilla.org/drag_drop_test/";
  const title = "Drag & Drop Test";
  let promiseItemAddedNotification = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    events => events.some(e => e.url == url)
  );

  // Ensure there's no bookmark initially
  let bookmark = await PlacesUtils.bookmarks.fetch({ url });
  Assert.ok(!bookmark, "There should not be a bookmark to the given URL");

  // We use the toolbar as the drag source, as we just need almost any node
  // to simulate the drag.
  let toolbar = document.getElementById("PersonalToolbar");
  EventUtils.synthesizeDrop(
    toolbar,
    document.getElementById("PlacesToolbarItems"),
    [mimeTypes.map(type => ({ type, data: getDataForType(url, title, type) }))],
    effect,
    window
  );

  await promiseItemAddedNotification;

  // Verify that the drop produces exactly one bookmark.
  bookmark = await PlacesUtils.bookmarks.fetch({ url });
  Assert.ok(bookmark, "There should be exactly one bookmark");
  Assert.equal(bookmark.url, url, "Check bookmark URL is correct");
  Assert.equal(bookmark.title, title, "Check bookmark title was preserved");
  await PlacesUtils.bookmarks.remove(bookmark);
}

add_task(async function test() {
  registerCleanupFunction(() => PlacesUtils.bookmarks.eraseEverything());

  // Make sure the bookmarks bar is visible and restore its state on cleanup.
  let toolbar = document.getElementById("PersonalToolbar");
  if (toolbar.collapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
    registerCleanupFunction(function () {
      return promiseSetToolbarVisibility(toolbar, false);
    });
  }

  // Test a bookmark drop for all of the mime types and effects.
  let mimeTypes = ["text/plain", "text/html", "text/x-moz-url"];
  let effects = ["move", "copy", "link"];
  for (let effect of effects) {
    await testDragDrop(effect, mimeTypes);
  }
});

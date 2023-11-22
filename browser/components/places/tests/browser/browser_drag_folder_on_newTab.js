/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_setup(async function () {
  let toolbar = document.getElementById("PersonalToolbar");
  let wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  // Clean before and after so we don't have anything in the folders.
  await PlacesUtils.bookmarks.eraseEverything();

  registerCleanupFunction(async function () {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed) {
      await promiseSetToolbarVisibility(toolbar, false);
    }

    await PlacesUtils.bookmarks.eraseEverything();
  });
});

const TEST_FOLDER_NAME = "Test folder";

add_task(async function test_change_location_from_Toolbar() {
  let newTabButton = document.getElementById("tabs-newtab-button");

  let children = [
    {
      title: "first",
      url: "http://www.example.com/first",
    },
    {
      title: "second",
      url: "http://www.example.com/second",
    },
    {
      type: PlacesUtils.bookmarks.TYPE_SEPARATOR,
    },
    {
      title: "third",
      url: "http://www.example.com/third",
    },
  ];
  let guid = PlacesUtils.history.makeGuid();
  await PlacesUtils.bookmarks.insertTree({
    guid: PlacesUtils.bookmarks.toolbarGuid,
    children: [
      {
        guid,
        type: PlacesUtils.bookmarks.TYPE_FOLDER,
        title: TEST_FOLDER_NAME,
        children,
      },
    ],
  });

  let folder = getToolbarNodeForItemGuid(guid);

  let loadedPromises = children
    .filter(item => "url" in item)
    .map(item =>
      BrowserTestUtils.waitForNewTab(gBrowser, item.url, true, true)
    );

  let srcX = 10,
    srcY = 10;
  // We should drag upwards, since dragging downwards opens menu instead.
  let stepX = 0,
    stepY = -5;

  // We need to dispatch mousemove before dragging, to populate
  // PlacesToolbar._cachedMouseMoveEvent, with the cursor position after the
  // first step, so that the places code detects it as dragging upward.
  EventUtils.synthesizeMouse(folder, srcX + stepX, srcY + stepY, {
    type: "mousemove",
  });

  await EventUtils.synthesizePlainDragAndDrop({
    srcElement: folder,
    destElement: newTabButton,
    srcX,
    srcY,
    stepX,
    stepY,
  });

  let tabs = await Promise.all(loadedPromises);

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }

  ok(true);
});

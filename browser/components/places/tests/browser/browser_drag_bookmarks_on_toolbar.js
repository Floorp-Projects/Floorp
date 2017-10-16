/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL = "http://www.mozilla.org";
const TEST_TITLE = "example_title";

var gBookmarksToolbar = window.document.getElementById("PlacesToolbar");
var dragDirections = { LEFT: 0, UP: 1, RIGHT: 2, DOWN: 3 };

/**
 * Tests dragging on toolbar.
 *
 * We must test these 2 cases:
 *   - Dragging toward left, top, right should start a drag.
 *   - Dragging toward down should should open the container if the item is a
 *     container, drag the item otherwise.
 *
 * @param aElement
 *        DOM node element we will drag
 * @param aExpectedDragData
 *        Array of flavors and values in the form:
 *        [ ["text/plain: sometext", "text/html: <b>sometext</b>"], [...] ]
 *        Pass an empty array to check that drag even has been canceled.
 * @param aDirection
 *        Direction for the dragging gesture, see dragDirections helper object.
 * @return {Promise} Resolved once the drag gesture has been observed.
 */
function synthesizeDragWithDirection(aElement, aExpectedDragData, aDirection) {
  let promise = new Promise(resolve => {
    // Dragstart listener function.
    gBookmarksToolbar.addEventListener("dragstart", function listener(event) {
      info("A dragstart event has been trapped.");
      var dataTransfer = event.dataTransfer;
      is(dataTransfer.mozItemCount, aExpectedDragData.length,
         "Number of dragged items should be the same.");

      for (var t = 0; t < dataTransfer.mozItemCount; t++) {
        var types = dataTransfer.mozTypesAt(t);
        var expecteditem = aExpectedDragData[t];
        is(types.length, expecteditem.length,
          "Number of flavors for item " + t + " should be the same.");

        for (var f = 0; f < types.length; f++) {
          is(types[f], expecteditem[f].substring(0, types[f].length),
             "Flavor " + types[f] + " for item " + t + " should be the same.");
          is(dataTransfer.mozGetDataAt(types[f], t),
             expecteditem[f].substring(types[f].length + 2),
             "Contents for item " + t + " with flavor " + types[f] + " should be the same.");
        }
      }

      if (!aExpectedDragData.length)
        ok(event.defaultPrevented, "Drag has been canceled.");

      event.preventDefault();
      event.stopPropagation();

      gBookmarksToolbar.removeEventListener("dragstart", listener);

      // This is likely to cause a click event, and, in case we are dragging a
      // bookmark, an unwanted page visit.  Prevent the click event.
      aElement.addEventListener("click", prevent);
      EventUtils.synthesizeMouse(aElement,
                                 startingPoint.x + xIncrement * 9,
                                 startingPoint.y + yIncrement * 9,
                                 { type: "mouseup" });
      aElement.removeEventListener("click", prevent);

      // Cleanup eventually opened menus.
      if (aElement.localName == "menu" && aElement.open)
        aElement.open = false;
      resolve();
    });
  });

  var prevent = function(aEvent) { aEvent.preventDefault(); };

  var xIncrement = 0;
  var yIncrement = 0;

  switch (aDirection) {
    case dragDirections.LEFT:
      xIncrement = -1;
      break;
    case dragDirections.RIGHT:
      xIncrement = +1;
      break;
    case dragDirections.UP:
      yIncrement = -1;
      break;
    case dragDirections.DOWN:
      yIncrement = +1;
      break;
  }

  var rect = aElement.getBoundingClientRect();
  var startingPoint = { x: (rect.right - rect.left) / 2,
                        y: (rect.bottom - rect.top) / 2 };

  EventUtils.synthesizeMouse(aElement,
                             startingPoint.x,
                             startingPoint.y,
                             { type: "mousedown" });
  EventUtils.synthesizeMouse(aElement,
                             startingPoint.x + xIncrement * 1,
                             startingPoint.y + yIncrement * 1,
                             { type: "mousemove" });
  EventUtils.synthesizeMouse(aElement,
                             startingPoint.x + xIncrement * 9,
                             startingPoint.y + yIncrement * 9,
                             { type: "mousemove" });

  return promise;
}

function getToolbarNodeForItemId(itemGuid) {
  var children = document.getElementById("PlacesToolbarItems").childNodes;
  var node = null;
  for (var i = 0; i < children.length; i++) {
    if (itemGuid == children[i]._placesNode.bookmarkGuid) {
      node = children[i];
      break;
    }
  }
  return node;
}

function getExpectedDataForPlacesNode(aNode) {
  var wrappedNode = [];
  var flavors = ["text/x-moz-place",
                 "text/x-moz-url",
                 "text/plain",
                 "text/html"];

  flavors.forEach(function(aFlavor) {
    var wrappedFlavor = aFlavor + ": " +
                        PlacesUtils.wrapNode(aNode, aFlavor);
    wrappedNode.push(wrappedFlavor);
  });

  return [wrappedNode];
}

add_task(async function setup() {
  var toolbar = document.getElementById("PersonalToolbar");
  var wasCollapsed = toolbar.collapsed;

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed) {
    await promiseSetToolbarVisibility(toolbar, true);
  }

  registerCleanupFunction(async () => {
    // Collapse the personal toolbar if needed.
    await promiseSetToolbarVisibility(toolbar, false);
  });
});

add_task(async function test_drag_folder_on_toolbar() {
  let folder = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: TEST_TITLE,
    type: PlacesUtils.bookmarks.TYPE_FOLDER,
  });

  var element = getToolbarNodeForItemId(folder.guid);
  isnot(element, null, "Found node on toolbar");

  isnot(element._placesNode, null, "Toolbar node has an associated Places node.");
  var expectedData = getExpectedDataForPlacesNode(element._placesNode);

  info("Dragging left");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.LEFT);

  info("Dragging right");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.RIGHT);

  info("Dragging up");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.UP);

  info("Dragging down");
  await synthesizeDragWithDirection(element, [], dragDirections.DOWN);

  await PlacesUtils.bookmarks.remove(folder);
});

add_task(async function test_drag_bookmark_on_toolbar() {
  var bookmark = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    title: TEST_TITLE,
    url: TEST_URL,
  });

  var element = getToolbarNodeForItemId(bookmark.guid);
  isnot(element, null, "Found node on toolbar");

  isnot(element._placesNode, null, "Toolbar node has an associated Places node.");
  var expectedData = getExpectedDataForPlacesNode(element._placesNode);

  info("Dragging left");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.LEFT);

  info("Dragging right");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.RIGHT);

  info("Dragging up");
  await synthesizeDragWithDirection(element, expectedData, dragDirections.UP);

  info("Dragging down");
  synthesizeDragWithDirection(element, expectedData, dragDirections.DOWN);

  await PlacesUtils.bookmarks.remove(bookmark);
});

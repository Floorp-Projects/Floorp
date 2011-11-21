/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Places test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Marco Bonardo <mak77@bonardo.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
 */
function synthesizeDragWithDirection(aElement, aExpectedDragData, aDirection, aCallback) {
  // Dragstart listener function.
  gBookmarksToolbar.addEventListener("dragstart", function(event)
  {
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
      ok(event.getPreventDefault(), "Drag has been canceled.");

    event.preventDefault();
    event.stopPropagation();

    gBookmarksToolbar.removeEventListener("dragstart", arguments.callee, false);

    // This is likely to cause a click event, and, in case we are dragging a
    // bookmark, an unwanted page visit.  Prevent the click event.
    aElement.addEventListener("click", prevent, false);
    EventUtils.synthesizeMouse(aElement,
                               startingPoint.x + xIncrement * 9,
                               startingPoint.y + yIncrement * 9,
                               { type: "mouseup" });
    aElement.removeEventListener("click", prevent, false);

    // Cleanup eventually opened menus.
    if (aElement.localName == "menu" && aElement.open)
      aElement.open = false;
    aCallback()
  }, false);

  var prevent = function(aEvent) {aEvent.preventDefault();}

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
  var startingPoint = { x: (rect.right - rect.left)/2,
                        y: (rect.bottom - rect.top)/2 };

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
}

function getToolbarNodeForItemId(aItemId) {
  var children = document.getElementById("PlacesToolbarItems").childNodes;
  var node = null;
  for (var i = 0; i < children.length; i++) {
    if (aItemId == children[i]._placesNode.itemId) {
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

var gTests = [

//------------------------------------------------------------------------------

  {
    desc: "Drag a folder on toolbar",
    run: function() {
      // Create a test folder to be dragged.
      var folderId = PlacesUtils.bookmarks
                                .createFolder(PlacesUtils.toolbarFolderId,
                                              TEST_TITLE,
                                              PlacesUtils.bookmarks.DEFAULT_INDEX);
      var element = getToolbarNodeForItemId(folderId);
      isnot(element, null, "Found node on toolbar");

      isnot(element._placesNode, null, "Toolbar node has an associated Places node.");
      var expectedData = getExpectedDataForPlacesNode(element._placesNode);

      info("Dragging left");
      synthesizeDragWithDirection(element, expectedData, dragDirections.LEFT,
        function ()
        {
          info("Dragging right");
          synthesizeDragWithDirection(element, expectedData, dragDirections.RIGHT,
            function ()
            {
              info("Dragging up");
              synthesizeDragWithDirection(element, expectedData, dragDirections.UP,
                function ()
                {
                  info("Dragging down");
                  synthesizeDragWithDirection(element, new Array(), dragDirections.DOWN,
                    function () {
                      // Cleanup.
                      PlacesUtils.bookmarks.removeItem(folderId);
                      nextTest();
                    });
                });
            });
        });
    }
  },

//------------------------------------------------------------------------------

  {
    desc: "Drag a bookmark on toolbar",
    run: function() {
      // Create a test bookmark to be dragged.
      var itemId = PlacesUtils.bookmarks
                              .insertBookmark(PlacesUtils.toolbarFolderId,
                                              PlacesUtils._uri(TEST_URL),
                                              PlacesUtils.bookmarks.DEFAULT_INDEX,
                                              TEST_TITLE);
      var element = getToolbarNodeForItemId(itemId);
      isnot(element, null, "Found node on toolbar");

      isnot(element._placesNode, null, "Toolbar node has an associated Places node.");
      var expectedData = getExpectedDataForPlacesNode(element._placesNode);

      info("Dragging left");
      synthesizeDragWithDirection(element, expectedData, dragDirections.LEFT,
        function ()
        {
          info("Dragging right");
          synthesizeDragWithDirection(element, expectedData, dragDirections.RIGHT,
            function ()
            {
              info("Dragging up");
              synthesizeDragWithDirection(element, expectedData, dragDirections.UP,
                function ()
                {
                  info("Dragging down");
                  synthesizeDragWithDirection(element, expectedData, dragDirections.DOWN,
                    function () {
                      // Cleanup.
                      PlacesUtils.bookmarks.removeItem(itemId);
                      nextTest();
                    });
                });
            });
        });
    }
  },
];

function nextTest() {
  if (gTests.length) {
    var test = gTests.shift();
    waitForFocus(function() {
      info("Start of test: " + test.desc);
      test.run();
    });
  }
  else {
    // Collapse the personal toolbar if needed.
    if (wasCollapsed)
      setToolbarVisibility(toolbar, false);
    finish();
  }
}

let toolbar = document.getElementById("PersonalToolbar");
let wasCollapsed = toolbar.collapsed;

function test() {
  waitForExplicitFinish();

  // Uncollapse the personal toolbar if needed.
  if (wasCollapsed)
    setToolbarVisibility(toolbar, true);

  nextTest();
}


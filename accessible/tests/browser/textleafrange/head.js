/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported createTextLeafPoint, DIRECTION_NEXT, DIRECTION_PREVIOUS,
 BOUNDARY_FLAG_DEFAULT, BOUNDARY_FLAG_INCLUDE_ORIGIN,
 BOUNDARY_FLAG_STOP_IN_EDITABLE, BOUNDARY_FLAG_SKIP_LIST_ITEM_MARKER,
 readablePoint, testPointEqual, textBoundaryGenerator, testBoundarySequence */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.
loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "text.js", dir: MOCHITESTS_DIR },
  { name: "promisified-events.js", dir: MOCHITESTS_DIR }
);

const DIRECTION_NEXT = Ci.nsIAccessibleTextLeafPoint.DIRECTION_NEXT;
const DIRECTION_PREVIOUS = Ci.nsIAccessibleTextLeafPoint.DIRECTION_PREVIOUS;

const BOUNDARY_FLAG_DEFAULT =
  Ci.nsIAccessibleTextLeafPoint.BOUNDARY_FLAG_DEFAULT;
const BOUNDARY_FLAG_INCLUDE_ORIGIN =
  Ci.nsIAccessibleTextLeafPoint.BOUNDARY_FLAG_INCLUDE_ORIGIN;
const BOUNDARY_FLAG_STOP_IN_EDITABLE =
  Ci.nsIAccessibleTextLeafPoint.BOUNDARY_FLAG_STOP_IN_EDITABLE;
const BOUNDARY_FLAG_SKIP_LIST_ITEM_MARKER =
  Ci.nsIAccessibleTextLeafPoint.BOUNDARY_FLAG_SKIP_LIST_ITEM_MARKER;

function createTextLeafPoint(acc, offset) {
  let accService = Cc["@mozilla.org/accessibilityService;1"].getService(
    nsIAccessibilityService
  );

  return accService.createTextLeafPoint(acc, offset);
}

// Converts an nsIAccessibleTextLeafPoint into a human/machine
// readable tuple with a readable accessible and the offset within it.
// For a point text leaf it would look like this: ["hello", 2],
// For a point in an empty input it would look like this ["input#name", 0]
function readablePoint(point) {
  const readableLeaf = acc => {
    let tagName = getAccessibleTagName(acc);
    if (tagName && !tagName.startsWith("_moz_generated")) {
      let domNodeID = getAccessibleDOMNodeID(acc);
      if (domNodeID) {
        return `${tagName}#${domNodeID}`;
      }
      return tagName;
    }

    return acc.name;
  };

  return [readableLeaf(point.accessible), point.offset];
}

function sequenceEqual(val, expected, msg) {
  Assert.deepEqual(val, expected, msg);
}

// eslint-disable-next-line camelcase
function sequenceEqualTodo(val, expected, msg) {
  todo_is(JSON.stringify(val), JSON.stringify(expected), msg);
}

function pointsEqual(pointA, pointB) {
  return (
    pointA.offset == pointB.offset && pointA.accessible == pointB.accessible
  );
}

function testPointEqual(pointA, pointB, msg) {
  is(pointA.offset, pointB.offset, `Offset mismatch - ${msg}`);
  is(pointA.accessible, pointB.accessible, `Accessible mismatch - ${msg}`);
}

function* textBoundaryGenerator(
  firstPoint,
  boundaryType,
  direction,
  flags = BOUNDARY_FLAG_DEFAULT
) {
  // Our start point should be inclusive of the given point.
  let nextLeafPoint = firstPoint.findBoundary(
    boundaryType,
    direction,
    flags | BOUNDARY_FLAG_INCLUDE_ORIGIN
  );
  let textLeafPoint = null;

  do {
    textLeafPoint = nextLeafPoint;
    yield textLeafPoint;
    nextLeafPoint = textLeafPoint.findBoundary(boundaryType, direction, flags);
  } while (!pointsEqual(textLeafPoint, nextLeafPoint));
}

// This function takes FindBoundary arguments and an expected sequence
// of boundary points formatted with readablePoint.
// For example, word starts would look like this:
// [["one two", 0], ["one two", 4], ["one two", 7]]
function testBoundarySequence(
  startPoint,
  boundaryType,
  direction,
  expectedSequence,
  msg,
  options = {}
) {
  let sequence = [
    ...textBoundaryGenerator(
      startPoint,
      boundaryType,
      direction,
      options.flags ? options.flags : BOUNDARY_FLAG_DEFAULT
    ),
  ];
  (options.todo ? sequenceEqualTodo : sequenceEqual)(
    sequence.map(readablePoint),
    expectedSequence,
    msg
  );
}

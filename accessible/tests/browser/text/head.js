/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported createTextLeafPoint, DIRECTION_NEXT, DIRECTION_PREVIOUS,
 BOUNDARY_FLAG_DEFAULT, BOUNDARY_FLAG_INCLUDE_ORIGIN,
 BOUNDARY_FLAG_STOP_IN_EDITABLE, BOUNDARY_FLAG_SKIP_LIST_ITEM_MARKER,
 readablePoint, testPointEqual, textBoundaryGenerator, testBoundarySequence,
 isFinalValueCorrect, isFinalValueCorrect, testInsertText, testDeleteText,
 testCopyText, testPasteText, testCutText, testSetTextContents */

// Load the shared-head file first.
Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/accessible/tests/browser/shared-head.js",
  this
);

// Loading and common.js from accessible/tests/mochitest/ for all tests, as
// well as promisified-events.js.

/* import-globals-from ../../mochitest/role.js */

loadScripts(
  { name: "common.js", dir: MOCHITESTS_DIR },
  { name: "text.js", dir: MOCHITESTS_DIR },
  { name: "role.js", dir: MOCHITESTS_DIR },
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

///////////////////////////////////////////////////////////////////////////////
// Editable text

async function waitForCopy(browser) {
  await BrowserTestUtils.waitForContentEvent(browser, "copy", false, evt => {
    return true;
  });

  let clipboardData = await invokeContentTask(browser, [], async () => {
    let text = await content.navigator.clipboard.readText();
    return text;
  });

  return clipboardData;
}

async function isFinalValueCorrect(
  browser,
  acc,
  expectedTextLeafs,
  msg = "Value is correct"
) {
  let value =
    acc.role == ROLE_ENTRY
      ? acc.value
      : await invokeContentTask(browser, [], () => {
          return content.document.body.textContent;
        });

  let [before, text, after] = expectedTextLeafs;
  let finalValue =
    before && after && !text
      ? [before, after].join(" ")
      : [before, text, after].join("");

  is(value.replace("\xa0", " "), finalValue, msg);
}

function waitForTextChangeEvents(acc, eventSeq) {
  let events = eventSeq.map(eventType => {
    return [eventType, acc];
  });

  if (acc.role == ROLE_ENTRY) {
    events.push([EVENT_TEXT_VALUE_CHANGE, acc]);
  }

  return waitForEvents(events);
}

async function testSetTextContents(acc, text, staticContentOffset, events) {
  acc.QueryInterface(nsIAccessibleEditableText);
  let evtPromise = waitForTextChangeEvents(acc, events);
  acc.setTextContents(text);
  let evt = (await evtPromise)[0];
  evt.QueryInterface(nsIAccessibleTextChangeEvent);
  is(evt.start, staticContentOffset);
}

async function testInsertText(
  acc,
  textToInsert,
  insertOffset,
  staticContentOffset
) {
  acc.QueryInterface(nsIAccessibleEditableText);

  let evtPromise = waitForTextChangeEvents(acc, [EVENT_TEXT_INSERTED]);
  acc.insertText(textToInsert, staticContentOffset + insertOffset);
  let evt = (await evtPromise)[0];
  evt.QueryInterface(nsIAccessibleTextChangeEvent);
  is(evt.start, staticContentOffset + insertOffset);
}

async function testDeleteText(
  acc,
  startOffset,
  endOffset,
  staticContentOffset
) {
  acc.QueryInterface(nsIAccessibleEditableText);

  let evtPromise = waitForTextChangeEvents(acc, [EVENT_TEXT_REMOVED]);
  acc.deleteText(
    staticContentOffset + startOffset,
    staticContentOffset + endOffset
  );
  let evt = (await evtPromise)[0];
  evt.QueryInterface(nsIAccessibleTextChangeEvent);
  is(evt.start, staticContentOffset + startOffset);
}

async function testCopyText(
  acc,
  startOffset,
  endOffset,
  staticContentOffset,
  browser,
  aExpectedClipboard = null
) {
  acc.QueryInterface(nsIAccessibleEditableText);
  let copied = waitForCopy(browser);
  acc.copyText(
    staticContentOffset + startOffset,
    staticContentOffset + endOffset
  );
  let clipboardText = await copied;
  if (aExpectedClipboard != null) {
    is(clipboardText, aExpectedClipboard, "Correct text in clipboard");
  }
}

async function testPasteText(acc, insertOffset, staticContentOffset) {
  acc.QueryInterface(nsIAccessibleEditableText);
  let evtPromise = waitForTextChangeEvents(acc, [EVENT_TEXT_INSERTED]);
  acc.pasteText(staticContentOffset + insertOffset);

  let evt = (await evtPromise)[0];
  evt.QueryInterface(nsIAccessibleTextChangeEvent);
  // XXX: In non-headless mode pasting text produces several text leaves
  // and the offset is not what we expect.
  // is(evt.start, staticContentOffset + insertOffset);
}

async function testCutText(acc, startOffset, endOffset, staticContentOffset) {
  acc.QueryInterface(nsIAccessibleEditableText);
  let evtPromise = waitForTextChangeEvents(acc, [EVENT_TEXT_REMOVED]);
  acc.cutText(
    staticContentOffset + startOffset,
    staticContentOffset + endOffset
  );

  let evt = (await evtPromise)[0];
  evt.QueryInterface(nsIAccessibleTextChangeEvent);
  is(evt.start, staticContentOffset + startOffset);
}

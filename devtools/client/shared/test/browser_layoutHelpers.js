/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that scrollIntoViewIfNeeded works properly.
const {scrollIntoViewIfNeeded} = require("devtools/client/shared/scroll");

const TEST_URI = TEST_URI_ROOT + "doc_layoutHelpers.html";

add_task(function* () {
  let [host, win] = yield createHost("bottom", TEST_URI);
  runTest(win);
  host.destroy();
});

function runTest(win) {
  let some = win.document.getElementById("some");

  some.style.top = win.innerHeight + "px";
  some.style.left = win.innerWidth + "px";
  // The tests start with a black 2x2 pixels square below bottom right.
  // Do not resize the window during the tests.

  let xPos = Math.floor(win.innerWidth / 2);
  // Above the viewport.
  win.scroll(xPos, win.innerHeight + 2);
  scrollIntoViewIfNeeded(some);
  is(win.scrollY, Math.floor(win.innerHeight / 2) + 1,
     "Element completely hidden above should appear centered.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // On the top edge.
  win.scroll(win.innerWidth / 2, win.innerHeight + 1);
  scrollIntoViewIfNeeded(some);
  is(win.scrollY, win.innerHeight,
     "Element partially visible above should appear above.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // Just below the viewport.
  win.scroll(win.innerWidth / 2, 0);
  scrollIntoViewIfNeeded(some);
  is(win.scrollY, Math.floor(win.innerHeight / 2) + 1,
     "Element completely hidden below should appear centered.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // On the bottom edge.
  win.scroll(win.innerWidth / 2, 1);
  scrollIntoViewIfNeeded(some);
  is(win.scrollY, 2,
     "Element partially visible below should appear below.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // Above the viewport.
  win.scroll(win.innerWidth / 2, win.innerHeight + 2);
  scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, win.innerHeight,
     "Element completely hidden above should appear above " +
     "if parameter is false.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // On the top edge.
  win.scroll(win.innerWidth / 2, win.innerHeight + 1);
  scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, win.innerHeight,
     "Element partially visible above should appear above " +
     "if parameter is false.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // Below the viewport.
  win.scroll(win.innerWidth / 2, 0);
  scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, 2,
     "Element completely hidden below should appear below " +
     "if parameter is false.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");

  // On the bottom edge.
  win.scroll(win.innerWidth / 2, 1);
  scrollIntoViewIfNeeded(some, false);
  is(win.scrollY, 2,
     "Element partially visible below should appear below " +
     "if parameter is false.");
  is(win.scrollX, xPos,
     "scrollX position has not changed.");
}

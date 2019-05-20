/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// When stepping into a function, 'let' variables should show as uninitialized
// instead of undefined.

function findNodeValue(dbg, text) {
  for (let index = 0;; index++) {
    var elem = findElement(dbg, "scopeNode", index);
    if (elem && elem.innerText == text) {
      return findElement(dbg, "scopeValue", index).innerText;
    }
  }
}

add_task(async function test() {
  const dbg = await initDebugger("doc-step-in-uninitialized.html");
  invokeInTab("main");
  await waitForPaused(dbg, "doc-step-in-uninitialized.html");

  await stepOver(dbg);
  await stepIn(dbg);

  assertDebugLine(dbg, 8);
  assertPausedLocation(dbg);

  // We step past the 'let x' at the start of the function because it is not
  // a breakpoint position.
  ok(findNodeValue(dbg, "x") == "undefined", "x undefined");
  ok(findNodeValue(dbg, "y") == "(uninitialized)", "y uninitialized");

  await stepOver(dbg);

  assertDebugLine(dbg, 9);

  ok(findNodeValue(dbg, "y") == "3", "y initialized");
});

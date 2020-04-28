/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests minfied + source maps.

add_task(async function() {
  const dbg = await initDebugger("doc-minified2.html", "sum.js");
  dbg.actions.toggleMapScopes();

  await selectSource(dbg, "sum.js");
  await addBreakpoint(dbg, "sum.js", 2);

  invokeInTab("test");
  await waitForPaused(dbg);

  is(getScopeNodeLabel(dbg, 1), "sum", "check scope label");
  is(getScopeNodeLabel(dbg, 2), "first", "check scope label");
  is(getScopeNodeValue(dbg, 2), "40", "check scope value");
  is(getScopeNodeLabel(dbg, 3), "second", "check scope label");
  is(getScopeNodeValue(dbg, 3), "2", "check scope value");
  is(getScopeNodeLabel(dbg, 4), "Window", "check scope label");
});

function getScopeNodeLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getScopeNodeValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

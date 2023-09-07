/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

PromiseTestUtils.allowMatchingRejectionsGlobally(/Current state is running/);
PromiseTestUtils.allowMatchingRejectionsGlobally(/Connection closed/);

// Test that unusual objects have their contents shown in worker thread scopes.
add_task(async function () {
  const dbg = await initDebugger("doc-worker-scopes.html", "scopes-worker.js");

  await selectSource(dbg, "scopes-worker.js");
  await addBreakpointViaGutter(dbg, 11);
  await dbg.toolbox.target.waitForRequestsToSettle();
  invokeInTab("startWorker");
  await waitForPaused(dbg, "scopes-worker.js");
  await removeBreakpointViaGutter(dbg, 11);
  // We should be paused at the first line of simple-worker.js
  const workerSource2 = dbg.selectors.getSelectedSource();
  assertPausedAtSourceAndLine(dbg, workerSource2.id, 11);

  await toggleNode(dbg, "var_array");
  ok(findNodeValue(dbg, "0") == '"mango"', "array elem0");
  ok(findNodeValue(dbg, "1") == '"pamplemousse"', "array elem1");
  ok(findNodeValue(dbg, "2") == '"pineapple"', "array elem2");
  await toggleNode(dbg, "var_array");

  await toggleNode(dbg, "var_tarray");
  ok(findNodeValue(dbg, "0") == "42", "tarray elem0");
  ok(findNodeValue(dbg, "1") == "43", "tarray elem1");
  ok(findNodeValue(dbg, "2") == "44", "tarray elem2");
  await toggleNode(dbg, "var_tarray");

  await toggleNode(dbg, "var_set");
  await toggleNode(dbg, "<entries>");

  ok(findNodeValue(dbg, "0") == '"papaya"', "set elem0");
  ok(findNodeValue(dbg, "1") == '"banana"', "set elem1");
  await toggleNode(dbg, "var_set");

  await toggleNode(dbg, "var_map");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  ok(findNodeValue(dbg, "<key>"), "1");
  ok(findNodeValue(dbg, "<value>"), '"one"');
  await toggleNode(dbg, "0");
  await toggleNode(dbg, "1");
  ok(findNodeValue(dbg, "<key>"), "2");
  ok(findNodeValue(dbg, "<value>"), '"two"');
  await toggleNode(dbg, "var_map");

  await toggleNode(dbg, "var_weakmap");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  await toggleNode(dbg, "<key>");
  ok(findNodeValue(dbg, "foo"), "foo");
  await toggleNode(dbg, "<value>");
  ok(findNodeValue(dbg, "bar"), "bar");
  await toggleNode(dbg, "var_weakmap");

  await toggleNode(dbg, "var_weakset");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  ok(findNodeValue(dbg, "foo"), "foo");
  await toggleNode(dbg, "var_weakset");

  // Close the scopes in order to unmount the reps in order to force spawning
  // the various `release` RDP requests which, otherwise, would happen in
  // middle of the toolbox destruction. Then, wait for them to finish.
  await toggleScopes(dbg);
  await waitForRequestsToSettle(dbg);
});

function findNode(dbg, text) {
  for (let index = 0; ; index++) {
    const elem = findElement(dbg, "scopeNode", index);
    if (elem?.innerText == text) {
      return elem;
    }
  }
}

function toggleNode(dbg, text) {
  return toggleObjectInspectorNode(findNode(dbg, text));
}

function findNodeValue(dbg, text) {
  for (let index = 0; ; index++) {
    const elem = findElement(dbg, "scopeNode", index);
    if (elem?.innerText == text) {
      return getScopeNodeValue(dbg, index);
    }
  }
}

async function removeBreakpointViaGutter(dbg, line) {
  const onRemoved = waitForDispatch(dbg.store, "REMOVE_BREAKPOINT");
  await clickGutter(dbg, line);
  await onRemoved;
}

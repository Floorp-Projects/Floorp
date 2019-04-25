/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// This error shows up sometimes when running the test, and while this is a
// strange problem that shouldn't be happening it doesn't prevent the test from
// completing successfully.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/Current state is running/);

function findNode(dbg, text) {
  for (let index = 0;; index++) {
    var elem = findElement(dbg, "scopeNode", index);
    if (elem && elem.innerText == text) {
      return elem;
    }
  }
}

function toggleNode(dbg, text) {
  return toggleObjectInspectorNode(findNode(dbg, text));
}

function findNodeValue(dbg, text) {
  for (let index = 0;; index++) {
    var elem = findElement(dbg, "scopeNode", index);
    if (elem && elem.innerText == text) {
      return findElement(dbg, "scopeValue", index).innerText;
    }
  }
}

// Test that unusual objects have their contents shown in worker thread scopes.
add_task(async function() {
  const dbg = await initDebugger("doc-worker-scopes.html", "scopes-worker.js");
  const workerSource = findSource(dbg, "scopes-worker.js");

  await addBreakpoint(dbg, workerSource, 11);
  invokeInTab("startWorker");
  await waitForPaused(dbg, "scopes-worker.js");
  await removeBreakpoint(dbg, workerSource.id, 11);

  // We should be paused at the first line of simple-worker.js
  assertPausedAtSourceAndLine(dbg, workerSource.id, 11);

  await toggleNode(dbg, "var_array");
  ok(findNodeValue(dbg, "0") == "\"mango\"", "array elem0");
  ok(findNodeValue(dbg, "1") == "\"pamplemousse\"", "array elem1");
  ok(findNodeValue(dbg, "2") == "\"pineapple\"", "array elem2");
  await toggleNode(dbg, "var_array");

  await toggleNode(dbg, "var_tarray");
  ok(findNodeValue(dbg, "0") == "42", "tarray elem0");
  ok(findNodeValue(dbg, "1") == "43", "tarray elem1");
  ok(findNodeValue(dbg, "2") == "44", "tarray elem2");
  await toggleNode(dbg, "var_tarray");

  await toggleNode(dbg, "var_set");
  await toggleNode(dbg, "<entries>");

  ok(findNodeValue(dbg, "0") == "\"papaya\"", "set elem0");
  ok(findNodeValue(dbg, "1") == "\"banana\"", "set elem1");
  await toggleNode(dbg, "var_set");

  await toggleNode(dbg, "var_map");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  ok(findNodeValue(dbg, "<key>"), "1");
  ok(findNodeValue(dbg, "<value>"), "\"one\"");
  await toggleNode(dbg, "0");
  await toggleNode(dbg, "1");
  ok(findNodeValue(dbg, "<key>"), "2");
  ok(findNodeValue(dbg, "<value>"), "\"two\"");
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
});

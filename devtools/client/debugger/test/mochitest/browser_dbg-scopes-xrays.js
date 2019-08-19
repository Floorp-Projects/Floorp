/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

async function checkObjectNode(dbg, text, value) {
  await toggleNode(dbg, text);
  ok(findNodeValue(dbg, "a") == value, "object value");
}

// Test that xrays do not interfere with examining objects in the scopes pane.
add_task(async function() {
  const dbg = await initDebugger("doc-scopes-xrays.html");

  invokeInTab("start");
  await waitForPaused(dbg, "doc-scopes-xrays.html");

  await toggleNode(dbg, "set");
  await toggleNode(dbg, "<entries>");
  await checkObjectNode(dbg, "0", "1");
  await toggleNode(dbg, "set");

  await toggleNode(dbg, "weakset");
  await toggleNode(dbg, "<entries>");
  await checkObjectNode(dbg, "0", "2");
  await toggleNode(dbg, "weakset");

  await toggleNode(dbg, "map");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  await checkObjectNode(dbg, "<key>", "3");
  await toggleNode(dbg, "<key>");
  await checkObjectNode(dbg, "<value>", "4");
  await toggleNode(dbg, "map");

  await toggleNode(dbg, "weakmap");
  await toggleNode(dbg, "<entries>");
  await toggleNode(dbg, "0");
  await checkObjectNode(dbg, "<key>", "5");
  await toggleNode(dbg, "<key>");
  await checkObjectNode(dbg, "<value>", "6");
  await toggleNode(dbg, "weakmap");
});

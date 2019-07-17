/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Ignore strange errors when shutting down.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/No such actor/);
PromiseTestUtils.whitelistRejectionsGlobally(/connection just closed/);

const MaxItems = 10;

function findNode(dbg, text) {
  for (let index = 0; index < MaxItems; index++) {
    var elem = findElement(dbg, "scopeNode", index);
    if (elem && elem.innerText == text) {
      return elem;
    }
  }
  return null;
}

async function toggleNode(dbg, text) {
  const node = await waitUntilPredicate(() => findNode(dbg, text));
  return toggleObjectInspectorNode(node);
}

// Test that expanded scopes stay expanded after resuming and pausing again.
add_task(async function() {
  const dbg = await initDebugger("doc-remember-expanded-scopes.html");
  invokeInTab("main", "doc-remember-expanded-scopes.html");
  await waitForPaused(dbg);

  const MaxItems = 10;

  await toggleNode(dbg, "object");
  await toggleNode(dbg, "innerObject");
  await stepOver(dbg);
  await waitForPaused(dbg);

  await waitUntil(() => findNode(dbg, "innerData"));
  ok("Inner object data automatically expanded after stepping");
});

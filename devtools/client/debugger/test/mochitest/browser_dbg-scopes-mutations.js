/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-script-mutate.html");

  let onPaused = waitForPaused(dbg);
  invokeInTab("mutate");
  await onPaused;
  await waitForSelectedSource(dbg, "script-mutate");
  await waitForDispatch(dbg.store, "ADD_INLINE_PREVIEW");

  is(
    getScopeNodeLabel(dbg, 2),
    "<this>",
    'The second element in the scope panel is "<this>"'
  );
  is(
    getScopeNodeLabel(dbg, 4),
    "phonebook",
    'The fourth element in the scope panel is "phonebook"'
  );

  info("Expand `phonebook`");
  await expandNode(dbg, 4);
  is(
    getScopeNodeLabel(dbg, 5),
    "S",
    'The fifth element in the scope panel is "S"'
  );

  info("Expand `S`");
  await expandNode(dbg, 5);
  is(
    getScopeNodeLabel(dbg, 6),
    "sarah",
    'The sixth element in the scope panel is "sarah"'
  );
  is(
    getScopeNodeLabel(dbg, 7),
    "serena",
    'The seventh element in the scope panel is "serena"'
  );

  info("Expand `sarah`");
  await expandNode(dbg, 6);
  is(
    getScopeNodeLabel(dbg, 7),
    "lastName",
    'The seventh element in the scope panel is now "lastName"'
  );
  is(
    getScopeNodeValue(dbg, 7),
    '"Doe"',
    'The "lastName" element has the expected "Doe" value'
  );

  await resume(dbg);
  await waitForPaused(dbg);
  await waitForDispatch(dbg.store, "ADD_INLINE_PREVIEW");

  is(
    getScopeNodeLabel(dbg, 2),
    "<this>",
    'The second element in the scope panel is "<this>"'
  );
  is(
    getScopeNodeLabel(dbg, 4),
    "phonebook",
    'The fourth element in the scope panel is "phonebook"'
  );
});

function getScopeNodeLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getScopeNodeValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

function expandNode(dbg, index) {
  const node = findElement(dbg, "scopeNode", index);
  const objectInspector = node.closest(".object-inspector");
  const properties = objectInspector.querySelectorAll(".node").length;
  findElement(dbg, "scopeNode", index).click();
  return waitUntil(
    () => objectInspector.querySelectorAll(".node").length !== properties
  );
}

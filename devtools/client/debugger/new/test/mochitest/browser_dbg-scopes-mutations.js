/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function getScopeNodeLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getScopeNodeValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

function expandNode(dbg, index) {
  let onLoadProperties = onLoadObjectProperties(dbg);
  findElement(dbg, "scopeNode", index).click();
  return onLoadProperties;
}

function onLoadObjectProperties(dbg) {
  return waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-mutate.html");

  let onPaused = waitForPaused(dbg);
  invokeInTab("mutate");
  await onPaused;
  await waitForLoadedSource(dbg, "script-mutate");

  is(
    getScopeNodeLabel(dbg, 2),
    "<this>",
    'The second element in the scope panel is "<this>"'
  );
  is(
    getScopeNodeLabel(dbg, 3),
    "phonebook",
    'The third element in the scope panel is "phonebook"'
  );

  info("Expand `phonebook`");
  await expandNode(dbg, 3);
  is(
    getScopeNodeLabel(dbg, 4),
    "S",
    'The fourth element in the scope panel is "S"'
  );

  info("Expand `S`");
  await expandNode(dbg, 4);
  is(
    getScopeNodeLabel(dbg, 5),
    "sarah",
    'The fifth element in the scope panel is "sarah"'
  );
  is(
    getScopeNodeLabel(dbg, 6),
    "serena",
    'The sixth element in the scope panel is "serena"'
  );

  info("Expand `sarah`");
  await expandNode(dbg, 5);
  is(
    getScopeNodeLabel(dbg, 6),
    "lastName",
    'The sixth element in the scope panel is now "lastName"'
  );
  is(
    getScopeNodeValue(dbg, 6),
    '"Doe"',
    'The "lastName" element has the expected "Doe" value'
  );

  await resume(dbg);
  await waitForPaused(dbg);

  is(
    getScopeNodeLabel(dbg, 2),
    "<this>",
    'The second element in the scope panel is "<this>"'
  );
  is(
    getScopeNodeLabel(dbg, 3),
    "phonebook",
    'The third element in the scope panel is "phonebook"'
  );
});

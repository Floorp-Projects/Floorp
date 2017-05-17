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
  clickElement(dbg, "scopeNode", index);
  return onLoadProperties;
}

function toggleScopes(dbg) {
  return findElement(dbg, "scopesHeader").click();
}

function onLoadObjectProperties(dbg) {
  return waitForDispatch(dbg, "LOAD_OBJECT_PROPERTIES");
}

add_task(async function() {
  const dbg = await initDebugger("doc-script-mutate.html");

  toggleScopes(dbg);

  let onPaused = waitForPaused(dbg);
  invokeInTab("mutate");
  await onPaused;

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

  // Expand `phonebook`
  await expandNode(dbg, 3);
  is(
    getScopeNodeLabel(dbg, 4),
    "S",
    'The fourth element in the scope panel is "S"'
  );

  // Expand `S`
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

  // Expand `sarah`
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

  info("Resuming");
  onPaused = waitForPaused(dbg);
  await resume(dbg);
  await onPaused;

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

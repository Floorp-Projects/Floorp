/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests minfied + source maps.

async function foo() {
  return new Promise(r => setTimeout(r, 3000000));
}

function getScopeNodeLabel(dbg, index) {
  return findElement(dbg, "scopeNode", index).innerText;
}

function getScopeNodeValue(dbg, index) {
  return findElement(dbg, "scopeValue", index).innerText;
}

add_task(async function() {
  await pushPref("devtools.debugger.features.map-scopes", true);

  const dbg = await initDebugger("doc-minified2.html");

  await waitForSources(dbg, "sum.js");

  await selectSource(dbg, "sum.js");
  await addBreakpoint(dbg, "sum.js", 2);

  invokeInTab("test");
  await waitForPaused(dbg);
  await waitForMappedScopes(dbg);

  is(getScopeNodeLabel(dbg, 1), "sum", "check scope label");
  is(getScopeNodeLabel(dbg, 2), "<this>", "check scope label");
  is(getScopeNodeLabel(dbg, 3), "arguments", "check scope label");

  is(getScopeNodeLabel(dbg, 4), "first", "check scope label");
  is(getScopeNodeValue(dbg, 4), "40", "check scope value");
  is(getScopeNodeLabel(dbg, 5), "second", "check scope label");
  is(getScopeNodeValue(dbg, 5), "2", "check scope value");
});

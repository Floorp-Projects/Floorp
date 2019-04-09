/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests re-opening pretty printed tabs on load

add_task(async function() {
  const dbg = await initDebugger("doc-minified.html", "math.min.js");

  await selectSource(dbg, "math.min.js");
  clickElement(dbg, "prettyPrintButton");
  await waitForSource(dbg, "math.min.js:formatted");
  // Test reloading the debugger
  await waitForSelectedSource(dbg, "math.min.js:formatted");
  await reload(dbg);

  await waitForSelectedSource(dbg, "math.min.js:formatted");
  ok(true, "Pretty printed source is selected on reload");

  await selectSource(dbg, "math.min.js:formatted");
  const source = findSource(dbg, "math.min.js:formatted");
  dbg.actions.showSource(getContext(dbg), source.id);
  const focusedTreeElement = findElementWithSelector(dbg, ".sources-list .focused .label");
  is(focusedTreeElement.textContent.trim(), "math.min.js", "Pretty printed source is selected in tree");
});

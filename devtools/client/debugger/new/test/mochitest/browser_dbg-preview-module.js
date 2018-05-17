/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test hovering in a script that is paused on load
// and doesn't have functions.
add_task(async function() {
  const dbg = await initDebugger("doc-scripts.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  navigate(dbg, "doc-on-load.html");

  // wait for `top-level.js` to load and to pause at a debugger statement
  await waitForSelectedSource(dbg, "top-level.js");
  await waitForPaused(dbg);

  await assertPreviews(dbg, [
    {
      line: 1,
      column: 6,
      expression: "obj",
      fields: [["foo", "1"], ["bar", "2"]]
    }
  ]);

  await assertPreviewTooltip(dbg, 2, 7, { result: "3", expression: "func" });
});

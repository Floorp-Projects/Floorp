/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the source tree works.

function getLabel(dbg, index) {
  return findElement(dbg, "sourceNode", index)
    .textContent.trim()
    .replace(/^[\s\u200b]*/g, "");
}

add_task(async function() {
  const dbg = await initDebugger("doc-sources-querystring.html");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  await waitForSources(dbg, "simple1.js?x=1", "simple1.js?x=2");

  // Expand nodes and make sure more sources appear.
  await assertSourceCount(dbg, 2);
  await clickElement(dbg, "sourceDirectoryLabel", 2);

  const labels = [getLabel(dbg, 4), getLabel(dbg, 3)];
  is(
    labels.includes("simple1.js?x=1") && labels.includes("simple1.js?x=2"),
    true,
    "simple1.js?x=1 and simple2.jsx=2 exist"
  )
});

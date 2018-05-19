/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure named eval sources appear in the list.

async function waitForSourceCount(dbg, i) {
  // We are forced to wait until the DOM nodes appear because the
  // source tree batches its rendering.
  await waitUntil(() => {
    return findAllElements(dbg, "sourceNodes").length === i;
  });
}

function getLabel(dbg, index) {
  return findElement(dbg, "sourceNode", index)
    .textContent.trim()
    .replace(/^[\s\u200b]*/g, "");
}

add_task(async function() {
  const dbg = await initDebugger("doc-sources.html");
  const {
    selectors: { getSelectedSource },
    getState
  } = dbg;

  await waitForSources(dbg, "simple1", "simple2", "nested-source", "long.js");

  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.eval("window.evaledFunc = function() {} //# sourceURL=evaled.js");
  });

  await waitForSourceCount(dbg, 3);

  is(getLabel(dbg, 3), "evaled.js", "evaled exists");
});

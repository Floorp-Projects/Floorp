/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the source tree works.

function* waitForSourceCount(dbg, i) {
  // We are forced to wait until the DOM nodes appear because the
  // source tree batches its rendering.
  yield waitUntil(() => {
    return findAllElements(dbg, "sourceNodes").length === i;
  });
}

add_task(function* () {
  const dbg = yield initDebugger("doc-sources.html");
  const { selectors: { getSelectedSource }, getState } = dbg;

  yield waitForSources(dbg, "simple1");

  // Expand nodes and make sure more sources appear.
  is(findAllElements(dbg, "sourceNodes").length, 2);

  clickElement(dbg, "sourceArrow", 2);
  is(findAllElements(dbg, "sourceNodes").length, 7);

  clickElement(dbg, "sourceArrow", 3);
  is(findAllElements(dbg, "sourceNodes").length, 8);

  // Select a source.
  ok(!findElementWithSelector(dbg, ".sources-list .focused"),
     "Source is not focused");
  const selected = waitForDispatch(dbg, "SELECT_SOURCE");
  clickElement(dbg, "sourceNode", 4);
  yield selected;
  ok(findElementWithSelector(dbg, ".sources-list .focused"),
     "Source is focused");
  ok(getSelectedSource(getState()).get("url").includes("nested-source.js"),
     "The right source is selected");

  // Make sure new sources appear in the list.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    const script = content.document.createElement("script");
    script.src = "math.min.js";
    content.document.body.appendChild(script);
  });

  yield waitForSourceCount(dbg, 9);
  is(findElement(dbg, "sourceNode", 7).textContent,
     "math.min.js",
     "The dynamic script exists");

  // Make sure named eval sources appear in the list.
  ContentTask.spawn(gBrowser.selectedBrowser, null, function() {
    content.eval("window.evaledFunc = function() {} //# sourceURL=evaled.js");
  });
  yield waitForSourceCount(dbg, 11);
  is(findElement(dbg, "sourceNode", 2).textContent,
     "evaled.js",
     "The eval script exists");
});

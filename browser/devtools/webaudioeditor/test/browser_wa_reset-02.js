/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading a tab with the tools open properly cleans up
 * the graph.
 */

add_task(function*() {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $ } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let { nodes, edges } = countGraphObjects(panelWin);
  is(nodes, 3, "should only be 3 nodes.");
  is(edges, 2, "should only be 2 edges.");

  reload(target);

  [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  ({ nodes, edges } = countGraphObjects(panelWin));
  is(nodes, 3, "after reload, should only be 3 nodes.");
  is(edges, 2, "after reload, should only be 2 edges.");

  yield teardown(target);
});

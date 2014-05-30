/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests reloading a tab with the tools open properly cleans up
 * the graph.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $ } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let { nodes, edges } = countGraphObjects(panelWin);
  ise(nodes, 3, "should only be 3 nodes.");
  ise(edges, 2, "should only be 2 edges.");

  reload(target);

  let [actors] = yield Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let { nodes, edges } = countGraphObjects(panelWin);
  ise(nodes, 3, "after reload, should only be 3 nodes.");
  ise(edges, 2, "after reload, should only be 2 edges.");

  yield teardown(panel);
  finish();
}

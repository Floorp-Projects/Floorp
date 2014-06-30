/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that selected nodes stay selected on graph redraw.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(CONNECT_TOGGLE_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2)
  ]);

  let nodeIDs = actors.map(actor => actor.actorID);

  yield clickGraphNode(panelWin, nodeIDs[1]);
  ok(findGraphNode(panelWin, nodeIDs[1]).classList.contains("selected"),
    "Node selected once.");

  yield once(panelWin, EVENTS.UI_GRAPH_RENDERED);
  
  ok(findGraphNode(panelWin, nodeIDs[1]).classList.contains("selected"),
    "Node still selected after rerender.");

  yield teardown(panel);
  finish();
}


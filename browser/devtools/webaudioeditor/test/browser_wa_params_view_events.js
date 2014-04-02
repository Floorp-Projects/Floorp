/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the shader editor shows the appropriate UI when opened.
 */

function spawnTest() {
  let [target, debuggee, panel] = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { gFront, $, $$, EVENTS } = panel.panelWin;

  let started = once(gFront, "start-context");

  reload(target);

  let [[dest, osc, gain], [[_, destID], [_, oscID], [_, gainID]]] = yield Promise.all([
    get3(gFront, "create-node"),
    get3Spread(panel.panelWin, EVENTS.UI_ADD_NODE_LIST)
  ]);

  is(dest.actorID, destID, "EVENTS.UI_ADD_NODE_LIST fired for node with ID");
  is(osc.actorID, oscID, "EVENTS.UI_ADD_NODE_LIST fired for node with ID");
  is(gain.actorID, gainID, "EVENTS.UI_ADD_NODE_LIST fired for node with ID");

  yield teardown(panel);
  finish();
}

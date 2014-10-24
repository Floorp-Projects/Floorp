/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

function spawnTest() {
  let { target, panel } = yield initWebAudioEditor(CONNECT_TOGGLE_PARAM_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 1, 0)
  ]);
  ok(true, "Graph rendered without param connection");

  yield waitForGraphRendered(panelWin, 3, 1, 1);
  ok(true, "Graph re-rendered upon param connection");

  yield teardown(panel);
  finish();
}


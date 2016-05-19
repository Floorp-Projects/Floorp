/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  reload(target);

  let [actors] = yield Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2, 0)
  ]);

  let [dest, osc, gain] = actors;

  yield osc.disconnect();

  osc.connectParam(gain, "gain");
  yield waitForGraphRendered(panelWin, 3, 1, 1);
  ok(true, "Graph re-rendered upon param connection");

  yield teardown(target);
});

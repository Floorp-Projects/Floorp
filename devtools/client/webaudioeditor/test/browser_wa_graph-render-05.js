/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2, 0)
  ]);
  reload(target);
  let [actors] = await events;
  let [dest, osc, gain] = actors;

  await osc.disconnect();

  osc.connectParam(gain, "gain");
  await waitForGraphRendered(panelWin, 3, 1, 1);
  ok(true, "Graph re-rendered upon param connection");

  await teardown(target);
});

/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS } = panelWin;

  const events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 2, 0)
  ]);
  reload(target);
  const [actors] = await events;
  const [dest, osc, gain] = actors;

  await osc.disconnect();

  osc.connectParam(gain, "gain");
  await waitForGraphRendered(panelWin, 3, 1, 1);
  ok(true, "Graph re-rendered upon param connection");

  await teardown(target);
});

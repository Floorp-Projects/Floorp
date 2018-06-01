/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that AudioNodeActor#connectParam() work.
 * Uses the editor front as the actors do not retain connect state.
 */

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  const events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  const [actors] = await events;
  const [dest, osc, gain] = actors;

  await osc.disconnect();

  osc.connectParam(gain, "gain");
  await Promise.all([
    waitForGraphRendered(panelWin, 3, 1, 1),
    once(gAudioNodes, "connect")
  ]);
  ok(true, "Oscillator connect to Gain's Gain AudioParam, event emitted.");

  await teardown(target);
});

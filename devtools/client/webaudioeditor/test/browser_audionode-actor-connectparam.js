/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that AudioNodeActor#connectParam() work.
 * Uses the editor front as the actors do not retain connect state.
 */

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  let events = Promise.all([
    get3(gFront, "create-node"),
    waitForGraphRendered(panelWin, 3, 2)
  ]);
  reload(target);
  let [actors] = yield events;
  let [dest, osc, gain] = actors;

  yield osc.disconnect();

  osc.connectParam(gain, "gain");
  yield Promise.all([
    waitForGraphRendered(panelWin, 3, 1, 1),
    once(gAudioNodes, "connect")
  ]);
  ok(true, "Oscillator connect to Gain's Gain AudioParam, event emitted.");

  yield teardown(target);
});

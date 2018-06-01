/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that AudioNodeActor#connectNode() and AudioNodeActor#disconnect() work.
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

  info("Disconnecting oscillator...");
  osc.disconnect();
  await Promise.all([
    waitForGraphRendered(panelWin, 3, 1),
    once(gAudioNodes, "disconnect")
  ]);
  ok(true, "Oscillator disconnected, event emitted.");

  info("Reconnecting oscillator...");
  osc.connectNode(gain);
  await Promise.all([
    waitForGraphRendered(panelWin, 3, 2),
    once(gAudioNodes, "connect")
  ]);
  ok(true, "Oscillator reconnected.");

  await teardown(target);
});

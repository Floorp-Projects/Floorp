/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1125817
 * Tests to ensure that disconnecting a node immediately
 * after creating it does not fail.
 */

const BUG_1125817_URL = EXAMPLE_URL + "doc_bug_1125817.html";

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(BUG_1125817_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  const events = Promise.all([
    once(gAudioNodes, "add", 2),
    once(gAudioNodes, "disconnect"),
    waitForGraphRendered(panelWin, 2, 0)
  ]);
  reload(target);
  await events;

  ok(true, "Successfully disconnected a just-created node.");

  await teardown(target);
});

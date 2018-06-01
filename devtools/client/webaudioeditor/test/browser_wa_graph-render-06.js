/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

const BUG_1141261_URL = EXAMPLE_URL + "doc_bug_1141261.html";

add_task(async function() {
  const { target, panel } = await initWebAudioEditor(BUG_1141261_URL);
  const { panelWin } = panel;
  const { gFront, $, $$, EVENTS } = panelWin;

  const events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 1, 0)
  ]);
  reload(target);
  await events;

  ok(true, "Graph correctly shows gain node as disconnected");

  await teardown(target);
});

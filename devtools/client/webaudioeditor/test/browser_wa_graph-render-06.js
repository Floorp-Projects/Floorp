/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests to ensure that param connections trigger graph redraws
 */

const BUG_1141261_URL = EXAMPLE_URL + "doc_bug_1141261.html";

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(BUG_1141261_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS } = panelWin;

  let events = Promise.all([
    getN(gFront, "create-node", 3),
    waitForGraphRendered(panelWin, 3, 1, 0)
  ]);
  reload(target);
  yield events;

  ok(true, "Graph correctly shows gain node as disconnected");

  yield teardown(target);
});

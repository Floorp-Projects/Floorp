/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Bug 1130901
 * Tests to ensure that calling call/apply on methods wrapped
 * via CallWatcher do not throw a security permissions error:
 * "Error: Permission denied to access property 'call'"
 */

const BUG_1130901_URL = EXAMPLE_URL + "doc_bug_1130901.html";

add_task(function* () {
  let { target, panel } = yield initWebAudioEditor(BUG_1130901_URL);
  let { panelWin } = panel;
  let { gFront, $, $$, EVENTS, gAudioNodes } = panelWin;

  let rendered = waitForGraphRendered(panelWin, 3, 0);
  reload(target);
  yield rendered;

  ok(true, "Successfully created a node from AudioContext via `call`.");
  ok(true, "Successfully created a node from AudioContext via `apply`.");

  yield teardown(target);
});

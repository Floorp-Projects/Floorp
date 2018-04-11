/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests that reloading a tab will properly listen for the `start-context`
 * event and reshow the tools after reloading.
 */

add_task(async function() {
  let { target, panel } = await initWebAudioEditor(SIMPLE_CONTEXT_URL);
  let { gFront, $ } = panel.panelWin;

  is($("#reload-notice").hidden, false,
    "The 'reload this page' notice should initially be visible.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should initially be hidden.");
  is($("#content").hidden, true,
    "The tool's content should initially be hidden.");

  let navigating = once(target, "will-navigate");
  let started = once(gFront, "start-context");

  reload(target);

  await navigating;

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden when navigating.");
  is($("#waiting-notice").hidden, false,
    "The 'waiting for an audio context' notice should be visible when navigating.");
  is($("#content").hidden, true,
    "The tool's content should still be hidden.");

  await started;

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden after context found.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should be hidden after context found.");
  is($("#content").hidden, false,
    "The tool's content should not be hidden anymore.");

  navigating = once(target, "will-navigate");
  started = once(gFront, "start-context");

  reload(target);

  await Promise.all([navigating, started]);
  let rendered = waitForGraphRendered(panel.panelWin, 3, 2);

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden after context found after reload.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should be hidden after context found after reload.");
  is($("#content").hidden, false,
    "The tool's content should reappear without closing and reopening the toolbox.");

  await rendered;

  await teardown(target);
});

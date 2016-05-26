/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// /////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("Error: Connection closed");

/**
 * Tests that switching to an iframe works fine.
 */

add_task(function* () {
  Services.prefs.setBoolPref("devtools.command-button-frames.enabled", true);

  let { target, panel, toolbox } = yield initWebAudioEditor(IFRAME_CONTEXT_URL);
  let { gFront, $ } = panel.panelWin;

  is($("#reload-notice").hidden, false,
    "The 'reload this page' notice should initially be visible.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should initially be hidden.");
  is($("#content").hidden, true,
    "The tool's content should initially be hidden.");

  let btn = toolbox.doc.getElementById("command-button-frames");
  ok(!btn.firstChild, "The frame list button has no children");

  // Open frame menu and wait till it's available on the screen.
  let menu = toolbox.showFramesMenu({target: btn});
  yield once(menu, "open");

  let frames = menu.menuitems;
  is(frames.length, 2, "We have both frames in the list");

  // Select the iframe
  frames[1].click();

  let navigating = once(target, "will-navigate");

  yield navigating;

  is($("#reload-notice").hidden, false,
    "The 'reload this page' notice should still be visible when switching to a frame.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should be kept hidden when switching to a frame.");
  is($("#content").hidden, true,
    "The tool's content should still be hidden.");

  navigating = once(target, "will-navigate");
  let started = once(gFront, "start-context");

  reload(target);

  yield Promise.all([navigating, started]);

  is($("#reload-notice").hidden, true,
    "The 'reload this page' notice should be hidden after reloading the frame.");
  is($("#waiting-notice").hidden, true,
    "The 'waiting for an audio context' notice should be hidden after reloading the frame.");
  is($("#content").hidden, false,
    "The tool's content should appear after reload.");

  yield teardown(target);
});

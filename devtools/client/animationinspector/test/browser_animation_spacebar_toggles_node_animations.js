/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the spacebar key press toggles the play/resume button state.
// This test doesn't need to test if animations actually pause/resume
// because there's an other test that does this.
// There are animations in the test page and since, by default, the <body> node
// is selected, animations will be displayed in the timeline, so the timeline
// play/resume button will be displayed
add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {panel, window} = yield openAnimationInspector();
  let {playTimelineButtonEl} = panel;

  // ensure the focus is on the animation panel
  window.focus();

  info("Simulate spacebar stroke and check playResume button" +
       " is in paused state");

  // sending the key will lead to a UI_UPDATE_EVENT
  let onUpdated = panel.once(panel.UI_UPDATED_EVENT);
  EventUtils.sendKey("SPACE", window);
  yield onUpdated;
  ok(playTimelineButtonEl.classList.contains("paused"),
    "The play/resume button is in its paused state");

  info("Simulate spacebar stroke and check playResume button" +
       " is in playing state");

  // sending the key will lead to a UI_UPDATE_EVENT
  onUpdated = panel.once(panel.UI_UPDATED_EVENT);
  EventUtils.sendKey("SPACE", window);
  yield onUpdated;
  ok(!playTimelineButtonEl.classList.contains("paused"),
    "The play/resume button is in its play state again");
});

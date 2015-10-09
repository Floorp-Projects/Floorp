/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the update of the animation panel participate in the
// inspector-updated event. This means that the test verifies that the
// inspector-updated event is emitted *after* the animation panel is ready.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, controller} = yield openAnimationInspector();

  info("Listen for the players-updated, ui-updated and inspector-updated events");
  let receivedEvents = [];
  controller.once(controller.PLAYERS_UPDATED_EVENT, () => {
    receivedEvents.push(controller.PLAYERS_UPDATED_EVENT);
  });
  panel.once(panel.UI_UPDATED_EVENT, () => {
    receivedEvents.push(panel.UI_UPDATED_EVENT);
  });
  inspector.once("inspector-updated", () => {
    receivedEvents.push("inspector-updated");
  });

  info("Selecting an animated node");
  let node = yield getNodeFront(".animated", inspector);
  yield selectNode(node, inspector);

  info("Check that all events were received");
  // Only assert that the inspector-updated event is last, the order of the
  // first 2 events is irrelevant.

  is(receivedEvents.length, 3, "3 events were received");
  is(receivedEvents[2], "inspector-updated",
     "The third event received was the inspector-updated event");

  ok(receivedEvents.indexOf(controller.PLAYERS_UPDATED_EVENT) !== -1,
     "The players-updated event was received");
  ok(receivedEvents.indexOf(panel.UI_UPDATED_EVENT) !== -1,
     "The ui-updated event was received");
});

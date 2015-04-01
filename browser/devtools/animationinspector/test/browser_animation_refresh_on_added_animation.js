/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the panel content refreshes when new animations are added.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {toolbox, inspector, panel} = yield openAnimationInspector();

  info("Select a non animated node");
  yield selectNode(".still", inspector);

  is(panel.playersEl.querySelectorAll(".player-widget").length, 0,
    "There are no player widgets in the panel");

  info("Listen to the next UI update event");
  let onPanelUpdated = panel.once(panel.UI_UPDATED_EVENT);

  info("Start an animation on the node");
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball animated"
  });

  yield onPanelUpdated;
  ok(true, "The panel update event was fired");

  is(panel.playersEl.querySelectorAll(".player-widget").length, 1,
    "There is one player widget in the panel");
});

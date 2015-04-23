/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when animations end, the corresponding player widgets are disabled.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, controller} = yield openAnimationInspector();

  info("Select the test node");
  yield selectNode(".still", inspector);

  info("Apply 2 finite animations to the test node and wait for the widgets to appear");
  let onUiUpdated = panel.once(panel.UI_UPDATED_EVENT);
  yield executeInContent("devtools:test:setAttribute", {
    selector: ".still",
    attributeName: "class",
    attributeValue: "ball still multi-finite"
  });
  yield onUiUpdated;

  is(controller.animationPlayers.length, 2, "2 animation players exist");

  info("Wait for both animations to end");

  let promises = controller.animationPlayers.map(front => {
    return waitForPlayState(front, "finished");
  });

  yield promise.all(promises);

  for (let widgetEl of panel.playersEl.querySelectorAll(".player-widget")) {
    ok(widgetEl.classList.contains("finished"), "The player widget has the right class");
  }
});

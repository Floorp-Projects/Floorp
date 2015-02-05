/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that if the animation's duration, iterations or delay change in
// content, then the widget reflects the changes.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel, inspector} = yield openAnimationInspector();

  info("Select the test node");
  yield selectNode(".animated", inspector);

  info("Get the player widget");
  let widget = panel.playerWidgets[0];

  yield setStyle(widget, "animationDuration", "5.5s");
  is(widget.metaDataComponent.durationValue.textContent, "5.50s",
    "The widget shows the new duration");

  yield setStyle(widget, "animationIterationCount", "300");
  is(widget.metaDataComponent.iterationValue.textContent, "300",
    "The widget shows the new iteration count");

  yield setStyle(widget, "animationDelay", "45s");
  is(widget.metaDataComponent.delayValue.textContent, "45s",
    "The widget shows the new delay");
});

function* setStyle(widget, name, value) {
  info("Change the animation style via the content DOM. Setting " +
    name + " to " + value);
  yield executeInContent("Test:SetNodeStyle", {
    propertyName: name,
    propertyValue: value
  }, {
    node: getNode(".animated")
  });

  info("Wait for the next state update");
  yield widget.player.once(widget.player.AUTO_REFRESH_EVENT);
}

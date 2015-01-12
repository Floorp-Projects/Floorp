/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that player widgets are destroyed correctly when needed.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Select an animated node");
  yield selectNode(".multi", inspector);

  info("Hold on to one of the player widget instances to test it after destroy");
  let widget = panel.playerWidgets[0];

  info("Select another node to get the previous widgets destroyed");
  yield selectNode(".animated", inspector);

  ok(widget.destroyed, "The widget's destroyed flag is true");
});

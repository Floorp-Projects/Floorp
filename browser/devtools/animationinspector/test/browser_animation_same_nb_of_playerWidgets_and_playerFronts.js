/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that when playerFronts are updated, the same number of playerWidgets
// are created in the panel.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, controller} = yield openAnimationInspector();

  info("Selecting the test animated node");
  yield selectNode(".multi", inspector);

  is(controller.animationPlayers.length, panel.playerWidgets.length,
    "As many playerWidgets were created as there are playerFronts");

  for (let widget of panel.playerWidgets) {
    ok(widget.initialized, "The player widget is initialized");
    is(widget.el.parentNode, panel.playersEl,
      "The player widget has been appended to the panel");
  }
});

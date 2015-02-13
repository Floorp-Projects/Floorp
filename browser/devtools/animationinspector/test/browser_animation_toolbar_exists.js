/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the animation panel has a top toolbar that contains the play/pause
// button and that is displayed at all times.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel, window} = yield openAnimationInspector();
  let doc = window.document;

  let toolbar = doc.querySelector("#toolbar");
  ok(toolbar, "The panel contains the toolbar element");
  ok(toolbar.querySelector("#toggle-all"), "The toolbar contains the toggle button");
  ok(isNodeVisible(toolbar), "The toolbar is visible");

  info("Select an animated node");
  yield selectNode(".animated", inspector);

  toolbar = doc.querySelector("#toolbar");
  ok(toolbar, "The panel still contains the toolbar element");
  ok(isNodeVisible(toolbar), "The toolbar is still visible");
});

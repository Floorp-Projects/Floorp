/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Test that the animation panel has a top toolbar that contains the play/pause
// button and that is displayed at all times.
// Also test that this toolbar gets replaced by the timeline toolbar when there
// are animations to be displayed.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, window} = yield openAnimationInspector();
  let doc = window.document;
  let toolbar = doc.querySelector("#global-toolbar");

  ok(toolbar, "The panel contains the toolbar element with the new UI");
  ok(!isNodeVisible(toolbar),
     "The toolbar is hidden while there are animations");

  let timelineToolbar = doc.querySelector("#timeline-toolbar");
  ok(timelineToolbar, "The panel contains a timeline toolbar element");
  ok(isNodeVisible(timelineToolbar),
     "The timeline toolbar is visible when there are animations");

  info("Select a node that has no animations");
  yield selectNodeAndWaitForAnimations(".still", inspector);

  ok(isNodeVisible(toolbar),
     "The toolbar is shown when there are no animations");
  ok(!isNodeVisible(timelineToolbar),
     "The timeline toolbar is hidden when there are no animations");
});

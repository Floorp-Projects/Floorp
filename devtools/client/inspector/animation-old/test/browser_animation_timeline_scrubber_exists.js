/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline does have a scrubber element.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {panel} = await openAnimationInspector();

  const timeline = panel.animationsTimelineComponent;
  const scrubberEl = timeline.scrubberEl;

  ok(scrubberEl, "The scrubber element exists");
  ok(scrubberEl.classList.contains("scrubber"), "It has the right classname");
});

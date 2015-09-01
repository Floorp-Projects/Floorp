/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that the timeline-based UI displays animations' duration, delay and
// iteration counts in tooltips.

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_simple_animation.html");
  let {panel} = yield openAnimationInspectorNewUI();
  yield waitForAllAnimationTargets(panel);

  info("Getting the animation element from the panel");
  let timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  let timeBlockNameEls = timelineEl.querySelectorAll(".time-block .name");

  // Verify that each time-block's name element has a tooltip that looks sort of
  // ok. We don't need to test the actual content.
  for (let el of timeBlockNameEls) {
    ok(el.hasAttribute("title"), "The tooltip is defined");

    let title = el.getAttribute("title");
    ok(title.match(/Delay: [\d.]+s/), "The tooltip shows the delay");
    ok(title.match(/Duration: [\d.]+s/), "The tooltip shows the delay");
    ok(title.match(/Repeats: /), "The tooltip shows the iterations");
  }
});

/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the timeline displays animations' duration, delay iteration
// counts and iteration start in tooltips.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {panel, controller} = await openAnimationInspector();

  info("Getting the animation element from the panel");
  const timelineEl = panel.animationsTimelineComponent.rootWrapperEl;
  const timeBlockNameEls = timelineEl.querySelectorAll(".time-block .name");

  // Verify that each time-block's name element has a tooltip that looks sort of
  // ok. We don't need to test the actual content.
  [...timeBlockNameEls].forEach((el, i) => {
    ok(el.hasAttribute("title"), "The tooltip is defined for animation " + i);

    const title = el.getAttribute("title");
    const state = controller.animationPlayers[i].state;

    if (state.delay) {
      ok(title.match(/Delay: [\d.,-]+s/), "The tooltip shows the delay");
    }
    ok(title.match(/Duration: [\d.,]+s/), "The tooltip shows the duration");
    if (state.endDelay) {
      ok(title.match(/End delay: [\d.,-]+s/), "The tooltip shows the endDelay");
    }
    if (state.iterationCount !== 1) {
      ok(title.match(/Repeats: /), "The tooltip shows the iterations");
    } else {
      ok(!title.match(/Repeats: /), "The tooltip doesn't show the iterations");
    }
    if (state.easing && state.easing !== "linear") {
      ok(title.match(/Overall easing: /), "The tooltip shows the easing");
    } else {
      ok(!title.match(/Overall easing: /),
         "The tooltip doesn't show the easing if it is 'linear'");
    }
    if (state.animationTimingFunction && state.animationTimingFunction !== "ease") {
      is(state.type, "cssanimation",
         "The animation type should be CSS Animations if has animation-timing-function");
      ok(title.match(/Animation timing function: /),
         "The tooltip shows animation-timing-function");
    } else {
      ok(!title.match(/Animation timing function: /),
         "The tooltip doesn't show the animation-timing-function if it is 'ease'"
         + " or not CSS Animations");
    }
    if (state.fill) {
      ok(title.match(/Fill: /), "The tooltip shows the fill");
    }
    if (state.direction) {
      if (state.direction === "normal") {
        ok(!title.match(/Direction: /),
          "The tooltip doesn't show the direction if it is 'normal'");
      } else {
        ok(title.match(/Direction: /), "The tooltip shows the direction");
      }
    }
    ok(!title.match(/Iteration start:/),
      "The tooltip doesn't show the iteration start");
  });
});

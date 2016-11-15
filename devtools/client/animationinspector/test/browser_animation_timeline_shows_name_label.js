/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check the text content and width of name label.

add_task(function* () {
  yield addTab(URL_ROOT + "doc_simple_animation.html");
  let {inspector, panel} = yield openAnimationInspector();

  info("Selecting 'simple-animation' animation which is running on compositor");
  yield selectNodeAndWaitForAnimations(".animated", inspector);
  checkNameLabel(panel.animationsTimelineComponent.rootWrapperEl, "simple-animation");

  info("Selecting 'no-compositor' animation which is not running on compositor");
  yield selectNodeAndWaitForAnimations(".no-compositor", inspector);
  checkNameLabel(panel.animationsTimelineComponent.rootWrapperEl, "no-compositor");
});

function checkNameLabel(rootWrapperEl, expectedLabelContent) {
  const timeblockEl = rootWrapperEl.querySelector(".time-block");
  const labelEl = rootWrapperEl.querySelector(".name div");
  is(labelEl.textContent, expectedLabelContent,
     `Text content of labelEl sould be ${ expectedLabelContent }`);

  // Expand timeblockEl to avoid max-width of the label.
  timeblockEl.style.width = "10000px";
  const originalLabelWidth = labelEl.clientWidth;
  ok(originalLabelWidth < timeblockEl.clientWidth / 2,
     "Label width should be less than 50%");

  // Set timeblockEl width to double of original label width.
  timeblockEl.style.width = `${ originalLabelWidth * 2 }px`;
  is(labelEl.clientWidth + labelEl.offsetLeft, originalLabelWidth,
     `Label width + offsetLeft should be ${ originalLabelWidth }px`);

  // Shrink timeblockEl to enable max-width.
  timeblockEl.style.width = `${ originalLabelWidth }px`;
  is(labelEl.clientWidth + labelEl.offsetLeft,
     Math.round(timeblockEl.clientWidth / 2),
     "Label width + offsetLeft should be half of timeblockEl");
}

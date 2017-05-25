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
  const labelEl = rootWrapperEl.querySelector(".name svg text");
  is(labelEl.textContent, expectedLabelContent,
     `Text content of labelEl sould be ${ expectedLabelContent }`);
}

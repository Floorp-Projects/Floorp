/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check the text content and width of name label.

add_task(async function() {
  await addTab(URL_ROOT + "doc_simple_animation.html");
  const {inspector, panel} = await openAnimationInspector();

  info("Selecting 'simple-animation' animation which is running on compositor");
  await selectNodeAndWaitForAnimations(".animated", inspector);
  checkNameLabel(panel.animationsTimelineComponent.rootWrapperEl, "simple-animation");

  info("Selecting 'no-compositor' animation which is not running on compositor");
  await selectNodeAndWaitForAnimations(".no-compositor", inspector);
  checkNameLabel(panel.animationsTimelineComponent.rootWrapperEl, "no-compositor");
});

function checkNameLabel(rootWrapperEl, expectedLabelContent) {
  const labelEl = rootWrapperEl.querySelector(".name svg text");
  is(labelEl.textContent, expectedLabelContent,
     `Text content of labelEl sould be ${ expectedLabelContent }`);
}

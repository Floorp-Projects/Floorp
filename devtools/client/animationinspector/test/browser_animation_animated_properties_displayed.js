/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that when an animation is selected, its list of animated properties is
// displayed below it.

const EXPECTED_PROPERTIES = [
  "background-color",
  "background-position",
  "background-size",
  "border-bottom-left-radius",
  "border-bottom-right-radius",
  "border-top-left-radius",
  "border-top-right-radius",
  "filter",
  "height",
  "transform",
  "width"
].sort();

add_task(function*() {
  yield addTab(TEST_URL_ROOT + "doc_keyframes.html");
  let {panel} = yield openAnimationInspector();
  let timeline = panel.animationsTimelineComponent;
  let propertiesList = timeline.rootWrapperEl
                               .querySelector(".animated-properties");

  ok(!isNodeVisible(propertiesList),
     "The list of properties panel is hidden by default");

  info("Click to select the animation");
  yield clickOnAnimation(panel, 0);

  ok(isNodeVisible(propertiesList),
     "The list of properties panel is shown");
  ok(propertiesList.querySelectorAll(".property").length,
     "The list of properties panel actually contains properties");
  ok(hasExpectedProperties(propertiesList),
     "The list of proeprties panel contains the right properties");

  info("Click to unselect the animation");
  yield clickOnAnimation(panel, 0, true);

  ok(!isNodeVisible(propertiesList),
     "The list of properties panel is hidden again");
});

function hasExpectedProperties(containerEl) {
  let names = [...containerEl.querySelectorAll(".property .name")]
              .map(n => n.textContent)
              .sort();

  if (names.length !== EXPECTED_PROPERTIES.length) {
    return false;
  }

  for (let i = 0; i < names.length; i++) {
    if (names[i] !== EXPECTED_PROPERTIES[i]) {
      return false;
    }
  }

  return true;
}

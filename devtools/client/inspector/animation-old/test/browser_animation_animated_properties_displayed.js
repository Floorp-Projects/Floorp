/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const LAYOUT_ERRORS_L10N =
  new LocalizationHelper("toolkit/locales/layout_errors.properties");

// Test that when an animation is selected, its list of animated properties is
// displayed below it.

const EXPECTED_PROPERTIES = [
  "border-bottom-left-radius",
  "border-bottom-right-radius",
  "border-top-left-radius",
  "border-top-right-radius",
  "filter",
  "height",
  "transform",
  "width",
  // Unchanged value properties
  "background-attachment",
  "background-clip",
  "background-color",
  "background-image",
  "background-origin",
  "background-position-x",
  "background-position-y",
  "background-repeat",
  "background-size"
].sort();

add_task(async function() {
  await addTab(URL_ROOT + "doc_keyframes.html");
  const {panel} = await openAnimationInspector();
  const timeline = panel.animationsTimelineComponent;
  const propertiesList = timeline.rootWrapperEl
                               .querySelector(".animated-properties");

  // doc_keyframes.html has only one animation,
  // so the propertiesList shoud be shown.
  ok(isNodeVisible(propertiesList),
     "The list of properties panel shoud be shown");

  ok(propertiesList.querySelectorAll(".property").length,
     "The list of properties panel actually contains properties");
  ok(hasExpectedProperties(propertiesList),
     "The list of properties panel contains the right properties");
  ok(hasExpectedWarnings(propertiesList),
     "The list of properties panel contains the right warnings");

  info("Click same animation again");
  await clickOnAnimation(panel, 0, true);
  ok(isNodeVisible(propertiesList),
     "The list of properties panel keeps");
});

function hasExpectedProperties(containerEl) {
  const names = [...containerEl.querySelectorAll(".property .name")]
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

function hasExpectedWarnings(containerEl) {
  const warnings = [...containerEl.querySelectorAll(".warning")];
  for (const warning of warnings) {
    const warningID =
      "CompositorAnimationWarningTransformWithSyncGeometricAnimations";
    if (warning.getAttribute("title") == LAYOUT_ERRORS_L10N.getStr(warningID)) {
      return true;
    }
  }
  return false;
}

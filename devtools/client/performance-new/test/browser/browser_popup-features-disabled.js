/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that the popup offers to restart the browser to set an enviroment flag."
  );

  const supportedFeatures = Services.profiler.GetFeatures();
  const allFeatures = Services.profiler.GetAllFeatures();
  const unsupportedFeatures = allFeatures.filter(
    feature => !supportedFeatures.includes(feature)
  );

  if (unsupportedFeatures.length === 0) {
    ok(true, "This platform has no unsupported features. Skip this test.");
    return;
  }

  await makeSureProfilerPopupIsEnabled();
  toggleOpenProfilerPopup();

  {
    info("Open up the features section.");
    const features = await getElementFromPopupByText("Features:");
    features.click();
  }

  {
    info("Find and click a supported feature to toggle it.");
    const [firstSupportedFeature] = supportedFeatures;
    const checkbox = getFeatureCheckbox(firstSupportedFeature);
    const initialValue = checkbox.checked;
    info("Click the supported checkbox.");
    checkbox.click();
    is(initialValue, !checkbox.checked, "A supported feature can be toggled.");
    checkbox.click();
  }

  {
    info("Find and click an unsupported feature, it should be disabled.");
    const [firstUnsupportedFeature] = unsupportedFeatures;
    const checkbox = getFeatureCheckbox(firstUnsupportedFeature);
    is(checkbox.checked, false, "The unsupported feature is not checked.");

    info("Click the unsupported checkbox.");
    checkbox.click();
    is(checkbox.checked, false, "After clicking it, it's still not checked.");
  }

  await closePopup();
});

/**
 * @param {string} feature
 * @return {HTMLElement}
 */
function getFeatureCheckbox(feature) {
  const element = getIframeDocument().querySelector(
    `input[value="${feature}"]`
  );
  if (!element) {
    throw new Error("Could not find the checkbox for the feature: " + feature);
  }
  return element;
}

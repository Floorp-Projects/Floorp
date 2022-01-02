/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(async function test() {
  info(
    "Test that features that are disabled on the platform are disabled in about:profiling."
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

  await withAboutProfiling(async document => {
    {
      info("Find and click a supported feature to toggle it.");
      const [firstSupportedFeature] = supportedFeatures;
      const checkbox = getFeatureCheckbox(document, firstSupportedFeature);
      const initialValue = checkbox.checked;
      info("Click the supported checkbox.");
      checkbox.click();
      is(
        initialValue,
        !checkbox.checked,
        "A supported feature can be toggled."
      );
      checkbox.click();
    }

    {
      info("Find and click an unsupported feature, it should be disabled.");
      const [firstUnsupportedFeature] = unsupportedFeatures;
      const checkbox = getFeatureCheckbox(document, firstUnsupportedFeature);
      is(checkbox.checked, false, "The unsupported feature is not checked.");

      info("Click the unsupported checkbox.");
      checkbox.click();
      is(checkbox.checked, false, "After clicking it, it's still not checked.");
    }
  });
});

/**
 * @param {HTMLDocument} document
 * @param {string} feature
 * @return {HTMLElement}
 */
function getFeatureCheckbox(document, feature) {
  const element = document.querySelector(`input[value="${feature}"]`);
  if (!element) {
    throw new Error("Could not find the checkbox for the feature: " + feature);
  }
  return element;
}

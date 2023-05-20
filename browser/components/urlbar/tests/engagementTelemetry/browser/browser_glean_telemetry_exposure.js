/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SPONSORED_QUERY = "sponsored";
const NONSPONSORED_QUERY = "nonsponsored";

// test for exposure events
add_setup(async function () {
  await initExposureTest();
});

add_task(async function exposureSponsoredOnEngagement() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", "rs_adm_sponsored"],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: SPONSORED_QUERY,
    trigger: () => doClick(),
    assert: () => assertExposureTelemetry([{ results: "rs_adm_sponsored" }]),
  });
});

add_task(async function exposureSponsoredOnAbandonment() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", "rs_adm_sponsored"],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: SPONSORED_QUERY,
    trigger: () => doBlur(),
    assert: () => assertExposureTelemetry([{ results: "rs_adm_sponsored" }]),
  });
});

add_task(async function exposureFilter() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", "rs_adm_sponsored"],
      ["browser.urlbar.showExposureResults", false],
    ],
    query: SPONSORED_QUERY,
    select: async () => {
      // assert that the urlbar has no results
      Assert.equal(await getResultByType("rs_adm_sponsored"), null);
    },
    trigger: () => doBlur(),
    assert: () => assertExposureTelemetry([{ results: "rs_adm_sponsored" }]),
  });
});

add_task(async function innerQueryExposure() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", "rs_adm_sponsored"],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: NONSPONSORED_QUERY,
    select: () => {},
    trigger: async () => {
      // delete the old query
      gURLBar.select();
      EventUtils.synthesizeKey("KEY_Backspace");
      await openPopup(SPONSORED_QUERY);
      await defaultSelect(SPONSORED_QUERY);
      await doClick();
    },
    assert: () => assertExposureTelemetry([{ results: "rs_adm_sponsored" }]),
  });
});

add_task(async function innerQueryInvertedExposure() {
  await doExposureTest({
    prefs: [
      ["browser.urlbar.exposureResults", "rs_adm_sponsored"],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: SPONSORED_QUERY,
    select: () => {},
    trigger: async () => {
      // delete the old query
      gURLBar.select();
      EventUtils.synthesizeKey("KEY_Backspace");
      await openPopup(NONSPONSORED_QUERY);
      await defaultSelect(SPONSORED_QUERY);
      await doClick();
    },
    assert: () => assertExposureTelemetry([{ results: "rs_adm_sponsored" }]),
  });
});

add_task(async function multipleProviders() {
  await doExposureTest({
    prefs: [
      [
        "browser.urlbar.exposureResults",
        "rs_adm_sponsored,rs_adm_nonsponsored",
      ],
      ["browser.urlbar.showExposureResults", true],
    ],
    query: NONSPONSORED_QUERY,
    trigger: () => doClick(),
    assert: () => assertExposureTelemetry([{ results: "rs_adm_nonsponsored" }]),
  });
});

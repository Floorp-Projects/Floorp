/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that maxTouchPoints is updated when touch simulation is toggled.

const TEST_URL = "http://example.com/";

addRDMTask(
  TEST_URL,
  async function({ ui }) {
    info("Toggling on touch simulation.");
    reloadOnTouchChange(true);
    await toggleTouchSimulation(ui);

    info(
      "Test value maxTouchPoints is non-zero when touch simulation is enabled."
    );
    await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
      is(
        content.navigator.maxTouchPoints,
        1,
        "navigator.maxTouchPoints should be 1"
      );
    });

    info("Toggling off touch simulation.");
    await toggleTouchSimulation(ui);
    await SpecialPowers.spawn(ui.getViewportBrowser(), [], async function() {
      is(
        content.navigator.maxTouchPoints,
        0,
        "navigator.maxTouchPoints should be 0"
      );
    });
    reloadOnTouchChange(false);
  },
  { usingBrowserUI: true }
);

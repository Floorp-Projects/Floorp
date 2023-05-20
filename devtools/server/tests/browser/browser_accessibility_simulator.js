/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  accessibility: {
    SIMULATION_TYPE: { PROTANOPIA },
  },
} = require("resource://devtools/shared/constants.js");
const {
  simulation: {
    COLOR_TRANSFORMATION_MATRICES: {
      PROTANOPIA: PROTANOPIA_MATRIX,
      NONE: DEFAULT_MATRIX,
    },
  },
} = require("resource://devtools/server/actors/accessibility/constants.js");

// Checks for the SimulatorActor

async function setup() {
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.window.testColorMatrix = function (actual, expected) {
      for (const idx in actual) {
        is(
          actual[idx].toFixed(3),
          expected[idx].toFixed(3),
          "Color matrix value is set correctly."
        );
      }
    };
  });
  SimpleTest.registerCleanupFunction(async function () {
    await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
      content.window.testColorMatrix = null;
    });
  });
}

async function testSimulate(simulator, matrix, type = null) {
  const matrixApplied = await simulator.simulate({ types: type ? [type] : [] });
  ok(matrixApplied, "Simulation color matrix is successfully applied.");

  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [[type, matrix]],
    ([simulationType, simulationMatrix]) => {
      const { window } = content;
      info(
        `Test that color matrix is set to ${
          simulationType || "default"
        } simulation values.`
      );
      window.testColorMatrix(
        window.docShell.getColorMatrix(),
        simulationMatrix
      );
    }
  );
}

add_task(async function () {
  const { target, accessibility } = await initAccessibilityFrontsForUrl(
    MAIN_DOMAIN + "doc_accessibility.html",
    { enableByDefault: false }
  );

  const simulator = accessibility.simulatorFront;
  if (!simulator) {
    ok(false, "Missing simulator actor.");
    return;
  }

  await setup();

  info("Test that protanopia is successfully simulated.");
  await testSimulate(simulator, PROTANOPIA_MATRIX, PROTANOPIA);

  info(
    "Test that simulations are successfully removed by setting default color matrix."
  );
  await testSimulate(simulator, DEFAULT_MATRIX);

  await target.destroy();
  gBrowser.removeCurrentTab();
});

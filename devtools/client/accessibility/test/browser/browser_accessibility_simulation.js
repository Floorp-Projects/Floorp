/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global openSimulationMenu, toggleSimulationOption */

const TEST_URI = `<html>
  <head>
    <meta charset="utf-8"/>
    <title>Accessibility Simulations Test</title>
  </head>
  <body>
    <h1 style="color:blue; background-color:rgba(255,255,255,1);">
      Top level header
    </h1>
    <h2 style="color:green; background-color:rgba(255,255,255,1);">
      Second level header
    </h2>
  </body>
</html>`;

/**
 * Test data has the format of:
 * {
 *   desc     {String}    description for better logging
 *   setup    {Function}  An optional setup that needs to be performed before
 *                        the state of the simulation components can be checked.
 *   expected {JSON}      An expected states for the simulation components.
 * }
 */
const tests = [
  {
    desc: "Check that the menu button is inactivate and the menu is closed initially.",
    expected: {
      simulation: {
        buttonActive: false,
      },
    },
  },
  {
    desc: "Clicking the menu button shows the menu with No Simulation selected.",
    setup: async ({ doc }) => {
      await openSimulationMenu(doc);
    },
    expected: {
      simulation: {
        buttonActive: false,
        checkedOptionIndices: [0],
      },
    },
  },
  {
    desc: "Selecting an option renders the menu button active and closes the menu.",
    setup: async ({ doc }) => {
      await toggleSimulationOption(doc, 2);
    },
    expected: {
      simulation: {
        buttonActive: true,
        checkedOptionIndices: [2],
      },
    },
  },
  {
    desc: "Reopening the menu preserves the previously selected option.",
    setup: async ({ doc }) => {
      await openSimulationMenu(doc);
    },
    expected: {
      simulation: {
        buttonActive: true,
        checkedOptionIndices: [2],
      },
    },
  },
  {
    desc: "Unselecting the option renders the button inactive and closes the menu.",
    setup: async ({ doc }) => {
      await toggleSimulationOption(doc, 2);
    },
    expected: {
      simulation: {
        buttonActive: false,
        checkedOptionIndices: [0],
      },
    },
  },
];

/**
 * Test that checks state of simulation button and menu when
 * options are selected/unselected with web render enabled.
 */
addA11yPanelTestsTask(
  tests,
  TEST_URI,
  "Test selecting and unselecting simulation options updates UI."
);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const AccessibilityTreeFilter = createFactory(
  require("devtools/client/accessibility/components/AccessibilityTreeFilter")
);
const AccessibilityPrefs = createFactory(
  require("devtools/client/accessibility/components/AccessibilityPrefs")
);
loader.lazyGetter(this, "SimulationMenuButton", function() {
  return createFactory(
    require("devtools/client/accessibility/components/SimulationMenuButton")
  );
});

function Toolbar({ toolboxDoc, audit, simulate }) {
  const optionalSimulationSection = simulate
    ? [
        div({
          role: "separator",
          className: "devtools-separator",
        }),
        SimulationMenuButton({ simulate, toolboxDoc }),
      ]
    : [];

  return div(
    {
      className: "devtools-toolbar",
      role: "toolbar",
    },
    AccessibilityTreeFilter({ audit, toolboxDoc }),
    // Simulation section is shown if webrender is enabled
    ...optionalSimulationSection,
    AccessibilityPrefs({ toolboxDoc })
  );
}

// Exports from this module
exports.Toolbar = Toolbar;

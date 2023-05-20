/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const AccessibilityTreeFilter = createFactory(
  require("resource://devtools/client/accessibility/components/AccessibilityTreeFilter.js")
);
const AccessibilityPrefs = createFactory(
  require("resource://devtools/client/accessibility/components/AccessibilityPrefs.js")
);
loader.lazyGetter(this, "SimulationMenuButton", function () {
  return createFactory(
    require("resource://devtools/client/accessibility/components/SimulationMenuButton.js")
  );
});
const DisplayTabbingOrder = createFactory(
  require("resource://devtools/client/accessibility/components/DisplayTabbingOrder.js")
);

const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");

function Toolbar({ audit, simulate, supportsTabbingOrder, toolboxDoc }) {
  const optionalSimulationSection = simulate
    ? [
        div({
          role: "separator",
          className: "devtools-separator",
        }),
        SimulationMenuButton({ simulate, toolboxDoc }),
      ]
    : [];
  const optionalDisplayTabbingOrderSection = supportsTabbingOrder
    ? [
        div({
          role: "separator",
          className: "devtools-separator",
        }),
        DisplayTabbingOrder(),
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
    ...optionalDisplayTabbingOrderSection,
    AccessibilityPrefs({ toolboxDoc })
  );
}

const mapStateToProps = ({
  ui: {
    supports: { tabbingOrder },
  },
}) => ({
  supportsTabbingOrder: tabbingOrder,
});

// Exports from this module
exports.Toolbar = connect(mapStateToProps)(Toolbar);

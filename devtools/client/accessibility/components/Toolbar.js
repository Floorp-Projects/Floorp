/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { createFactory } = require("devtools/client/shared/vendor/react");
const {
  div,
  span,
} = require("devtools/client/shared/vendor/react-dom-factories");
const { L10N } = require("devtools/client/accessibility/utils/l10n");
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
const DisplayTabbingOrder = createFactory(
  require("devtools/client/accessibility/components/DisplayTabbingOrder")
);

const { connect } = require("devtools/client/shared/vendor/react-redux");

function Toolbar({ audit, simulate, supportsTabbingOrder, toolboxDoc }) {
  const betaID = "beta";
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
        span(
          {
            className: "beta",
            role: "presentation",
            id: betaID,
          },
          L10N.getStr("accessibility.beta")
        ),
        DisplayTabbingOrder({ describedby: betaID }),
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

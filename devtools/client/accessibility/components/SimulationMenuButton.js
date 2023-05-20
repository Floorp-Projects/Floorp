/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React
const {
  createFactory,
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const {
  hr,
  span,
  div,
} = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");
const {
  connect,
} = require("resource://devtools/client/shared/vendor/react-redux.js");
const MenuButton = createFactory(
  require("resource://devtools/client/shared/components/menu/MenuButton.js")
);
const { openDocLink } = require("resource://devtools/client/shared/link.js");
const {
  A11Y_SIMULATION_DOCUMENTATION_LINK,
} = require("resource://devtools/client/accessibility/constants.js");
const {
  accessibility: { SIMULATION_TYPE },
} = require("resource://devtools/shared/constants.js");
const actions = require("resource://devtools/client/accessibility/actions/simulation.js");

loader.lazyGetter(this, "MenuItem", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuItem.js")
  );
});
loader.lazyGetter(this, "MenuList", function () {
  return createFactory(
    require("resource://devtools/client/shared/components/menu/MenuList.js")
  );
});

const TELEMETRY_SIMULATION_ACTIVATED =
  "devtools.accessibility.simulation_activated";
const SIMULATION_MENU_LABELS = {
  NONE: "accessibility.filter.none",
  [SIMULATION_TYPE.ACHROMATOPSIA]: "accessibility.simulation.achromatopsia",
  [SIMULATION_TYPE.PROTANOPIA]: "accessibility.simulation.protanopia",
  [SIMULATION_TYPE.DEUTERANOPIA]: "accessibility.simulation.deuteranopia",
  [SIMULATION_TYPE.TRITANOPIA]: "accessibility.simulation.tritanopia",
  [SIMULATION_TYPE.CONTRAST_LOSS]: "accessibility.simulation.contrastLoss",
  DOCUMENTATION: "accessibility.documentation.label",
};

class SimulationMenuButton extends Component {
  static get propTypes() {
    return {
      simulate: PropTypes.func,
      simulation: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.disableSimulation = this.disableSimulation.bind(this);
  }

  disableSimulation() {
    const { dispatch, simulate: simulateFunc } = this.props;

    dispatch(actions.simulate(simulateFunc));
  }

  toggleSimulation(simKey) {
    const { dispatch, simulation, simulate: simulateFunc } = this.props;

    if (!simulation[simKey]) {
      if (gTelemetry) {
        gTelemetry.keyedScalarAdd(TELEMETRY_SIMULATION_ACTIVATED, simKey, 1);
      }

      dispatch(actions.simulate(simulateFunc, [simKey]));
      return;
    }

    this.disableSimulation();
  }

  render() {
    const { simulation, toolboxDoc } = this.props;
    const simulationMenuButtonId = "simulation-menu-button";
    const toolbarLabelID = "accessibility-simulation-label";
    const currSimulation = Object.entries(simulation).find(
      simEntry => simEntry[1]
    );

    const items = [
      // No simulation option
      MenuItem({
        key: "simulation-none",
        label: L10N.getStr(SIMULATION_MENU_LABELS.NONE),
        checked: !currSimulation,
        onClick: this.disableSimulation,
      }),
      hr({ key: "hr-1" }),
      // Simulation options
      ...Object.keys(SIMULATION_TYPE).map(simType =>
        MenuItem({
          key: `simulation-${simType}`,
          label: L10N.getStr(SIMULATION_MENU_LABELS[simType]),
          checked: simulation[simType],
          onClick: this.toggleSimulation.bind(this, simType),
        })
      ),
      hr({ key: "hr-2" }),
      // Documentation link
      MenuItem({
        className: "link",
        key: "simulation-documentation",
        label: L10N.getStr(SIMULATION_MENU_LABELS.DOCUMENTATION),
        role: "link",
        onClick: () => openDocLink(A11Y_SIMULATION_DOCUMENTATION_LINK),
      }),
    ];

    return div(
      {
        role: "group",
        className: "accessibility-simulation",
        "aria-labelledby": toolbarLabelID,
      },
      span(
        { id: toolbarLabelID, role: "presentation" },
        L10N.getStr("accessibility.simulation")
      ),
      MenuButton(
        {
          id: simulationMenuButtonId,
          menuId: simulationMenuButtonId + "-menu",
          className: `devtools-button toolbar-menu-button simulation${
            currSimulation ? " active" : ""
          }`,
          toolboxDoc,
          label: L10N.getStr(
            SIMULATION_MENU_LABELS[currSimulation ? currSimulation[0] : "NONE"]
          ),
        },
        MenuList({}, items)
      )
    );
  }
}

const mapStateToProps = ({ simulation }) => {
  return { simulation };
};

// Exports from this module
module.exports = connect(mapStateToProps)(SimulationMenuButton);

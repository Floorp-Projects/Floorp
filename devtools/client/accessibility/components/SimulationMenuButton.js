/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const {
  hr,
  span,
  div,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const MenuButton = createFactory(
  require("devtools/client/shared/components/menu/MenuButton")
);
const { openDocLink } = require("devtools/client/shared/link");
const {
  A11Y_SIMULATION_DOCUMENTATION_LINK,
} = require("devtools/client/accessibility/constants");
const {
  accessibility: { SIMULATION_TYPE },
} = require("devtools/shared/constants");
const actions = require("../actions/simulation");

loader.lazyGetter(this, "MenuItem", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuItem")
  );
});
loader.lazyGetter(this, "MenuList", function() {
  return createFactory(
    require("devtools/client/shared/components/menu/MenuList")
  );
});

const TELEMETRY_SIMULATION_ACTIVATED =
  "devtools.accessibility.simulation_activated";
const SIMULATION_MENU_LABELS = {
  NONE: "accessibility.filter.none",
  [SIMULATION_TYPE.DEUTERANOMALY]: "accessibility.simulation.deuteranomaly",
  [SIMULATION_TYPE.PROTANOMALY]: "accessibility.simulation.protanomaly",
  [SIMULATION_TYPE.PROTANOPIA]: "accessibility.simulation.protanopia",
  [SIMULATION_TYPE.DEUTERANOPIA]: "accessibility.simulation.deuteranopia",
  [SIMULATION_TYPE.TRITANOPIA]: "accessibility.simulation.tritanopia",
  [SIMULATION_TYPE.TRITANOMALY]: "accessibility.simulation.tritanomaly",
  [SIMULATION_TYPE.CONTRAST_LOSS]: "accessibility.simulation.contrastLoss",
  DOCUMENTATION: "accessibility.documentation.label",
};

class SimulationMenuButton extends Component {
  static get propTypes() {
    return {
      simulator: PropTypes.object.isRequired,
      simulation: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.disableSimulation = this.disableSimulation.bind(this);
  }

  disableSimulation() {
    const { dispatch, simulator } = this.props;

    dispatch(actions.simulate(simulator));
  }

  toggleSimulation(simKey) {
    const { dispatch, simulation, simulator } = this.props;

    if (!simulation[simKey]) {
      if (gTelemetry) {
        gTelemetry.keyedScalarAdd(TELEMETRY_SIMULATION_ACTIVATED, simKey, 1);
      }

      dispatch(actions.simulate(simulator, [simKey]));
      return;
    }

    this.disableSimulation();
  }

  render() {
    const { simulation } = this.props;
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
      hr(),
      // Simulation options
      ...Object.keys(SIMULATION_TYPE).map(simType =>
        MenuItem({
          key: `simulation-${simType}`,
          label: L10N.getStr(SIMULATION_MENU_LABELS[simType]),
          checked: simulation[simType],
          onClick: this.toggleSimulation.bind(this, simType),
        })
      ),
      hr(),
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
          doc: document,
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

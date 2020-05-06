/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("devtools/client/accessibility/utils/l10n");
const Button = createFactory(
  require("devtools/client/accessibility/components/Button").Button
);
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

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { disable } = require("devtools/client/accessibility/actions/ui");

class Toolbar extends Component {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      disableAccessibility: PropTypes.func.isRequired,
      canBeDisabled: PropTypes.bool.isRequired,
      toolboxDoc: PropTypes.object.isRequired,
      audit: PropTypes.func.isRequired,
      simulate: PropTypes.func,
      autoInit: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      disabling: false,
    };

    this.onDisable = this.onDisable.bind(this);
  }

  onDisable() {
    const { disableAccessibility, dispatch } = this.props;
    this.setState({ disabling: true });

    dispatch(disable(disableAccessibility))
      .then(() => this.setState({ disabling: false }))
      .catch(() => this.setState({ disabling: false }));
  }

  render() {
    const { canBeDisabled, simulate, toolboxDoc, audit, autoInit } = this.props;
    const { disabling } = this.state;
    const disableButtonStr = disabling
      ? "accessibility.disabling"
      : "accessibility.disable";
    let title;
    let isDisabled = false;

    if (canBeDisabled) {
      title = L10N.getStr("accessibility.disable.enabledTitle");
    } else {
      isDisabled = true;
      title = L10N.getStr("accessibility.disable.disabledTitle");
    }

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
      !autoInit &&
        Button(
          {
            className: "disable",
            id: "accessibility-disable-button",
            onClick: this.onDisable,
            disabled: disabling || isDisabled,
            busy: disabling,
            title,
          },
          L10N.getStr(disableButtonStr)
        ),
      !autoInit &&
        div({
          role: "separator",
          className: "devtools-separator",
        }),
      AccessibilityTreeFilter({ audit, toolboxDoc }),
      // Simulation section is shown if webrender is enabled
      ...optionalSimulationSection,
      AccessibilityPrefs({ toolboxDoc })
    );
  }
}

const mapStateToProps = ({
  ui: {
    canBeDisabled,
    supports: { autoInit },
  },
}) => ({
  canBeDisabled,
  autoInit,
});

// Exports from this module
module.exports = connect(mapStateToProps)(Toolbar);

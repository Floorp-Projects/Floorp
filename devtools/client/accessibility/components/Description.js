/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React & Redux
const {
  createFactory,
  Component,
} = require("devtools/client/shared/vendor/react");
const {
  div,
  p,
  img,
} = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Button = createFactory(
  require("devtools/client/accessibility/components/Button").Button
);
const LearnMoreLink = createFactory(
  require("devtools/client/accessibility/components/LearnMoreLink")
);
const { enable } = require("devtools/client/accessibility/actions/ui");

// Localization
const { L10N } = require("devtools/client/accessibility/utils/l10n");

const {
  A11Y_LEARN_MORE_LINK,
  A11Y_SERVICE_ENABLED_COUNT,
} = require("devtools/client/accessibility/constants");

/**
 * Landing UI for the accessibility panel when Accessibility features are
 * deactivated.
 */
class Description extends Component {
  static get propTypes() {
    return {
      canBeEnabled: PropTypes.bool,
      dispatch: PropTypes.func.isRequired,
      enableAccessibility: PropTypes.func.isRequired,
      autoInit: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      enabling: false,
    };

    this.onEnable = this.onEnable.bind(this);
  }

  onEnable() {
    const { enableAccessibility, dispatch } = this.props;
    this.setState({ enabling: true });

    if (gTelemetry) {
      gTelemetry.scalarAdd(A11Y_SERVICE_ENABLED_COUNT, 1);
    }

    dispatch(enable(enableAccessibility))
      .then(() => this.setState({ enabling: false }))
      .catch(() => this.setState({ enabling: false }));
  }

  render() {
    const { canBeEnabled, autoInit } = this.props;
    let warningStringName = "accessibility.enable.disabledTitle";
    let button;
    if (!autoInit) {
      const { enabling } = this.state;
      const enableButtonStr = enabling
        ? "accessibility.enabling"
        : "accessibility.enable";

      let title;
      let disableButton = false;

      if (canBeEnabled) {
        title = L10N.getStr("accessibility.enable.enabledTitle");
      } else {
        disableButton = true;
        title = L10N.getStr("accessibility.enable.disabledTitle");
      }

      button = Button(
        {
          id: "accessibility-enable-button",
          onClick: this.onEnable,
          disabled: enabling || disableButton,
          busy: enabling,
          "data-standalone": true,
          title,
        },
        L10N.getStr(enableButtonStr)
      );
      warningStringName = "accessibility.description.general.p2";
    }

    return div(
      { className: "description", role: "presentation" },
      div(
        { className: "general", role: "presentation" },
        img({
          src: "chrome://devtools/skin/images/accessibility.svg",
          alt: L10N.getStr("accessibility.logo"),
        }),
        div(
          { role: "presentation" },
          LearnMoreLink({
            href:
              A11Y_LEARN_MORE_LINK +
              "?utm_source=devtools&utm_medium=a11y-panel-description",
            learnMoreStringKey: "accessibility.learnMore",
            l10n: L10N,
            messageStringKey: "accessibility.description.general.p1",
          }),
          p({}, L10N.getStr(warningStringName))
        )
      ),
      button
    );
  }
}

const mapStateToProps = ({
  ui: {
    canBeEnabled,
    supports: { autoInit },
  },
}) => ({
  canBeEnabled,
  autoInit,
});

// Exports from this module
exports.Description = connect(mapStateToProps)(Description);

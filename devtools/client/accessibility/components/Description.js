/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* global gTelemetry */

// React & Redux
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { div, p, img } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Button = createFactory(require("./Button").Button);
const LearnMoreLink = createFactory(require("./LearnMoreLink"));
const { enable, updateCanBeEnabled } = require("../actions/ui");

// Localization
const { L10N } = require("../utils/l10n");

const { A11Y_LEARN_MORE_LINK, A11Y_SERVICE_ENABLED_COUNT } = require("../constants");

class OldVersionDescription extends Component {
  render() {
    return (
      div({ className: "description" },
        p({ className: "general" },
          img({
            src: "chrome://devtools/skin/images/accessibility.svg",
            alt: L10N.getStr("accessibility.logo"),
          }), L10N.getStr("accessibility.description.oldVersion")))
    );
  }
}

/**
 * Landing UI for the accessibility panel when Accessibility features are
 * deactivated.
 */
class Description extends Component {
  static get propTypes() {
    return {
      accessibility: PropTypes.object.isRequired,
      canBeEnabled: PropTypes.bool,
      dispatch: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      enabling: false,
    };

    this.onEnable = this.onEnable.bind(this);
    this.onCanBeEnabledChange = this.onCanBeEnabledChange.bind(this);
  }

  componentWillMount() {
    this.props.accessibility.on("can-be-enabled-change",
      this.onCanBeEnabledChange);
  }

  componentWillUnmount() {
    this.props.accessibility.off("can-be-enabled-change",
      this.onCanBeEnabledChange);
  }

  onEnable() {
    const { accessibility, dispatch } = this.props;
    this.setState({ enabling: true });

    if (gTelemetry) {
      gTelemetry.scalarAdd(A11Y_SERVICE_ENABLED_COUNT, 1);
    }

    dispatch(enable(accessibility))
      .then(() => this.setState({ enabling: false }))
      .catch(() => this.setState({ enabling: false }));
  }

  onCanBeEnabledChange(canBeEnabled) {
    this.props.dispatch(updateCanBeEnabled(canBeEnabled));
  }

  render() {
    const { canBeEnabled } = this.props;
    const { enabling } = this.state;
    const enableButtonStr = enabling ? "accessibility.enabling" : "accessibility.enable";

    let title;
    let disableButton = false;

    if (canBeEnabled) {
      title = L10N.getStr("accessibility.enable.enabledTitle");
    } else {
      disableButton = true;
      title = L10N.getStr("accessibility.enable.disabledTitle");
    }

    return (
      div({ className: "description", role: "presentation" },
        div({ className: "general", role: "presentation" },
          img({
            src: "chrome://devtools/skin/images/accessibility.svg",
            alt: L10N.getStr("accessibility.logo"),
          }),
          div({ role: "presentation" },
            LearnMoreLink({
              href: A11Y_LEARN_MORE_LINK +
                    "?utm_source=devtools&utm_medium=a11y-panel-description",
              learnMoreStringKey: "accessibility.learnMore",
              l10n: L10N,
              messageStringKey: "accessibility.description.general.p1",
            }),
            p({}, L10N.getStr("accessibility.description.general.p2"))
          )
        ),
        Button({
          id: "accessibility-enable-button",
          onClick: this.onEnable,
          disabled: enabling || disableButton,
          busy: enabling,
          "data-standalone": true,
          title,
        }, L10N.getStr(enableButtonStr))
      )
    );
  }
}

const mapStateToProps = ({ ui }) => ({
  canBeEnabled: ui.canBeEnabled,
});

// Exports from this module
exports.Description = connect(mapStateToProps)(Description);
exports.OldVersionDescription = OldVersionDescription;

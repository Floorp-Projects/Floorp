/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { createFactory, Component } = require("devtools/client/shared/vendor/react");
const { div, span } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../utils/l10n");
const Button = createFactory(require("./Button").Button);
const AccessibilityTreeFilter = createFactory(require("./AccessibilityTreeFilter"));

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { disable, updateCanBeDisabled } = require("../actions/ui");

const { A11Y_LEARN_MORE_LINK } = require("../constants");
const { openDocLink } = require("devtools/client/shared/link");

class Toolbar extends Component {
  static get propTypes() {
    return {
      walker: PropTypes.object.isRequired,
      dispatch: PropTypes.func.isRequired,
      accessibility: PropTypes.object.isRequired,
      canBeDisabled: PropTypes.bool.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      disabling: false,
    };

    this.onDisable = this.onDisable.bind(this);
    this.onCanBeDisabledChange = this.onCanBeDisabledChange.bind(this);
  }

  componentWillMount() {
    this.props.accessibility.on("can-be-disabled-change",
      this.onCanBeDisabledChange);
  }

  componentWillUnmount() {
    this.props.accessibility.off("can-be-disabled-change",
      this.onCanBeDisabledChange);
  }

  onCanBeDisabledChange(canBeDisabled) {
    this.props.dispatch(updateCanBeDisabled(canBeDisabled));
  }

  onDisable() {
    const { accessibility, dispatch } = this.props;
    this.setState({ disabling: true });

    dispatch(disable(accessibility))
      .then(() => this.setState({ disabling: false }))
      .catch(() => this.setState({ disabling: false }));
  }

  onLearnMoreClick() {
    openDocLink(A11Y_LEARN_MORE_LINK +
      "?utm_source=devtools&utm_medium=a11y-panel-toolbar");
  }

  render() {
    const { canBeDisabled, walker } = this.props;
    const { disabling } = this.state;
    const disableButtonStr = disabling ?
      "accessibility.disabling" : "accessibility.disable";
    const betaID = "beta";
    let title;
    let isDisabled = false;

    if (canBeDisabled) {
      title = L10N.getStr("accessibility.disable.enabledTitle");
    } else {
      isDisabled = true;
      title = L10N.getStr("accessibility.disable.disabledTitle");
    }

    return (
      div({
        className: "devtools-toolbar",
        role: "toolbar",
      }, Button({
        className: "disable",
        id: "accessibility-disable-button",
        onClick: this.onDisable,
        disabled: disabling || isDisabled,
        busy: disabling,
        title,
      }, L10N.getStr(disableButtonStr)),
        div({
          role: "separator",
          className: "devtools-separator",
        }),
        // @remove after release 68 (See Bug 1551574)
        span({
          className: "beta",
          role: "presentation",
          id: betaID,
        },
          L10N.getStr("accessibility.beta")),
        AccessibilityTreeFilter({ walker, describedby: betaID }),
        Button({
          className: "help",
          title: L10N.getStr("accessibility.learnMore"),
          onClick: this.onLearnMoreClick,
        }))
    );
  }
}

const mapStateToProps = ({ ui }) => ({
  canBeDisabled: ui.canBeDisabled,
});

// Exports from this module
module.exports = connect(mapStateToProps)(Toolbar);

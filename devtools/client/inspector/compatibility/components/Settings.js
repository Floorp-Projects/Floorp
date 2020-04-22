/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { connect } = require("devtools/client/shared/vendor/react-redux");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  updateSettingsVisibility,
} = require("devtools/client/inspector/compatibility/actions/compatibility");

const CLOSE_ICON = "chrome://devtools/skin/images/close.svg";

class Settings extends PureComponent {
  static get propTypes() {
    return {
      updateSettingsVisibility: PropTypes.func.isRequired,
    };
  }

  render() {
    return dom.section(
      {
        className: "compatibility-settings",
      },
      dom.header(
        {
          className: "compatibility-settings__header",
        },
        dom.label(
          {
            className: "compatibility-settings__header-label",
          },
          "Settings"
        ),
        dom.button(
          {
            className: "compatibility-settings__header-button",
            title: "Close settings",
            onClick: this.props.updateSettingsVisibility,
          },
          dom.img({
            className: "compatibility-settings__header-icon",
            src: CLOSE_ICON,
          })
        )
      )
    );
  }
}

const mapDispatchToProps = dispatch => {
  return {
    updateSettingsVisibility: () => dispatch(updateSettingsVisibility(false)),
  };
};

module.exports = connect(null, mapDispatchToProps)(Settings);

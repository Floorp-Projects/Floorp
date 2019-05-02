/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const FluentReact = require("devtools/client/shared/vendor/fluent-react");
const Localized = createFactory(FluentReact.Localized);

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Actions = require("../../actions/index");

class RefreshDevicesButton extends PureComponent {
  static get propTypes() {
    return {
      dispatch: PropTypes.func.isRequired,
      isScanning: PropTypes.bool.isRequired,
    };
  }

  refreshDevices() {
    this.props.dispatch(Actions.scanUSBRuntimes());
  }

  render() {
    return Localized(
      { id: "about-debugging-refresh-usb-devices-button" },
      dom.button(
        {
          className: "default-button qa-refresh-devices-button",
          disabled: this.props.isScanning,
          onClick: () => this.refreshDevices(),
        },
        "Refresh devices"
      )
    );
  }
}

module.exports = RefreshDevicesButton;

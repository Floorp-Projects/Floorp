/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getFormatStr } = require("../utils/l10n");
const Types = require("../types");

class Device extends PureComponent {
  static get propTypes() {
    return {
      // props.children are the buttons rendered as part of custom device label.
      children: PropTypes.oneOfType([
        PropTypes.arrayOf(PropTypes.node),
        PropTypes.node,
      ]),
      device: PropTypes.shape(Types.devices).isRequired,
      onDeviceCheckboxChange: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the device's input label is checked.
      isChecked: this.props.device.isChecked,
    };

    this.onCheckboxChanged = this.onCheckboxChanged.bind(this);
  }

  onCheckboxChanged(e) {
    this.setState(prevState => {
      return { isChecked: !prevState.isChecked };
    });

    this.props.onDeviceCheckboxChange(e);
  }

  render() {
    const { children, device } = this.props;
    const details = getFormatStr(
      "responsive.deviceDetails",
      device.width,
      device.height,
      device.pixelRatio,
      device.userAgent,
      device.touch
    );

    return dom.label(
      {
        className: "device-label",
        key: device.name,
        title: details,
      },
      dom.input({
        className: "device-input-checkbox",
        name: device.name,
        type: "checkbox",
        value: device.name,
        checked: device.isChecked,
        onChange: this.onCheckboxChanged,
      }),
      dom.span({ className: "device-name" }, device.name),
      children
    );
  }
}

module.exports = Device;

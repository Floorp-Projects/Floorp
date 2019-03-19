/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

const Device = createFactory(require("./Device"));

class DeviceList extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      onDeviceCheckboxChange: PropTypes.func.isRequired,
      onRemoveCustomDevice: PropTypes.func.isRequired,
      type: PropTypes.string.isRequired,
    };
  }

  renderCustomDevice(device) {
    const { onRemoveCustomDevice, onDeviceCheckboxChange, type } = this.props;

    // Show a remove button for custom devices.
    const removeDeviceButton = dom.button({
        id: "device-editor-remove",
        className: "device-remove-button devtools-button",
        onClick: () => onRemoveCustomDevice(device),
    });

    return Device(
      {
        device,
        key: device.name,
        type,
        onDeviceCheckboxChange,
      },
      removeDeviceButton
    );
  }

  render() {
    const { devices, type, onDeviceCheckboxChange } = this.props;

    return (
      dom.div({ className: "device-list"},
        devices[type].map(device => {
          if (type === "custom") {
            return this.renderCustomDevice(device);
          }

          return Device({
            device,
            key: device.name,
            type,
            onDeviceCheckboxChange,
          });
        })
      )
    );
  }
}

module.exports = DeviceList;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");
const { DOM: dom, createClass, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const OPEN_DEVICE_MODAL_VALUE = "OPEN_DEVICE_MODAL";

module.exports = createClass({
  displayName: "DeviceSelector",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    selectedDevice: PropTypes.string.isRequired,
    onChangeViewportDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  onSelectChange({ target }) {
    let {
      devices,
      onChangeViewportDevice,
      onResizeViewport,
      onUpdateDeviceModalOpen,
    } = this.props;

    if (target.value === OPEN_DEVICE_MODAL_VALUE) {
      onUpdateDeviceModalOpen(true);
      return;
    }

    for (let type of devices.types) {
      for (let device of devices[type]) {
        if (device.name === target.value) {
          onResizeViewport(device.width, device.height);
          break;
        }
      }
    }

    onChangeViewportDevice(target.value);
  },

  render() {
    let {
      devices,
      selectedDevice,
    } = this.props;

    let options = [];
    for (let type of devices.types) {
      for (let device of devices[type]) {
        if (device.displayed) {
          options.push(device);
        }
      }
    }

    options.sort(function (a, b) {
      return a.name.localeCompare(b.name);
    });

    let selectClass = "viewport-device-selector";
    if (selectedDevice) {
      selectClass += " selected";
    }

    return dom.select(
      {
        className: selectClass,
        value: selectedDevice,
        onChange: this.onSelectChange,
      },
      dom.option({
        value: "",
        disabled: true,
        hidden: true,
      }, getStr("responsive.noDeviceSelected")),
      options.map(device => {
        return dom.option({
          key: device.name,
          value: device.name,
        }, device.name);
      }),
      dom.option({
        value: OPEN_DEVICE_MODAL_VALUE,
      }, getStr("responsive.editDeviceList"))
    );
  },

});

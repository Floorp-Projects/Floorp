/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getStr } = require("../utils/l10n");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");
const OPEN_DEVICE_MODAL_VALUE = "OPEN_DEVICE_MODAL";

class DeviceSelector extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      selectedDevice: PropTypes.string.isRequired,
      viewportId: PropTypes.number.isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onSelectChange = this.onSelectChange.bind(this);
  }

  onSelectChange({ target }) {
    const {
      devices,
      viewportId,
      onChangeDevice,
      onResizeViewport,
      onUpdateDeviceModal,
    } = this.props;

    if (target.value === OPEN_DEVICE_MODAL_VALUE) {
      onUpdateDeviceModal(true, viewportId);
      return;
    }
    for (const type of devices.types) {
      for (const device of devices[type]) {
        if (device.name === target.value) {
          onResizeViewport(device.width, device.height);
          onChangeDevice(device, type);
          return;
        }
      }
    }
  }

  render() {
    const {
      devices,
      selectedDevice,
    } = this.props;

    const options = [];
    for (const type of devices.types) {
      for (const device of devices[type]) {
        if (device.displayed) {
          options.push(device);
        }
      }
    }

    options.sort(function(a, b) {
      return a.name.localeCompare(b.name);
    });

    let selectClass = "viewport-device-selector toolbar-dropdown";
    if (selectedDevice) {
      selectClass += " selected";
    }

    const state = devices.listState;
    let listContent;

    if (state == Types.loadableState.LOADED) {
      listContent = [
        dom.option({
          value: "",
          title: "",
          disabled: true,
          hidden: true,
        },
        getStr("responsive.noDeviceSelected")),
        options.map(device => {
          return dom.option({
            key: device.name,
            value: device.name,
            title: "",
          }, device.name);
        }),
        dom.option({
          value: OPEN_DEVICE_MODAL_VALUE,
          title: "",
        }, getStr("responsive.editDeviceList"))];
    } else if (state == Types.loadableState.LOADING
      || state == Types.loadableState.INITIALIZED) {
      listContent = [dom.option({
        value: "",
        title: "",
        disabled: true,
      }, getStr("responsive.deviceListLoading"))];
    } else if (state == Types.loadableState.ERROR) {
      listContent = [dom.option({
        value: "",
        title: "",
        disabled: true,
      }, getStr("responsive.deviceListError"))];
    }

    return dom.select(
      {
        className: selectClass,
        value: selectedDevice,
        title: selectedDevice,
        onChange: this.onSelectChange,
        disabled: (state !== Types.loadableState.LOADED),
      },
      ...listContent
    );
  }
}

module.exports = DeviceSelector;

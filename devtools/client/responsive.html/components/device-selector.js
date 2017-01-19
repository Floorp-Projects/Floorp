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
    onChangeDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  onSelectChange({ target }) {
    let {
      devices,
      onChangeDevice,
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
          onChangeDevice(device);
          return;
        }
      }
    }
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

    let state = devices.listState;
    let listContent;

    if (state == Types.deviceListState.LOADED) {
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
    } else if (state == Types.deviceListState.LOADING
      || state == Types.deviceListState.INITIALIZED) {
      listContent = [dom.option({
        value: "",
        title: "",
        disabled: true,
      }, getStr("responsive.deviceListLoading"))];
    } else if (state == Types.deviceListState.ERROR) {
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
        disabled: (state !== Types.deviceListState.LOADED),
      },
      ...listContent
    );
  },

});

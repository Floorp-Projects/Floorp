/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

loader.lazyRequireGetter(this, "showMenu", "devtools/client/shared/components/menu/utils", true);

class DeviceSelector extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      doResizeViewport: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
      selectedDevice: PropTypes.string.isRequired,
      viewportId: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowDeviceMenu = this.onShowDeviceMenu.bind(this);
  }

  onShowDeviceMenu(event) {
    const {
      devices,
      onChangeDevice,
      doResizeViewport,
      onUpdateDeviceModal,
      selectedDevice,
      viewportId,
    } = this.props;

    const menuItems = [];

    for (const type of devices.types) {
      for (const device of devices[type]) {
        if (device.displayed) {
          menuItems.push({
            label: device.name,
            type: "checkbox",
            checked: selectedDevice === device.name,
            click: () => {
              doResizeViewport(viewportId, device.width, device.height);
              onChangeDevice(viewportId, device, type);
            },
          });
        }
      }
    }

    menuItems.sort(function(a, b) {
      return a.label.localeCompare(b.label);
    });

    if (menuItems.length > 0) {
      menuItems.push("-");
    }

    menuItems.push({
      label: getStr("responsive.editDeviceList2"),
      click: () => onUpdateDeviceModal(true, viewportId),
    });

    showMenu(menuItems, {
      button: event.target,
    });
  }

  render() {
    const {
      devices,
      selectedDevice,
    } = this.props;

    return (
      dom.button(
        {
          id: "device-selector",
          className: "devtools-button devtools-dropdown-button",
          disabled: devices.listState !== Types.loadableState.LOADED,
          title: selectedDevice,
          onClick: this.onShowDeviceMenu,
        },
        dom.span({ className: "title" },
          selectedDevice || getStr("responsive.responsiveMode")
        )
      )
    );
  }
}

module.exports = DeviceSelector;

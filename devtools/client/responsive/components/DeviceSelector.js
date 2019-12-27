/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");
const { parseUserAgent } = require("../utils/ua");
const Types = require("../types");

const DeviceInfo = createFactory(require("./DeviceInfo"));

loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);

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
          const { browser, os } = parseUserAgent(device.userAgent);
          let label = device.name;
          if (os) {
            label += ` ${os.name}`;
            if (os.version) {
              label += ` ${os.version}`;
            }
          }
          const image = browser
            ? `chrome://devtools/skin/images/browsers/${browser.name.toLowerCase()}.svg`
            : " ";

          menuItems.push({
            label,
            image,
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

  createTitle(device) {
    if (!device) {
      return null;
    }

    const { browser, os } = parseUserAgent(device.userAgent);
    let title = device.name;
    if (os) {
      title += ` ${os.name}`;
      if (os.version) {
        title += ` ${os.version}`;
      }
    }
    if (browser) {
      title += ` ${browser.name} ${browser.version}`;
    }

    return title;
  }

  getSelectedDevice() {
    const { devices, selectedDevice } = this.props;

    if (!selectedDevice) {
      return null;
    }

    for (const type of devices.types) {
      for (const device of devices[type]) {
        if (selectedDevice === device.name) {
          return device;
        }
      }
    }

    return null;
  }

  render() {
    const { devices } = this.props;

    const device = this.getSelectedDevice();
    const title = this.createTitle(device);

    return dom.button(
      {
        id: "device-selector",
        className: "devtools-button devtools-dropdown-button",
        disabled: devices.listState !== Types.loadableState.LOADED,
        title,
        onClick: this.onShowDeviceMenu,
      },
      dom.span(
        { className: "title" },
        device ? DeviceInfo({ device }) : getStr("responsive.responsiveMode")
      )
    );
  }
}

module.exports = DeviceSelector;

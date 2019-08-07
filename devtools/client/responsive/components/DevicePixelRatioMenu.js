/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr, getFormatStr } = require("../utils/l10n");
const Types = require("../types");

loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);

const PIXEL_RATIO_PRESET = [1, 2, 3];

class DevicePixelRatioMenu extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      displayPixelRatio: PropTypes.number.isRequired,
      onChangePixelRatio: PropTypes.func.isRequired,
      selectedDevice: PropTypes.string.isRequired,
      selectedPixelRatio: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onShowDevicePixelMenu = this.onShowDevicePixelMenu.bind(this);
  }

  onShowDevicePixelMenu(event) {
    const {
      displayPixelRatio,
      onChangePixelRatio,
      selectedPixelRatio,
    } = this.props;

    const menuItems = PIXEL_RATIO_PRESET.map(value => {
      return {
        label: getFormatStr("responsive.devicePixelRatioOption", value),
        type: "checkbox",
        checked:
          selectedPixelRatio > 0
            ? selectedPixelRatio === value
            : displayPixelRatio === value,
        click: () => onChangePixelRatio(+value),
      };
    });

    showMenu(menuItems, {
      button: event.target,
    });
  }

  render() {
    const {
      devices,
      displayPixelRatio,
      selectedDevice,
      selectedPixelRatio,
    } = this.props;

    const isDisabled =
      devices.listState !== Types.loadableState.LOADED || selectedDevice !== "";

    let title;
    if (isDisabled) {
      title = getFormatStr("responsive.devicePixelRatio.auto", selectedDevice);
    } else {
      title = getStr("responsive.changeDevicePixelRatio");
    }

    return dom.button(
      {
        id: "device-pixel-ratio-menu",
        className: "devtools-button devtools-dropdown-button",
        disabled: isDisabled,
        title,
        onClick: this.onShowDevicePixelMenu,
      },
      dom.span(
        { className: "title" },
        getFormatStr(
          "responsive.devicePixelRatioOption",
          selectedPixelRatio || displayPixelRatio
        )
      )
    );
  }
}

module.exports = DevicePixelRatioMenu;

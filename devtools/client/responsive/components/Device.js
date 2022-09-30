/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  getFormatStr,
} = require("resource://devtools/client/responsive/utils/l10n.js");
const {
  parseUserAgent,
} = require("resource://devtools/client/responsive/utils/ua.js");
const Types = require("resource://devtools/client/responsive/types.js");

const DeviceInfo = createFactory(
  require("resource://devtools/client/responsive/components/DeviceInfo.js")
);

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

  getBrowserOrOsTooltip(agent) {
    if (agent) {
      return agent.name + (agent.version ? ` ${agent.version}` : "");
    }

    return null;
  }

  getTooltip() {
    const { device } = this.props;
    const { browser, os } = parseUserAgent(device.userAgent);

    const browserTitle = this.getBrowserOrOsTooltip(browser);
    const osTitle = this.getBrowserOrOsTooltip(os);
    let browserAndOsTitle = null;
    if (browserTitle && osTitle) {
      browserAndOsTitle = getFormatStr(
        "responsive.deviceDetails.browserAndOS",
        browserTitle,
        osTitle
      );
    } else if (browserTitle || osTitle) {
      browserAndOsTitle = browserTitle || osTitle;
    }

    const sizeTitle = getFormatStr(
      "responsive.deviceDetails.size",
      device.width,
      device.height
    );

    const dprTitle = getFormatStr(
      "responsive.deviceDetails.DPR",
      device.pixelRatio
    );

    const uaTitle = getFormatStr(
      "responsive.deviceDetails.UA",
      device.userAgent
    );

    const touchTitle = getFormatStr(
      "responsive.deviceDetails.touch",
      device.touch
    );

    return (
      `${browserAndOsTitle ? browserAndOsTitle + "\n" : ""}` +
      `${sizeTitle}\n` +
      `${dprTitle}\n` +
      `${uaTitle}\n` +
      `${touchTitle}\n`
    );
  }

  render() {
    const { children, device } = this.props;
    const tooltip = this.getTooltip();

    return dom.label(
      {
        className: "device-label",
        key: device.name,
        title: tooltip,
      },
      dom.input({
        className: "device-input-checkbox",
        name: device.name,
        type: "checkbox",
        value: device.name,
        checked: device.isChecked,
        onChange: this.onCheckboxChanged,
      }),
      DeviceInfo({ device }),
      children
    );
  }
}

module.exports = Device;

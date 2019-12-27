/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { parseUserAgent } = require("../utils/ua");
const Types = require("../types");

class DeviceInfo extends PureComponent {
  static get propTypes() {
    return {
      device: PropTypes.shape(Types.devices).isRequired,
    };
  }

  renderBrowser({ name }) {
    return dom.span({
      className: `device-browser ${name.toLowerCase()}`,
    });
  }

  renderOS({ name, version }) {
    const text = version ? `${name} ${version}` : name;
    return dom.span({ className: "device-os" }, text);
  }

  render() {
    const { device } = this.props;
    const { browser, os } = parseUserAgent(device.userAgent);

    return createElement(
      Fragment,
      null,
      browser ? this.renderBrowser(browser) : dom.span(),
      dom.span({ className: "device-name" }, device.name),
      os ? this.renderOS(os) : dom.span()
    );
  }
}

module.exports = DeviceInfo;

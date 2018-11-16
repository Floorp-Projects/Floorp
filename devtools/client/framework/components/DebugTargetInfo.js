/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * This is header that should be displayed on top of the toolbox when using
 * about:devtools-toolbox.
 */
class DebugTargetInfo extends PureComponent {
  static get propTypes() {
    return {
      deviceDescription: PropTypes.shape({
        deviceName: PropTypes.string,
        name: PropTypes.string.isRequired,
        version: PropTypes.string.isRequired,
      }).isRequired,
      L10N: PropTypes.object.isRequired,
      toolbox: PropTypes.object.isRequired,
    };
  }

  getTargetText() {
    const { L10N, toolbox } = this.props;
    const name = toolbox.target.name;
    const type = L10N.getStr("toolbox.debugTargetInfo.type.tab");
    return L10N.getFormatStr("toolbox.debugTargetInfo.targetLabel", name, type);
  }

  getRuntimeText() {
    const { deviceDescription, L10N } = this.props;
    const { name, deviceName, version } = deviceDescription;
    const localizationType =
      deviceName ? "toolbox.debugTargetInfo.runtimeLabelWithDeviceName"
                 : "toolbox.debugTargetInfo.runtimeLabel";
    return L10N.getFormatStr(localizationType, name, version, deviceName);
  }

  render() {
    const { deviceDescription } = this.props;
    const { channel } = deviceDescription;
    const icon =
      (channel === "release" || channel === "beta" || channel === "aurora")
        ? `chrome://devtools/skin/images/aboutdebugging-firefox-${ channel }.svg`
        : "chrome://devtools/skin/images/aboutdebugging-firefox-nightly.svg";

    return dom.header(
      {
        className: "debug-target-info",
      },
      dom.img({ src: icon }),
      dom.span({}, this.getRuntimeText()),
      dom.span({ className: "target" }, this.getTargetText()),
    );
  }
}

module.exports = DebugTargetInfo;

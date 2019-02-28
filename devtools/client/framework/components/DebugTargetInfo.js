/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { CONNECTION_TYPES } =
  require("devtools/client/shared/remote-debugging/remote-client-manager");

/**
 * This is header that should be displayed on top of the toolbox when using
 * about:devtools-toolbox.
 */
class DebugTargetInfo extends PureComponent {
  static get propTypes() {
    return {
      deviceDescription: PropTypes.shape({
        brandName: PropTypes.string.isRequired,
        channel: PropTypes.string.isRequired,
        connectionType: PropTypes.string.isRequired,
        deviceName: PropTypes.string,
        version: PropTypes.string.isRequired,
      }).isRequired,
      L10N: PropTypes.object.isRequired,
      toolbox: PropTypes.object.isRequired,
    };
  }

  getRuntimeText() {
    const { deviceDescription, L10N } = this.props;
    const { brandName, version, connectionType } = deviceDescription;

    return (connectionType === CONNECTION_TYPES.THIS_FIREFOX)
      ? L10N.getFormatStr("toolbox.debugTargetInfo.runtimeLabel.thisFirefox", version)
      : L10N.getFormatStr("toolbox.debugTargetInfo.runtimeLabel", brandName, version);
  }

  getAssetsForConnectionType() {
    const { connectionType } = this.props.deviceDescription;

    switch (connectionType) {
      case CONNECTION_TYPES.USB:
        return {
          image: "chrome://devtools/skin/images/aboutdebugging-usb-icon.svg",
          l10nId: "toolbox.debugTargetInfo.connection.usb",
        };
      case CONNECTION_TYPES.NETWORK:
        return {
          image: "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg",
          l10nId: "toolbox.debugTargetInfo.connection.network",
        };
    }
  }

  shallRenderConnection() {
    const { connectionType } = this.props.deviceDescription;
    const renderableTypes = [
      CONNECTION_TYPES.USB,
      CONNECTION_TYPES.NETWORK,
    ];

    return renderableTypes.includes(connectionType);
  }

  renderConnection() {
    const { connectionType } = this.props.deviceDescription;
    const { image, l10nId } = this.getAssetsForConnectionType();

    return dom.span(
      {
        className: "iconized-label",
      },
      dom.img({ src: image, alt: `${connectionType} icon`}),
      this.props.L10N.getStr(l10nId),
    );
  }

  renderRuntime() {
    const { channel, deviceName } = this.props.deviceDescription;

    const channelIcon =
      (channel === "release" || channel === "beta" || channel === "aurora") ?
      `chrome://devtools/skin/images/aboutdebugging-firefox-${ channel }.svg` :
      "chrome://devtools/skin/images/aboutdebugging-firefox-nightly.svg";

    return dom.span(
      {
        className: "iconized-label",
      },
      dom.img({ src: channelIcon, className: "channel-icon" }),
      dom.b({ className: "devtools-ellipsis-text" }, this.getRuntimeText()),
      dom.span({ className: "devtools-ellipsis-text" }, deviceName),
    );
  }

  renderTarget() {
    const title = this.props.toolbox.target.name;
    const url = this.props.toolbox.target.url;
    // TODO: https://bugzilla.mozilla.org/show_bug.cgi?id=1520723
    //       Show actual favicon (currently toolbox.target.activeTab.favicon
    //       is unpopulated)
    const favicon = "chrome://devtools/skin/images/aboutdebugging-globe-icon.svg";

    return dom.span(
      {
        className: "iconized-label",
      },
      dom.img({ src: favicon, alt: "favicon"}),
      title ? dom.b({ className: "devtools-ellipsis-text"}, title) : null,
      dom.span({ className: "devtools-ellipsis-text" }, url),
    );
  }

  render() {
    return dom.header(
      {
        className: "debug-target-info",
      },
      this.shallRenderConnection() ? this.renderConnection() : null,
      this.renderRuntime(),
      this.renderTarget(),
    );
  }
}

module.exports = DebugTargetInfo;

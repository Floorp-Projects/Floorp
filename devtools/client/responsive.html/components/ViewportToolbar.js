/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { getStr } = require("../utils/l10n");
const Types = require("../types");
const DeviceSelector = createFactory(require("./DeviceSelector"));

class ViewportToolbar extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      viewport: PropTypes.shape(Types.viewport).isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
      onRotateViewport: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      devices,
      viewport,
      onChangeDevice,
      onResizeViewport,
      onRotateViewport,
      onUpdateDeviceModal,
    } = this.props;

    return dom.div(
      {
        className: "viewport-toolbar container",
      },
      DeviceSelector({
        devices,
        selectedDevice: viewport.device,
        viewportId: viewport.id,
        onChangeDevice,
        onResizeViewport,
        onUpdateDeviceModal,
      }),
      dom.button({
        className: "viewport-rotate-button toolbar-button devtools-button",
        onClick: onRotateViewport,
        title: getStr("responsive.rotate"),
      })
    );
  }
}

module.exports = ViewportToolbar;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");

const { getStr } = require("../utils/l10n");
const Types = require("../types");
const DeviceSelector = createFactory(require("./device-selector"));

module.exports = createClass({
  displayName: "ViewportToolbar",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onChangeDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
    onUpdateDeviceModal: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
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
  },

});

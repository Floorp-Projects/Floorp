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
    selectedDevice: PropTypes.string.isRequired,
    onChangeDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  render() {
    let {
      devices,
      selectedDevice,
      onChangeDevice,
      onResizeViewport,
      onRotateViewport,
      onUpdateDeviceModalOpen,
    } = this.props;

    return dom.div(
      {
        className: "viewport-toolbar container",
      },
      DeviceSelector({
        devices,
        selectedDevice,
        onChangeDevice,
        onResizeViewport,
        onUpdateDeviceModalOpen,
      }),
      dom.button({
        className: "viewport-rotate-button toolbar-button devtools-button",
        onClick: onRotateViewport,
        title: getStr("responsive.rotate"),
      })
    );
  },

});

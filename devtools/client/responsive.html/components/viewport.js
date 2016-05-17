/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Types = require("../types");
const ResizableViewport = createFactory(require("./resizable-viewport"));
const ViewportDimension = createFactory(require("./viewport-dimension"));

module.exports = createClass({
  displayName: "Viewport",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    location: Types.location.isRequired,
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onBrowserMounted: PropTypes.func.isRequired,
    onChangeViewportDevice: PropTypes.func.isRequired,
    onContentResize: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
  },

  onChangeViewportDevice(device) {
    let {
      viewport,
      onChangeViewportDevice,
    } = this.props;

    onChangeViewportDevice(viewport.id, device);
  },

  onResizeViewport(width, height) {
    let {
      viewport,
      onResizeViewport,
    } = this.props;

    onResizeViewport(viewport.id, width, height);
  },

  onRotateViewport() {
    let {
      viewport,
      onRotateViewport,
    } = this.props;

    onRotateViewport(viewport.id);
  },

  render() {
    let {
      devices,
      location,
      screenshot,
      viewport,
      onBrowserMounted,
      onContentResize,
      onUpdateDeviceModalOpen,
    } = this.props;

    let {
      onChangeViewportDevice,
      onRotateViewport,
      onResizeViewport,
    } = this;

    return dom.div(
      {
        className: "viewport",
      },
      ResizableViewport({
        devices,
        location,
        screenshot,
        viewport,
        onBrowserMounted,
        onChangeViewportDevice,
        onContentResize,
        onResizeViewport,
        onRotateViewport,
        onUpdateDeviceModalOpen,
      }),
      ViewportDimension({
        viewport,
        onChangeViewportDevice,
        onResizeViewport,
      })
    );
  },

});

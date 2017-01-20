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
    swapAfterMount: PropTypes.bool.isRequired,
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onBrowserMounted: PropTypes.func.isRequired,
    onChangeDevice: PropTypes.func.isRequired,
    onContentResize: PropTypes.func.isRequired,
    onRemoveDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
    onRotateViewport: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
  },

  onChangeDevice(device) {
    let {
      viewport,
      onChangeDevice,
    } = this.props;

    onChangeDevice(viewport.id, device);
  },

  onRemoveDevice() {
    let {
      viewport,
      onRemoveDevice,
    } = this.props;

    onRemoveDevice(viewport.id);
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
      swapAfterMount,
      viewport,
      onBrowserMounted,
      onContentResize,
      onUpdateDeviceModalOpen,
    } = this.props;

    let {
      onChangeDevice,
      onRemoveDevice,
      onRotateViewport,
      onResizeViewport,
    } = this;

    return dom.div(
      {
        className: "viewport",
      },
      ViewportDimension({
        viewport,
        onRemoveDevice,
        onResizeViewport,
      }),
      ResizableViewport({
        devices,
        location,
        screenshot,
        swapAfterMount,
        viewport,
        onBrowserMounted,
        onChangeDevice,
        onContentResize,
        onRemoveDevice,
        onResizeViewport,
        onRotateViewport,
        onUpdateDeviceModalOpen,
      })
    );
  },

});

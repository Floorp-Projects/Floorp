/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const Types = require("../types");
const ResizableViewport = createFactory(require("./ResizableViewport"));
const ViewportDimension = createFactory(require("./ViewportDimension"));

class Viewport extends Component {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      swapAfterMount: PropTypes.bool.isRequired,
      viewport: PropTypes.shape(Types.viewport).isRequired,
      onBrowserMounted: PropTypes.func.isRequired,
      onChangeDevice: PropTypes.func.isRequired,
      onContentResize: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      onResizeViewport: PropTypes.func.isRequired,
      onRotateViewport: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onChangeDevice = this.onChangeDevice.bind(this);
    this.onRemoveDeviceAssociation = this.onRemoveDeviceAssociation.bind(this);
    this.onResizeViewport = this.onResizeViewport.bind(this);
    this.onRotateViewport = this.onRotateViewport.bind(this);
  }

  onChangeDevice(device, deviceType) {
    let {
      viewport,
      onChangeDevice,
    } = this.props;

    onChangeDevice(viewport.id, device, deviceType);
  }

  onRemoveDeviceAssociation() {
    let {
      viewport,
      onRemoveDeviceAssociation,
    } = this.props;

    onRemoveDeviceAssociation(viewport.id);
  }

  onResizeViewport(width, height) {
    let {
      viewport,
      onResizeViewport,
    } = this.props;

    onResizeViewport(viewport.id, width, height);
  }

  onRotateViewport() {
    let {
      viewport,
      onRotateViewport,
    } = this.props;

    onRotateViewport(viewport.id);
  }

  render() {
    let {
      devices,
      screenshot,
      swapAfterMount,
      viewport,
      onBrowserMounted,
      onContentResize,
      onUpdateDeviceModal,
    } = this.props;

    let {
      onChangeDevice,
      onRemoveDeviceAssociation,
      onRotateViewport,
      onResizeViewport,
    } = this;

    return dom.div(
      {
        className: "viewport",
      },
      ViewportDimension({
        viewport,
        onChangeSize: onResizeViewport,
        onRemoveDeviceAssociation,
      }),
      ResizableViewport({
        devices,
        screenshot,
        swapAfterMount,
        viewport,
        onBrowserMounted,
        onChangeDevice,
        onContentResize,
        onRemoveDeviceAssociation,
        onResizeViewport,
        onRotateViewport,
        onUpdateDeviceModal,
      })
    );
  }
}

module.exports = Viewport;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global window */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Constants = require("../constants");
const Types = require("../types");
const Browser = createFactory(require("./browser"));
const ViewportToolbar = createFactory(require("./viewport-toolbar"));

const VIEWPORT_MIN_WIDTH = Constants.MIN_VIEWPORT_DIMENSION;
const VIEWPORT_MIN_HEIGHT = Constants.MIN_VIEWPORT_DIMENSION;

module.exports = createClass({

  displayName: "ResizableViewport",

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

  getInitialState() {
    return {
      isResizing: false,
      lastClientX: 0,
      lastClientY: 0,
      ignoreX: false,
      ignoreY: false,
    };
  },

  onResizeStart({ target, clientX, clientY }) {
    window.addEventListener("mousemove", this.onResizeDrag, true);
    window.addEventListener("mouseup", this.onResizeStop, true);

    this.setState({
      isResizing: true,
      lastClientX: clientX,
      lastClientY: clientY,
      ignoreX: target === this.refs.resizeBarY,
      ignoreY: target === this.refs.resizeBarX,
    });
  },

  onResizeStop() {
    window.removeEventListener("mousemove", this.onResizeDrag, true);
    window.removeEventListener("mouseup", this.onResizeStop, true);

    this.setState({
      isResizing: false,
      lastClientX: 0,
      lastClientY: 0,
      ignoreX: false,
      ignoreY: false,
    });
  },

  onResizeDrag({ clientX, clientY }) {
    if (!this.state.isResizing) {
      return;
    }

    let { lastClientX, lastClientY, ignoreX, ignoreY } = this.state;
    // the viewport is centered horizontally, so horizontal resize resizes
    // by twice the distance the mouse was dragged - on left and right side.
    let deltaX = 2 * (clientX - lastClientX);
    let deltaY = (clientY - lastClientY);

    if (ignoreX) {
      deltaX = 0;
    }
    if (ignoreY) {
      deltaY = 0;
    }

    let width = this.props.viewport.width + deltaX;
    let height = this.props.viewport.height + deltaY;

    if (width < VIEWPORT_MIN_WIDTH) {
      width = VIEWPORT_MIN_WIDTH;
    } else {
      lastClientX = clientX;
    }

    if (height < VIEWPORT_MIN_HEIGHT) {
      height = VIEWPORT_MIN_HEIGHT;
    } else {
      lastClientY = clientY;
    }

    // Update the viewport store with the new width and height.
    this.props.onResizeViewport(width, height);
    // Change the device selector back to an unselected device
    // TODO: Bug 1332754: Logic like this probably belongs in the action creator.
    if (this.props.viewport.device) {
      // In bug 1329843 and others, we may eventually stop this approach of removing the
      // the properties of the device on resize.  However, at the moment, there is no
      // way to edit dPR when a device is selected, and there is no UI at all for editing
      // UA, so it's important to keep doing this for now.
      this.props.onRemoveDevice();
    }

    this.setState({
      lastClientX,
      lastClientY
    });
  },

  render() {
    let {
      devices,
      location,
      screenshot,
      swapAfterMount,
      viewport,
      onBrowserMounted,
      onChangeDevice,
      onContentResize,
      onResizeViewport,
      onRotateViewport,
      onUpdateDeviceModalOpen,
    } = this.props;

    let resizeHandleClass = "viewport-resize-handle";
    if (screenshot.isCapturing) {
      resizeHandleClass += " hidden";
    }

    let contentClass = "viewport-content";
    if (this.state.isResizing) {
      contentClass += " resizing";
    }

    return dom.div(
      {
        className: "resizable-viewport",
      },
      ViewportToolbar({
        devices,
        selectedDevice: viewport.device,
        onChangeDevice,
        onResizeViewport,
        onRotateViewport,
        onUpdateDeviceModalOpen,
      }),
      dom.div(
        {
          className: contentClass,
          style: {
            width: viewport.width + "px",
            height: viewport.height + "px",
          },
        },
        Browser({
          location,
          swapAfterMount,
          onBrowserMounted,
          onContentResize,
        })
      ),
      dom.div({
        className: resizeHandleClass,
        onMouseDown: this.onResizeStart,
      }),
      dom.div({
        ref: "resizeBarX",
        className: "viewport-horizontal-resize-handle",
        onMouseDown: this.onResizeStart,
      }),
      dom.div({
        ref: "resizeBarY",
        className: "viewport-vertical-resize-handle",
        onMouseDown: this.onResizeStart,
      })
    );
  },

});

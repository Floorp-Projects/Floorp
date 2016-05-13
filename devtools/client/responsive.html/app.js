/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /* eslint-env browser */

"use strict";

const { createClass, createFactory, PropTypes, DOM: dom } =
  require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  updateDeviceDisplayed,
  updateDeviceModalOpen,
} = require("./actions/devices");
const {
  changeDevice,
  resizeViewport,
  rotateViewport
} = require("./actions/viewports");
const { takeScreenshot } = require("./actions/screenshot");
const { updateTouchSimulationEnabled } = require("./actions/touch-simulation");
const DeviceModal = createFactory(require("./components/device-modal"));
const GlobalToolbar = createFactory(require("./components/global-toolbar"));
const Viewports = createFactory(require("./components/viewports"));
const { updateDeviceList } = require("./devices");
const Types = require("./types");

let App = createClass({
  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    location: Types.location.isRequired,
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
    touchSimulation: PropTypes.shape(Types.touchSimulation).isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
  },

  displayName: "App",

  onBrowserMounted() {
    window.postMessage({ type: "browser-mounted" }, "*");
  },

  onChangeViewportDevice(id, device) {
    this.props.dispatch(changeDevice(id, device));
  },

  onContentResize({ width, height }) {
    window.postMessage({
      type: "content-resize",
      width,
      height,
    }, "*");
  },

  onDeviceListUpdate(devices) {
    updateDeviceList(devices);
  },

  onExit() {
    window.postMessage({ type: "exit" }, "*");
  },

  onResizeViewport(id, width, height) {
    this.props.dispatch(resizeViewport(id, width, height));
  },

  onRotateViewport(id) {
    this.props.dispatch(rotateViewport(id));
  },

  onScreenshot() {
    this.props.dispatch(takeScreenshot());
  },

  onUpdateDeviceDisplayed(device, deviceType, displayed) {
    this.props.dispatch(updateDeviceDisplayed(device, deviceType, displayed));
  },

  onUpdateDeviceModalOpen(isOpen) {
    this.props.dispatch(updateDeviceModalOpen(isOpen));
  },

  onUpdateTouchSimulationEnabled() {
    let { enabled } = this.props.touchSimulation;

    window.postMessage({
      type: "update-touch-simulation",
      enabled,
    }, "*");

    this.props.dispatch(updateTouchSimulationEnabled(!enabled));
  },

  render() {
    let {
      devices,
      location,
      screenshot,
      touchSimulation,
      viewports,
    } = this.props;

    let {
      onBrowserMounted,
      onChangeViewportDevice,
      onContentResize,
      onDeviceListUpdate,
      onExit,
      onResizeViewport,
      onRotateViewport,
      onScreenshot,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModalOpen,
      onUpdateTouchSimulationEnabled,
    } = this;

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        screenshot,
        touchSimulation,
        onExit,
        onScreenshot,
        onUpdateTouchSimulationEnabled,
      }),
      Viewports({
        devices,
        location,
        screenshot,
        viewports,
        onBrowserMounted,
        onChangeViewportDevice,
        onContentResize,
        onRotateViewport,
        onResizeViewport,
        onUpdateDeviceModalOpen,
      }),
      DeviceModal({
        devices,
        onDeviceListUpdate,
        onUpdateDeviceDisplayed,
        onUpdateDeviceModalOpen,
      })
    );
  },

});

module.exports = connect(state => state)(App);

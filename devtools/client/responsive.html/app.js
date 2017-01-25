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
  updatePreferredDevices,
} = require("./actions/devices");
const { changeNetworkThrottling } = require("./actions/network-throttling");
const { takeScreenshot } = require("./actions/screenshot");
const { changeTouchSimulation } = require("./actions/touch-simulation");
const {
  changeDevice,
  changePixelRatio,
  removeDevice,
  resizeViewport,
  rotateViewport,
} = require("./actions/viewports");
const DeviceModal = createFactory(require("./components/device-modal"));
const GlobalToolbar = createFactory(require("./components/global-toolbar"));
const Viewports = createFactory(require("./components/viewports"));
const Types = require("./types");

let App = createClass({
  displayName: "App",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    dispatch: PropTypes.func.isRequired,
    displayPixelRatio: Types.pixelRatio.value.isRequired,
    location: Types.location.isRequired,
    networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
    screenshot: PropTypes.shape(Types.screenshot).isRequired,
    touchSimulation: PropTypes.shape(Types.touchSimulation).isRequired,
    viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
  },

  onBrowserMounted() {
    window.postMessage({ type: "browser-mounted" }, "*");
  },

  onChangeDevice(id, device) {
    window.postMessage({
      type: "change-device",
      device,
    }, "*");
    this.props.dispatch(changeDevice(id, device.name));
    this.props.dispatch(changeTouchSimulation(device.touch));
    this.props.dispatch(changePixelRatio(id, device.pixelRatio));
  },

  onChangeNetworkThrottling(enabled, profile) {
    window.postMessage({
      type: "change-network-throtting",
      enabled,
      profile,
    }, "*");
    this.props.dispatch(changeNetworkThrottling(enabled, profile));
  },

  onChangePixelRatio(pixelRatio) {
    window.postMessage({
      type: "change-pixel-ratio",
      pixelRatio,
    }, "*");
    this.props.dispatch(changePixelRatio(0, pixelRatio));
  },

  onChangeTouchSimulation(enabled) {
    window.postMessage({
      type: "change-touch-simulation",
      enabled,
    }, "*");
    this.props.dispatch(changeTouchSimulation(enabled));
  },

  onContentResize({ width, height }) {
    window.postMessage({
      type: "content-resize",
      width,
      height,
    }, "*");
  },

  onDeviceListUpdate(devices) {
    updatePreferredDevices(devices);
  },

  onExit() {
    window.postMessage({ type: "exit" }, "*");
  },

  onRemoveDevice(id) {
    // TODO: Bug 1332754: Move messaging and logic into the action creator.
    window.postMessage({
      type: "remove-device",
    }, "*");
    this.props.dispatch(removeDevice(id));
    this.props.dispatch(changeTouchSimulation(false));
    this.props.dispatch(changePixelRatio(id, 0));
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

  render() {
    let {
      devices,
      displayPixelRatio,
      location,
      networkThrottling,
      screenshot,
      touchSimulation,
      viewports,
    } = this.props;

    let {
      onBrowserMounted,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeTouchSimulation,
      onContentResize,
      onDeviceListUpdate,
      onExit,
      onRemoveDevice,
      onResizeViewport,
      onRotateViewport,
      onScreenshot,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModalOpen,
    } = this;

    let selectedDevice = "";
    let selectedPixelRatio = { value: 0 };

    if (viewports.length) {
      selectedDevice = viewports[0].device;
      selectedPixelRatio = viewports[0].pixelRatio;
    }

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        devices,
        displayPixelRatio,
        networkThrottling,
        screenshot,
        selectedDevice,
        selectedPixelRatio,
        touchSimulation,
        onChangeNetworkThrottling,
        onChangePixelRatio,
        onChangeTouchSimulation,
        onExit,
        onScreenshot,
      }),
      Viewports({
        devices,
        location,
        screenshot,
        viewports,
        onBrowserMounted,
        onChangeDevice,
        onContentResize,
        onRemoveDevice,
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

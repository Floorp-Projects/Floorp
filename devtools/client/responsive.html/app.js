/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const {
  addCustomDevice,
  removeCustomDevice,
  updateDeviceDisplayed,
  updateDeviceModal,
  updatePreferredDevices,
} = require("./actions/devices");
const { changeNetworkThrottling } = require("devtools/client/shared/components/throttling/actions");
const { changeReloadCondition } = require("./actions/reload-conditions");
const { takeScreenshot } = require("./actions/screenshot");
const { changeTouchSimulation } = require("./actions/touch-simulation");
const {
  changeDevice,
  changePixelRatio,
  removeDeviceAssociation,
  resizeViewport,
  rotateViewport,
} = require("./actions/viewports");
const DeviceModal = createFactory(require("./components/DeviceModal"));
const GlobalToolbar = createFactory(require("./components/GlobalToolbar"));
const Viewports = createFactory(require("./components/Viewports"));
const Types = require("./types");

class App extends Component {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      dispatch: PropTypes.func.isRequired,
      displayPixelRatio: Types.pixelRatio.value.isRequired,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      reloadConditions: PropTypes.shape(Types.reloadConditions).isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      touchSimulation: PropTypes.shape(Types.touchSimulation).isRequired,
      viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onAddCustomDevice = this.onAddCustomDevice.bind(this);
    this.onBrowserMounted = this.onBrowserMounted.bind(this);
    this.onChangeDevice = this.onChangeDevice.bind(this);
    this.onChangeNetworkThrottling = this.onChangeNetworkThrottling.bind(this);
    this.onChangePixelRatio = this.onChangePixelRatio.bind(this);
    this.onChangeReloadCondition = this.onChangeReloadCondition.bind(this);
    this.onChangeTouchSimulation = this.onChangeTouchSimulation.bind(this);
    this.onContentResize = this.onContentResize.bind(this);
    this.onDeviceListUpdate = this.onDeviceListUpdate.bind(this);
    this.onExit = this.onExit.bind(this);
    this.onRemoveCustomDevice = this.onRemoveCustomDevice.bind(this);
    this.onRemoveDeviceAssociation = this.onRemoveDeviceAssociation.bind(this);
    this.onResizeViewport = this.onResizeViewport.bind(this);
    this.onRotateViewport = this.onRotateViewport.bind(this);
    this.onScreenshot = this.onScreenshot.bind(this);
    this.onUpdateDeviceDisplayed = this.onUpdateDeviceDisplayed.bind(this);
    this.onUpdateDeviceModal = this.onUpdateDeviceModal.bind(this);
  }

  onAddCustomDevice(device) {
    this.props.dispatch(addCustomDevice(device));
  }

  onBrowserMounted() {
    window.postMessage({ type: "browser-mounted" }, "*");
  }

  onChangeDevice(id, device, deviceType) {
    // TODO: Bug 1332754: Move messaging and logic into the action creator so that the
    // message is sent from the action creator and device property changes are sent from
    // there instead of this function.
    window.postMessage({
      type: "change-device",
      device,
    }, "*");
    this.props.dispatch(changeDevice(id, device.name, deviceType));
    this.props.dispatch(changeTouchSimulation(device.touch));
    this.props.dispatch(changePixelRatio(id, device.pixelRatio));
  }

  onChangeNetworkThrottling(enabled, profile) {
    window.postMessage({
      type: "change-network-throttling",
      enabled,
      profile,
    }, "*");
    this.props.dispatch(changeNetworkThrottling(enabled, profile));
  }

  onChangePixelRatio(pixelRatio) {
    window.postMessage({
      type: "change-pixel-ratio",
      pixelRatio,
    }, "*");
    this.props.dispatch(changePixelRatio(0, pixelRatio));
  }

  onChangeReloadCondition(id, value) {
    this.props.dispatch(changeReloadCondition(id, value));
  }

  onChangeTouchSimulation(enabled) {
    window.postMessage({
      type: "change-touch-simulation",
      enabled,
    }, "*");
    this.props.dispatch(changeTouchSimulation(enabled));
  }

  onContentResize({ width, height }) {
    window.postMessage({
      type: "content-resize",
      width,
      height,
    }, "*");
  }

  onDeviceListUpdate(devices) {
    updatePreferredDevices(devices);
  }

  onExit() {
    window.postMessage({ type: "exit" }, "*");
  }

  onRemoveCustomDevice(device) {
    this.props.dispatch(removeCustomDevice(device));
  }

  onRemoveDeviceAssociation(id) {
    // TODO: Bug 1332754: Move messaging and logic into the action creator so that device
    // property changes are sent from there instead of this function.
    this.props.dispatch(removeDeviceAssociation(id));
    this.props.dispatch(changeTouchSimulation(false));
    this.props.dispatch(changePixelRatio(id, 0));
  }

  onResizeViewport(id, width, height) {
    this.props.dispatch(resizeViewport(id, width, height));
  }

  onRotateViewport(id) {
    this.props.dispatch(rotateViewport(id));
  }

  onScreenshot() {
    this.props.dispatch(takeScreenshot());
  }

  onUpdateDeviceDisplayed(device, deviceType, displayed) {
    this.props.dispatch(updateDeviceDisplayed(device, deviceType, displayed));
  }

  onUpdateDeviceModal(isOpen, modalOpenedFromViewport) {
    this.props.dispatch(updateDeviceModal(isOpen, modalOpenedFromViewport));
  }

  render() {
    const {
      devices,
      displayPixelRatio,
      networkThrottling,
      reloadConditions,
      screenshot,
      touchSimulation,
      viewports,
    } = this.props;

    const {
      onAddCustomDevice,
      onBrowserMounted,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeReloadCondition,
      onChangeTouchSimulation,
      onContentResize,
      onDeviceListUpdate,
      onExit,
      onRemoveCustomDevice,
      onRemoveDeviceAssociation,
      onResizeViewport,
      onRotateViewport,
      onScreenshot,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModal,
    } = this;

    let selectedDevice = "";
    let selectedPixelRatio = { value: 0 };

    if (viewports.length) {
      selectedDevice = viewports[0].device;
      selectedPixelRatio = viewports[0].pixelRatio;
    }

    let deviceAdderViewportTemplate = {};
    if (devices.modalOpenedFromViewport !== null) {
      deviceAdderViewportTemplate = viewports[devices.modalOpenedFromViewport];
    }

    return dom.div(
      {
        id: "app",
      },
      GlobalToolbar({
        devices,
        displayPixelRatio,
        networkThrottling,
        reloadConditions,
        screenshot,
        selectedDevice,
        selectedPixelRatio,
        touchSimulation,
        onChangeNetworkThrottling,
        onChangePixelRatio,
        onChangeReloadCondition,
        onChangeTouchSimulation,
        onExit,
        onScreenshot,
      }),
      Viewports({
        devices,
        screenshot,
        viewports,
        onBrowserMounted,
        onChangeDevice,
        onContentResize,
        onRemoveDeviceAssociation,
        onRotateViewport,
        onResizeViewport,
        onUpdateDeviceModal,
      }),
      DeviceModal({
        deviceAdderViewportTemplate,
        devices,
        onAddCustomDevice,
        onDeviceListUpdate,
        onRemoveCustomDevice,
        onUpdateDeviceDisplayed,
        onUpdateDeviceModal,
      })
    );
  }
}

module.exports = connect(state => state)(App);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Toolbar = createFactory(require("./Toolbar"));
const Viewports = createFactory(require("./Viewports"));

loader.lazyGetter(this, "DeviceModal",
  () => createFactory(require("./DeviceModal")));

const { changeNetworkThrottling } = require("devtools/client/shared/components/throttling/actions");
const {
  addCustomDevice,
  removeCustomDevice,
  updateDeviceDisplayed,
  updateDeviceModal,
  updatePreferredDevices,
} = require("../actions/devices");
const { takeScreenshot } = require("../actions/screenshot");
const {
  changeUserAgent,
  toggleLeftAlignment,
  toggleReloadOnTouchSimulation,
  toggleReloadOnUserAgent,
  toggleTouchSimulation,
  toggleUserAgentInput,
} = require("../actions/ui");
const {
  changeDevice,
  changePixelRatio,
  removeDeviceAssociation,
  resizeViewport,
  rotateViewport,
} = require("../actions/viewports");

const Types = require("../types");

class App extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      dispatch: PropTypes.func.isRequired,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
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
    this.onChangeTouchSimulation = this.onChangeTouchSimulation.bind(this);
    this.onChangeUserAgent = this.onChangeUserAgent.bind(this);
    this.onContentResize = this.onContentResize.bind(this);
    this.onDeviceListUpdate = this.onDeviceListUpdate.bind(this);
    this.onExit = this.onExit.bind(this);
    this.onRemoveCustomDevice = this.onRemoveCustomDevice.bind(this);
    this.onRemoveDeviceAssociation = this.onRemoveDeviceAssociation.bind(this);
    this.onResizeViewport = this.onResizeViewport.bind(this);
    this.onRotateViewport = this.onRotateViewport.bind(this);
    this.onScreenshot = this.onScreenshot.bind(this);
    this.onToggleLeftAlignment = this.onToggleLeftAlignment.bind(this);
    this.onToggleReloadOnTouchSimulation =
      this.onToggleReloadOnTouchSimulation.bind(this);
    this.onToggleReloadOnUserAgent = this.onToggleReloadOnUserAgent.bind(this);
    this.onToggleUserAgentInput = this.onToggleUserAgentInput.bind(this);
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
    this.props.dispatch(changePixelRatio(id, device.pixelRatio));
    this.props.dispatch(changeUserAgent(device.userAgent));
    this.props.dispatch(toggleTouchSimulation(device.touch));
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

  onChangeTouchSimulation(enabled) {
    window.postMessage({
      type: "change-touch-simulation",
      enabled,
    }, "*");
    this.props.dispatch(toggleTouchSimulation(enabled));
  }

  onChangeUserAgent(userAgent) {
    window.postMessage({
      type: "change-user-agent",
      userAgent,
    }, "*");
    this.props.dispatch(changeUserAgent(userAgent));
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
    // If the custom device is currently selected on any of the viewports,
    // remove the device association and reset all the ui state.
    for (const viewport of this.props.viewports) {
      if (viewport.device === device.name) {
        this.onRemoveDeviceAssociation(viewport.id);
      }
    }

    this.props.dispatch(removeCustomDevice(device));
  }

  onRemoveDeviceAssociation(id) {
    // TODO: Bug 1332754: Move messaging and logic into the action creator so that device
    // property changes are sent from there instead of this function.
    this.props.dispatch(removeDeviceAssociation(id));
    this.props.dispatch(toggleTouchSimulation(false));
    this.props.dispatch(changePixelRatio(id, 0));
    this.props.dispatch(changeUserAgent(""));
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

  onToggleLeftAlignment() {
    this.props.dispatch(toggleLeftAlignment());
  }

  onToggleReloadOnTouchSimulation() {
    this.props.dispatch(toggleReloadOnTouchSimulation());
  }

  onToggleReloadOnUserAgent() {
    this.props.dispatch(toggleReloadOnUserAgent());
  }

  onToggleUserAgentInput() {
    this.props.dispatch(toggleUserAgentInput());
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
      networkThrottling,
      screenshot,
      viewports,
    } = this.props;

    const {
      onAddCustomDevice,
      onBrowserMounted,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeTouchSimulation,
      onChangeUserAgent,
      onContentResize,
      onDeviceListUpdate,
      onExit,
      onRemoveCustomDevice,
      onRemoveDeviceAssociation,
      onResizeViewport,
      onRotateViewport,
      onScreenshot,
      onToggleLeftAlignment,
      onToggleReloadOnTouchSimulation,
      onToggleReloadOnUserAgent,
      onToggleUserAgentInput,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModal,
    } = this;

    if (!viewports.length) {
      return null;
    }

    const selectedDevice = viewports[0].device;
    const selectedPixelRatio = viewports[0].pixelRatio;

    let deviceAdderViewportTemplate = {};
    if (devices.modalOpenedFromViewport !== null) {
      deviceAdderViewportTemplate = viewports[devices.modalOpenedFromViewport];
    }

    return (
      dom.div({ id: "app" },
        Toolbar({
          devices,
          networkThrottling,
          screenshot,
          selectedDevice,
          selectedPixelRatio,
          viewport: viewports[0],
          onChangeDevice,
          onChangeNetworkThrottling,
          onChangePixelRatio,
          onChangeTouchSimulation,
          onChangeUserAgent,
          onExit,
          onRemoveDeviceAssociation,
          onResizeViewport,
          onRotateViewport,
          onScreenshot,
          onToggleLeftAlignment,
          onToggleReloadOnTouchSimulation,
          onToggleReloadOnUserAgent,
          onToggleUserAgentInput,
          onUpdateDeviceModal,
        }),
        Viewports({
          screenshot,
          viewports,
          onBrowserMounted,
          onContentResize,
          onRemoveDeviceAssociation,
          onResizeViewport,
        }),
        devices.isModalOpen ?
          DeviceModal({
            deviceAdderViewportTemplate,
            devices,
            onAddCustomDevice,
            onDeviceListUpdate,
            onRemoveCustomDevice,
            onUpdateDeviceDisplayed,
            onUpdateDeviceModal,
          })
          :
          null
      )
    );
  }
}

module.exports = connect(state => state)(App);

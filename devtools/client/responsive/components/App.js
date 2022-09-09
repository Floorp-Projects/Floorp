/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const Toolbar = createFactory(
  require("devtools/client/responsive/components/Toolbar")
);

loader.lazyGetter(this, "DeviceModal", () =>
  createFactory(require("devtools/client/responsive/components/DeviceModal"))
);

const {
  changeNetworkThrottling,
} = require("devtools/client/shared/components/throttling/actions");
const {
  addCustomDevice,
  editCustomDevice,
  removeCustomDevice,
  updateDeviceDisplayed,
  updateDeviceModal,
  updatePreferredDevices,
} = require("devtools/client/responsive/actions/devices");
const {
  takeScreenshot,
} = require("devtools/client/responsive/actions/screenshot");
const {
  changeUserAgent,
  toggleLeftAlignment,
  toggleReloadOnTouchSimulation,
  toggleReloadOnUserAgent,
  toggleTouchSimulation,
  toggleUserAgentInput,
} = require("devtools/client/responsive/actions/ui");
const {
  changeDevice,
  changePixelRatio,
  changeViewportAngle,
  removeDeviceAssociation,
  resizeViewport,
  rotateViewport,
} = require("devtools/client/responsive/actions/viewports");
const {
  getOrientation,
} = require("devtools/client/responsive/utils/orientation");

const Types = require("devtools/client/responsive/types");

class App extends PureComponent {
  static get propTypes() {
    return {
      devices: PropTypes.shape(Types.devices).isRequired,
      dispatch: PropTypes.func.isRequired,
      leftAlignmentEnabled: PropTypes.bool.isRequired,
      networkThrottling: PropTypes.shape(Types.networkThrottling).isRequired,
      screenshot: PropTypes.shape(Types.screenshot).isRequired,
      viewports: PropTypes.arrayOf(PropTypes.shape(Types.viewport)).isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onAddCustomDevice = this.onAddCustomDevice.bind(this);
    this.onBrowserContextMenu = this.onBrowserContextMenu.bind(this);
    this.onChangeDevice = this.onChangeDevice.bind(this);
    this.onChangeNetworkThrottling = this.onChangeNetworkThrottling.bind(this);
    this.onChangePixelRatio = this.onChangePixelRatio.bind(this);
    this.onChangeTouchSimulation = this.onChangeTouchSimulation.bind(this);
    this.onChangeUserAgent = this.onChangeUserAgent.bind(this);
    this.onChangeViewportOrientation = this.onChangeViewportOrientation.bind(
      this
    );
    this.onDeviceListUpdate = this.onDeviceListUpdate.bind(this);
    this.onEditCustomDevice = this.onEditCustomDevice.bind(this);
    this.onExit = this.onExit.bind(this);
    this.onRemoveCustomDevice = this.onRemoveCustomDevice.bind(this);
    this.onRemoveDeviceAssociation = this.onRemoveDeviceAssociation.bind(this);
    this.doResizeViewport = this.doResizeViewport.bind(this);
    this.onRotateViewport = this.onRotateViewport.bind(this);
    this.onScreenshot = this.onScreenshot.bind(this);
    this.onToggleLeftAlignment = this.onToggleLeftAlignment.bind(this);
    this.onToggleReloadOnTouchSimulation = this.onToggleReloadOnTouchSimulation.bind(
      this
    );
    this.onToggleReloadOnUserAgent = this.onToggleReloadOnUserAgent.bind(this);
    this.onToggleUserAgentInput = this.onToggleUserAgentInput.bind(this);
    this.onUpdateDeviceDisplayed = this.onUpdateDeviceDisplayed.bind(this);
    this.onUpdateDeviceModal = this.onUpdateDeviceModal.bind(this);
  }

  componentWillUnmount() {
    this.browser.removeEventListener("contextmenu", this.onContextMenu);
    this.browser = null;
  }

  onAddCustomDevice(device) {
    this.props.dispatch(addCustomDevice(device));
  }

  onBrowserContextMenu() {
    // Update the position of remote browser so that makes the context menu to show at
    // proper position before showing.
    this.browser.frameLoader.requestUpdatePosition();
  }

  onChangeDevice(id, device, deviceType) {
    // Resize the viewport first.
    this.doResizeViewport(id, device.width, device.height);

    // TODO: Bug 1332754: Move messaging and logic into the action creator so that the
    // message is sent from the action creator and device property changes are sent from
    // there instead of this function.
    window.postMessage(
      {
        type: "change-device",
        device,
        viewport: device,
      },
      "*"
    );

    const orientation = getOrientation(device, device);

    this.props.dispatch(changeViewportAngle(0, orientation.angle));
    this.props.dispatch(changeDevice(id, device.name, deviceType));
    this.props.dispatch(changePixelRatio(id, device.pixelRatio));
    this.props.dispatch(changeUserAgent(device.userAgent));
    this.props.dispatch(toggleTouchSimulation(device.touch));
  }

  onChangeNetworkThrottling(enabled, profile) {
    window.postMessage(
      {
        type: "change-network-throttling",
        enabled,
        profile,
      },
      "*"
    );
    this.props.dispatch(changeNetworkThrottling(enabled, profile));
  }

  onChangePixelRatio(pixelRatio) {
    window.postMessage(
      {
        type: "change-pixel-ratio",
        pixelRatio,
      },
      "*"
    );
    this.props.dispatch(changePixelRatio(0, pixelRatio));
  }

  onChangeTouchSimulation(enabled) {
    window.postMessage(
      {
        type: "change-touch-simulation",
        enabled,
      },
      "*"
    );
    this.props.dispatch(toggleTouchSimulation(enabled));
  }

  onChangeUserAgent(userAgent) {
    window.postMessage(
      {
        type: "change-user-agent",
        userAgent,
      },
      "*"
    );
    this.props.dispatch(changeUserAgent(userAgent));
  }

  onChangeViewportOrientation(id, type, angle, isViewportRotated = false) {
    window.postMessage(
      {
        type: "viewport-orientation-change",
        orientationType: type,
        angle,
        isViewportRotated,
      },
      "*"
    );

    if (isViewportRotated) {
      this.props.dispatch(changeViewportAngle(id, angle));
    }
  }

  onDeviceListUpdate(devices) {
    updatePreferredDevices(devices);
  }

  onEditCustomDevice(oldDevice, newDevice) {
    // If the edited device is currently selected, then update its original association
    // and reset UI state.
    let viewport = this.props.viewports.find(
      ({ device }) => device === oldDevice.name
    );

    if (viewport) {
      viewport = {
        ...viewport,
        device: newDevice.name,
        deviceType: "custom",
        height: newDevice.height,
        width: newDevice.width,
        pixelRatio: newDevice.pixelRatio,
        touch: newDevice.touch,
        userAgent: newDevice.userAgent,
      };
    }

    this.props.dispatch(editCustomDevice(viewport, oldDevice, newDevice));
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

  doResizeViewport(id, width, height) {
    // This is the setter function that we pass to Toolbar and Viewports
    // so they can modify the viewport.
    window.postMessage(
      {
        type: "viewport-resize",
        width,
        height,
      },
      "*"
    );
    this.props.dispatch(resizeViewport(id, width, height));
  }

  /**
   * Dispatches the rotateViewport action creator. This utilized by the RDM toolbar as
   * a prop.
   *
   * @param {Number} id
   *        The viewport ID.
   */
  onRotateViewport(id) {
    let currentDevice;
    const viewport = this.props.viewports[id];

    for (const type of this.props.devices.types) {
      for (const device of this.props.devices[type]) {
        if (viewport.device === device.name) {
          currentDevice = device;
        }
      }
    }

    // If no device is selected, then assume the selected device's primary orientation is
    // opposite of the viewport orientation.
    if (!currentDevice) {
      currentDevice = {
        height: viewport.width,
        width: viewport.height,
      };
    }

    const currentAngle = Services.prefs.getIntPref(
      "devtools.responsive.viewport.angle"
    );
    const angleToRotateTo = currentAngle === 90 ? 0 : 90;
    const { type, angle } = getOrientation(
      currentDevice,
      viewport,
      angleToRotateTo
    );

    this.onChangeViewportOrientation(id, type, angle, true);
    this.props.dispatch(rotateViewport(id));

    window.postMessage(
      {
        type: "viewport-resize",
        height: viewport.width,
        width: viewport.height,
      },
      "*"
    );
  }

  onScreenshot() {
    this.props.dispatch(takeScreenshot());
  }

  onToggleLeftAlignment() {
    this.props.dispatch(toggleLeftAlignment());

    window.postMessage(
      {
        type: "toggle-left-alignment",
        leftAlignmentEnabled: this.props.leftAlignmentEnabled,
      },
      "*"
    );
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
    window.postMessage({ type: "update-device-modal", isOpen }, "*");
  }

  render() {
    const { devices, networkThrottling, screenshot, viewports } = this.props;

    const {
      onAddCustomDevice,
      onChangeDevice,
      onChangeNetworkThrottling,
      onChangePixelRatio,
      onChangeTouchSimulation,
      onChangeUserAgent,
      onDeviceListUpdate,
      onEditCustomDevice,
      onExit,
      onRemoveCustomDevice,
      onRemoveDeviceAssociation,
      doResizeViewport,
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

    return dom.div(
      { id: "app" },
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
        doResizeViewport,
        onRotateViewport,
        onScreenshot,
        onToggleLeftAlignment,
        onToggleReloadOnTouchSimulation,
        onToggleReloadOnUserAgent,
        onToggleUserAgentInput,
        onUpdateDeviceModal,
      }),
      devices.isModalOpen
        ? DeviceModal({
            deviceAdderViewportTemplate,
            devices,
            onAddCustomDevice,
            onDeviceListUpdate,
            onEditCustomDevice,
            onRemoveCustomDevice,
            onUpdateDeviceDisplayed,
            onUpdateDeviceModal,
          })
        : null
    );
  }
}

const mapStateToProps = state => {
  return {
    ...state,
    leftAlignmentEnabled: state.ui.leftAlignmentEnabled,
  };
};

module.exports = connect(mapStateToProps)(App);

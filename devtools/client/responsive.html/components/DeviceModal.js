/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const DeviceForm = createFactory(require("./DeviceForm"));
const DeviceList = createFactory(require("./DeviceList"));

const { getFormatStr, getStr } = require("../utils/l10n");
const Types = require("../types");

class DeviceModal extends PureComponent {
  static get propTypes() {
    return {
      deviceAdderViewportTemplate: PropTypes.shape(Types.viewport).isRequired,
      devices: PropTypes.shape(Types.devices).isRequired,
      onAddCustomDevice: PropTypes.func.isRequired,
      onDeviceListUpdate: PropTypes.func.isRequired,
      onRemoveCustomDevice: PropTypes.func.isRequired,
      onUpdateDeviceDisplayed: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isDeviceFormShown: false,
    };
    for (const type of this.props.devices.types) {
      for (const device of this.props.devices[type]) {
        this.state[device.name] = device.displayed;
      }
    }

    this.onAddCustomDevice = this.onAddCustomDevice.bind(this);
    this.onDeviceCheckboxChange = this.onDeviceCheckboxChange.bind(this);
    this.onDeviceFormShow = this.onDeviceFormShow.bind(this);
    this.onDeviceFormHide = this.onDeviceFormHide.bind(this);
    this.onDeviceModalSubmit = this.onDeviceModalSubmit.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.validateAddDeviceFormNameField = this.validateAddDeviceFormNameField.bind(this);
  }

  componentDidMount() {
    window.addEventListener("keydown", this.onKeyDown, true);
  }

  componentWillUnmount() {
    window.removeEventListener("keydown", this.onKeyDown, true);
  }

  onAddCustomDevice(device) {
    this.props.onAddCustomDevice(device);
    // Default custom devices to enabled
    this.setState({
      [device.name]: true,
    });
  }

  onDeviceCheckboxChange({ nativeEvent: { button }, target }) {
    if (button !== 0) {
      return;
    }
    this.setState({
      [target.value]: !this.state[target.value],
    });
  }

  onDeviceFormShow() {
    this.setState({ isDeviceFormShown: true });
  }

  onDeviceFormHide() {
    this.setState({ isDeviceFormShown: false });
  }

  onDeviceModalSubmit() {
    const {
      devices,
      onDeviceListUpdate,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModal,
    } = this.props;

    const preferredDevices = {
      "added": new Set(),
      "removed": new Set(),
    };

    for (const type of devices.types) {
      for (const device of devices[type]) {
        const newState = this.state[device.name];

        if (device.featured && !newState) {
          preferredDevices.removed.add(device.name);
        } else if (!device.featured && newState) {
          preferredDevices.added.add(device.name);
        }

        if (this.state[device.name] != device.displayed) {
          onUpdateDeviceDisplayed(device, type, this.state[device.name]);
        }
      }
    }

    onDeviceListUpdate(preferredDevices);
    onUpdateDeviceModal(false);
  }

  onKeyDown(event) {
    if (!this.props.devices.isModalOpen) {
      return;
    }
    // Escape keycode
    if (event.keyCode === 27) {
      const {
        onUpdateDeviceModal,
      } = this.props;
      onUpdateDeviceModal(false);
    }
  }

  renderAddDeviceForm(devices, viewportTemplate) {
    // If a device is currently selected, fold its attributes into a single object for use
    // as the starting values of the form.  If no device is selected, use the values for
    // the current window.
    const deviceTemplate = viewportTemplate;
    if (viewportTemplate.device) {
      const device = devices[viewportTemplate.deviceType].find(d => {
        return d.name == viewportTemplate.device;
      });
      Object.assign(deviceTemplate, {
        pixelRatio: device.pixelRatio,
        userAgent: device.userAgent,
        touch: device.touch,
        name: getFormatStr("responsive.customDeviceNameFromBase", device.name),
      });
    } else {
      Object.assign(deviceTemplate, {
        pixelRatio: window.devicePixelRatio,
        userAgent: navigator.userAgent,
        touch: false,
        name: getStr("responsive.customDeviceName"),
      });
    }

    return (
      DeviceForm({
        formType: "add",
        buttonText: getStr("responsive.addDevice2"),
        device: deviceTemplate,
        onDeviceFormHide: this.onDeviceFormHide,
        onDeviceFormShow: this.onDeviceFormShow,
        onSave: this.onAddCustomDevice,
        validateName: this.validateAddDeviceFormNameField,
        viewportTemplate,
      })
    );
  }

  renderDevices() {
    const sortedDevices = {};
    for (const type of this.props.devices.types) {
      sortedDevices[type] = this.props.devices[type]
        .sort((a, b) => a.name.localeCompare(b.name));

      sortedDevices[type].forEach(device => {
        device.isChecked = this.state[device.name];
      });
    }

    return (
      this.props.devices.types.map(type => {
        return sortedDevices[type].length ?
          dom.div(
            {
              className: `device-type device-type-${type}`,
              key: type,
            },
            dom.header({ className: "device-header" }, type),
            DeviceList({
              devices: sortedDevices,
              type,
              onDeviceCheckboxChange: this.onDeviceCheckboxChange,
              onRemoveCustomDevice: this.props.onRemoveCustomDevice,
            })
          )
          :
          null;
      })
    );
  }

  /**
   * Validates the name field's value by checking if the added device's name already
   * exists in the custom devices list.
   *
   * @param  {String} value
   *         The input field value for the device name.
   * @return {Boolean} true if device name is valid, false otherwise.
   */
  validateAddDeviceFormNameField(value) {
    const { devices } = this.props;
    const nameFieldValue = value.trim();
    const deviceFound = devices.custom.find(device => device.name == nameFieldValue);

    return !deviceFound;
  }

  render() {
    const {
      deviceAdderViewportTemplate,
      devices,
      onUpdateDeviceModal,
    } = this.props;

    return (
      dom.div(
        {
          id: "device-modal-wrapper",
          className: this.props.devices.isModalOpen ? "opened" : "closed",
        },
        dom.div({ className: "device-modal" },
          dom.div({ className: "device-modal-header" },
            !this.state.isDeviceFormShown ?
              dom.header({ className: "device-modal-title" },
                getStr("responsive.deviceSettings"),
                dom.button({
                  id: "device-close-button",
                  className: "devtools-button",
                  onClick: () => onUpdateDeviceModal(false),
                })
              )
              :
              null,
            this.renderAddDeviceForm(devices, deviceAdderViewportTemplate)
          ),
          dom.div({
            className: `device-modal-content
              ${this.state.isDeviceFormShown ? " form-shown" : ""}`,
          },
          this.renderDevices()
          ),
          dom.button(
            {
              id: "device-submit-button",
              onClick: this.onDeviceModalSubmit,
            },
            getStr("responsive.done")
          )
        ),
        dom.div(
          {
            className: "modal-overlay",
            onClick: () => onUpdateDeviceModal(false),
          }
        )
      )
    );
  }
}

module.exports = DeviceModal;

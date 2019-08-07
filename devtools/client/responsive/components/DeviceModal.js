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
      onEditCustomDevice: PropTypes.func.isRequired,
      onRemoveCustomDevice: PropTypes.func.isRequired,
      onUpdateDeviceDisplayed: PropTypes.func.isRequired,
      onUpdateDeviceModal: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // The device form type can be 3 states: "add", "edit", or "".
      // "add"  - The form shown is adding a new device.
      // "edit" - The form shown is editing an existing custom device.
      // ""     - The form is closed.
      deviceFormType: "",
      // The device being edited from the edit form.
      editingDevice: null,
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
    this.onEditCustomDevice = this.onEditCustomDevice.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
  }

  componentDidMount() {
    window.addEventListener("keydown", this.onKeyDown, true);
  }

  componentWillUnmount() {
    this.onDeviceModalSubmit();
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

  onDeviceFormShow(type, device) {
    this.setState({
      deviceFormType: type,
      editingDevice: device,
    });
  }

  onDeviceFormHide() {
    this.setState({
      deviceFormType: "",
      editingDevice: null,
    });
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

  onEditCustomDevice(newDevice) {
    this.props.onEditCustomDevice(this.state.editingDevice, newDevice);

    // We want to remove the original device name from state after editing, so create a
    // new state setting the old key to null and the new one to true.
    this.setState({
      [this.state.editingDevice.name]: null,
      [newDevice.name]: true,
    });
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

  renderAddForm() {
    // If a device is currently selected, fold its attributes into a single object for use
    // as the starting values of the form.  If no device is selected, use the values for
    // the current window.
    const { deviceAdderViewportTemplate: viewportTemplate } = this.props;
    const deviceTemplate = this.props.deviceAdderViewportTemplate;
    if (viewportTemplate.device) {
      const device = this.props.devices[viewportTemplate.deviceType].find(d => {
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
        device: deviceTemplate,
        devices: this.props.devices,
        onDeviceFormHide: this.onDeviceFormHide,
        onSave: this.onAddCustomDevice,
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
              isDeviceFormShown: this.state.deviceFormType,
              type,
              onDeviceCheckboxChange: this.onDeviceCheckboxChange,
              onDeviceFormHide: this.onDeviceFormHide,
              onDeviceFormShow: this.onDeviceFormShow,
              onEditCustomDevice: this.onEditCustomDevice,
              onRemoveCustomDevice: this.props.onRemoveCustomDevice,
            })
          )
          :
          null;
      })
    );
  }

  renderEditForm() {
    return (
      DeviceForm({
        formType: "edit",
        device: this.state.editingDevice,
        devices: this.props.devices,
        onDeviceFormHide: this.onDeviceFormHide,
        onSave: this.onEditCustomDevice,
        viewportTemplate: {
          width: this.state.editingDevice.width,
          height: this.state.editingDevice.height,
        },
      })
    );
  }

  renderForm() {
    let form = null;

    if (this.state.deviceFormType === "add") {
      form = this.renderAddForm();
    } else if (this.state.deviceFormType === "edit") {
      form = this.renderEditForm();
    }

    return form;
  }

  render() {
    const { onUpdateDeviceModal } = this.props;
    const isDeviceFormShown = this.state.deviceFormType;

    return (
      dom.div(
        {
          id: "device-modal-wrapper",
          className: this.props.devices.isModalOpen ? "opened" : "closed",
        },
        dom.div({ className: "device-modal" },
          dom.div({ className: "device-modal-header" },
            !isDeviceFormShown ?
              dom.header({ className: "device-modal-title" },
                getStr("responsive.deviceSettings"),
                dom.button({
                  id: "device-close-button",
                  className: "devtools-button",
                  onClick: () => onUpdateDeviceModal(false),
                }),
              )
              :
              null,
            !isDeviceFormShown ?
              dom.button(
                {
                  id: "device-add-button",
                  onClick: () => this.onDeviceFormShow("add"),
                },
                getStr("responsive.addDevice2")
              )
              :
              null,
            this.renderForm(),
          ),
          dom.div({ className: "device-modal-content" },
            this.renderDevices()
          ),
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

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");

const { getStr, getFormatStr } = require("../utils/l10n");
const Types = require("../types");
const DeviceAdder = createFactory(require("./device-adder"));

module.exports = createClass({
  displayName: "DeviceModal",

  propTypes: {
    deviceAdderViewportTemplate: PropTypes.shape(Types.viewport).isRequired,
    devices: PropTypes.shape(Types.devices).isRequired,
    onAddCustomDevice: PropTypes.func.isRequired,
    onDeviceListUpdate: PropTypes.func.isRequired,
    onRemoveCustomDevice: PropTypes.func.isRequired,
    onUpdateDeviceDisplayed: PropTypes.func.isRequired,
    onUpdateDeviceModal: PropTypes.func.isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {};
  },

  componentDidMount() {
    window.addEventListener("keydown", this.onKeyDown, true);
  },

  componentWillReceiveProps(nextProps) {
    let {
      devices,
    } = nextProps;

    for (let type of devices.types) {
      for (let device of devices[type]) {
        this.setState({
          [device.name]: device.displayed,
        });
      }
    }
  },

  componentWillUnmount() {
    window.removeEventListener("keydown", this.onKeyDown, true);
  },

  onDeviceCheckboxChange({ nativeEvent: { button }, target }) {
    if (button !== 0) {
      return;
    }
    this.setState({
      [target.value]: !this.state[target.value]
    });
  },

  onDeviceModalSubmit() {
    let {
      devices,
      onDeviceListUpdate,
      onUpdateDeviceDisplayed,
      onUpdateDeviceModal,
    } = this.props;

    let preferredDevices = {
      "added": new Set(),
      "removed": new Set(),
    };

    for (let type of devices.types) {
      for (let device of devices[type]) {
        let newState = this.state[device.name];

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
  },

  onKeyDown(event) {
    if (!this.props.devices.isModalOpen) {
      return;
    }
    // Escape keycode
    if (event.keyCode === 27) {
      let {
        onUpdateDeviceModal
      } = this.props;
      onUpdateDeviceModal(false);
    }
  },

  render() {
    let {
      deviceAdderViewportTemplate,
      devices,
      onAddCustomDevice,
      onRemoveCustomDevice,
      onUpdateDeviceModal,
    } = this.props;

    const sortedDevices = {};
    for (let type of devices.types) {
      sortedDevices[type] = Object.assign([], devices[type])
        .sort((a, b) => a.name.localeCompare(b.name));
    }

    return dom.div(
      {
        id: "device-modal-wrapper",
        className: this.props.devices.isModalOpen ? "opened" : "closed",
      },
      dom.div(
        {
          className: "device-modal container",
        },
        dom.button({
          id: "device-close-button",
          className: "toolbar-button devtools-button",
          onClick: () => onUpdateDeviceModal(false),
        }),
        dom.div(
          {
            className: "device-modal-content",
          },
          devices.types.map(type => {
            return dom.div(
              {
                className: "device-type",
                key: type,
              },
              dom.header(
                {
                  className: "device-header",
                },
                type
              ),
              sortedDevices[type].map(device => {
                let details = getFormatStr(
                  "responsive.deviceDetails", device.width, device.height,
                  device.pixelRatio, device.userAgent, device.touch
                );

                let removeDeviceButton;
                if (type == "custom") {
                  removeDeviceButton = dom.button({
                    className: "device-remove-button toolbar-button devtools-button",
                    onClick: () => onRemoveCustomDevice(device),
                  });
                }

                return dom.label(
                  {
                    className: "device-label",
                    key: device.name,
                    title: details,
                  },
                  dom.input({
                    className: "device-input-checkbox",
                    type: "checkbox",
                    value: device.name,
                    checked: this.state[device.name],
                    onChange: this.onDeviceCheckboxChange,
                  }),
                  dom.span(
                    {
                      className: "device-name",
                    },
                    device.name
                  ),
                  removeDeviceButton
                );
              })
            );
          })
        ),
        DeviceAdder({
          devices,
          viewportTemplate: deviceAdderViewportTemplate,
          onAddCustomDevice,
        }),
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
    );
  },
});

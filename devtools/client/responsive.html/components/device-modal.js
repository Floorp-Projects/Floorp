/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { DOM: dom, createClass, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");
const { getStr } = require("../utils/l10n");
const Types = require("../types");

module.exports = createClass({
  displayName: "DeviceModal",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    onDeviceListUpdate: PropTypes.func.isRequired,
    onUpdateDeviceDisplayed: PropTypes.func.isRequired,
    onUpdateDeviceModalOpen: PropTypes.func.isRequired,
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
      onUpdateDeviceModalOpen,
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
    onUpdateDeviceModalOpen(false);
  },

  onKeyDown(event) {
    if (!this.props.devices.isModalOpen) {
      return;
    }
    // Escape keycode
    if (event.keyCode === 27) {
      let {
        onUpdateDeviceModalOpen
      } = this.props;
      onUpdateDeviceModalOpen(false);
    }
  },

  render() {
    let {
      devices,
      onUpdateDeviceModalOpen,
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
          onClick: () => onUpdateDeviceModalOpen(false),
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
                return dom.label(
                  {
                    className: "device-label",
                    key: device.name,
                  },
                  dom.input({
                    className: "device-input-checkbox",
                    type: "checkbox",
                    value: device.name,
                    checked: this.state[device.name],
                    onChange: this.onDeviceCheckboxChange,
                  }),
                  device.name
                );
              })
            );
          })
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
          onClick: () => onUpdateDeviceModalOpen(false),
        }
      )
    );
  },
});

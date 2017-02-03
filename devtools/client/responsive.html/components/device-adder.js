/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { DOM: dom, createClass, createFactory, PropTypes, addons } =
  require("devtools/client/shared/vendor/react");

const { getFormatStr, getStr } = require("../utils/l10n");
const Types = require("../types");
const ViewportDimension = createFactory(require("./viewport-dimension"));

module.exports = createClass({
  displayName: "DeviceAdder",

  propTypes: {
    devices: PropTypes.shape(Types.devices).isRequired,
    viewportTemplate: PropTypes.shape(Types.viewport).isRequired,
  },

  mixins: [ addons.PureRenderMixin ],

  getInitialState() {
    return {};
  },

  onDeviceAdderShow() {
    this.setState({
      deviceAdderDisplayed: true,
    });
  },

  onDeviceAdderSave() {
    this.setState({
      deviceAdderDisplayed: false,
    });
  },

  render() {
    let {
      devices,
      viewportTemplate,
    } = this.props;

    let {
      deviceAdderDisplayed,
    } = this.state;

    if (!deviceAdderDisplayed) {
      return dom.div(
        {
          id: "device-adder"
        },
        dom.button(
          {
            id: "device-adder-show",
            onClick: this.onDeviceAdderShow,
          },
          getStr("responsive.addDevice")
        )
      );
    }

    // If a device is currently selected, fold its attributes into a single object for use
    // as the starting values of the form.  If no device is selected, use the values for
    // the current window.
    let deviceName;
    let normalizedViewport = Object.assign({}, viewportTemplate);
    if (viewportTemplate.device) {
      let device = devices[viewportTemplate.deviceType].find(d => {
        return d.name == viewportTemplate.device;
      });
      deviceName = getFormatStr("responsive.customDeviceNameFromBase", device.name);
      Object.assign(normalizedViewport, {
        pixelRatio: device.pixelRatio,
        userAgent: device.userAgent,
        touch: device.touch,
      });
    } else {
      deviceName = getStr("responsive.customDeviceName");
      Object.assign(normalizedViewport, {
        pixelRatio: window.devicePixelRatio,
        userAgent: navigator.userAgent,
        touch: false,
      });
    }

    return dom.div(
      {
        id: "device-adder"
      },
      dom.label(
        {},
        dom.span(
          {
            className: "device-adder-label",
          },
          getStr("responsive.deviceAdderName")
        ),
        dom.input({
          defaultValue: deviceName,
        })
      ),
      dom.label(
        {},
        dom.span(
          {
            className: "device-adder-label"
          },
          getStr("responsive.deviceAdderSize")
        ),
        ViewportDimension({
          viewport: {
            width: normalizedViewport.width,
            height: normalizedViewport.height,
          },
          onRemoveDeviceAssociation: () => {},
          onResizeViewport: () => {},
        })
      ),
      dom.label(
        {},
        dom.span(
          {
            className: "device-adder-label"
          },
          getStr("responsive.deviceAdderPixelRatio")
        ),
        dom.input({
          defaultValue: normalizedViewport.pixelRatio,
        })
      ),
      dom.label(
        {},
        dom.span(
          {
            className: "device-adder-label"
          },
          getStr("responsive.deviceAdderUserAgent")
        ),
        dom.input({
          defaultValue: normalizedViewport.userAgent,
        })
      ),
      dom.label(
        {},
        dom.span(
          {
            className: "device-adder-label"
          },
          getStr("responsive.deviceAdderTouch")
        ),
        dom.input({
          defaultChecked: normalizedViewport.touch,
          type: "checkbox",
        })
      ),
      dom.button(
        {
          id: "device-adder-save",
          onClick: this.onDeviceAdderSave,
        },
        getStr("responsive.deviceAdderSave")
      )
    );
  },
});

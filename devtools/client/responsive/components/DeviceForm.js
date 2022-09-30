/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const {
  createFactory,
  createRef,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const ViewportDimension = createFactory(
  require("resource://devtools/client/responsive/components/ViewportDimension.js")
);

const {
  getStr,
} = require("resource://devtools/client/responsive/utils/l10n.js");
const Types = require("resource://devtools/client/responsive/types.js");

class DeviceForm extends PureComponent {
  static get propTypes() {
    return {
      formType: PropTypes.string.isRequired,
      device: PropTypes.shape(Types.device).isRequired,
      devices: PropTypes.shape(Types.devices).isRequired,
      onSave: PropTypes.func.isRequired,
      viewportTemplate: PropTypes.shape(Types.viewport).isRequired,
      onDeviceFormHide: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    const { height, width } = this.props.viewportTemplate;

    this.state = {
      height,
      width,
    };

    this.nameInputRef = createRef();
    this.pixelRatioInputRef = createRef();
    this.touchInputRef = createRef();
    this.userAgentInputRef = createRef();

    this.onChangeSize = this.onChangeSize.bind(this);
    this.onDeviceFormHide = this.onDeviceFormHide.bind(this);
    this.onDeviceFormSave = this.onDeviceFormSave.bind(this);
    this.onInputFocus = this.onInputFocus.bind(this);
    this.validateNameField = this.validateNameField.bind(this);
  }

  onChangeSize(_, width, height) {
    this.setState({
      width,
      height,
    });
  }

  onDeviceFormSave(e) {
    e.preventDefault();

    if (!this.pixelRatioInputRef.current.checkValidity()) {
      return;
    }

    if (
      !this.validateNameField(
        this.nameInputRef.current.value,
        this.props.device
      )
    ) {
      this.nameInputRef.current.setCustomValidity(
        getStr("responsive.deviceNameAlreadyInUse")
      );
      return;
    }

    this.props.onSave({
      name: this.nameInputRef.current.value.trim(),
      width: this.state.width,
      height: this.state.height,
      pixelRatio: parseFloat(this.pixelRatioInputRef.current.value),
      userAgent: this.userAgentInputRef.current.value,
      touch: this.touchInputRef.current.checked,
    });

    this.onDeviceFormHide();
  }

  onDeviceFormHide() {
    // Ensure that we have onDeviceFormHide before calling it.
    if (this.props.onDeviceFormHide) {
      this.props.onDeviceFormHide();
    }
  }

  onInputFocus(e) {
    // If the formType is "add", select all text in input field when focused.
    if (this.props.formType === "add") {
      e.target.select();
    }
  }

  /**
   * Validates the name field's value.
   *
   * @param  {String} value
   *         The input field value for the device name.
   * @return {Boolean} true if device name is valid, false otherwise.
   */
  validateNameField(value) {
    const nameFieldValue = value.trim();
    let isValidDeviceName = false;

    // If the formType is "add", then we just need to check if a custom device with that
    // same name exists.
    if (this.props.formType === "add") {
      isValidDeviceName = !this.props.devices.custom.find(
        device => device.name == nameFieldValue
      );
    } else {
      // Otherwise, the formType is "edit". We'd have to find another device that
      // already has the same name, but not itself, so:
      // Find the index of the device being edited. Use this index value to distinguish
      // between the device being edited from other devices in the list.
      const deviceIndex = this.props.devices.custom.findIndex(({ name }) => {
        return name === this.props.device.name;
      });

      isValidDeviceName = !this.props.devices.custom.find((d, index) => {
        return d.name === nameFieldValue && index !== deviceIndex;
      });
    }

    return isValidDeviceName;
  }

  render() {
    const { device, formType } = this.props;
    const { width, height } = this.state;

    return dom.form(
      { id: "device-form" },
      dom.label(
        { id: "device-form-name" },
        dom.span(
          { className: "device-form-label" },
          getStr("responsive.deviceAdderName")
        ),
        dom.input({
          defaultValue: device.name,
          ref: this.nameInputRef,
          onFocus: this.onInputFocus,
        })
      ),
      dom.label(
        { id: "device-form-size" },
        dom.span(
          { className: "device-form-label" },
          getStr("responsive.deviceAdderSize")
        ),
        ViewportDimension({
          viewport: { width, height },
          doResizeViewport: this.onChangeSize,
          onRemoveDeviceAssociation: () => {},
        })
      ),
      dom.label(
        { id: "device-form-pixel-ratio" },
        dom.span(
          { className: "device-form-label" },
          getStr("responsive.deviceAdderPixelRatio2")
        ),
        dom.input({
          type: "number",
          step: "any",
          defaultValue: device.pixelRatio,
          ref: this.pixelRatioInputRef,
          onFocus: this.onInputFocus,
        })
      ),
      dom.label(
        { id: "device-form-user-agent" },
        dom.span(
          { className: "device-form-label" },
          getStr("responsive.deviceAdderUserAgent2")
        ),
        dom.input({
          defaultValue: device.userAgent,
          ref: this.userAgentInputRef,
          onFocus: this.onInputFocus,
        })
      ),
      dom.label(
        { id: "device-form-touch" },
        dom.input({
          defaultChecked: device.touch,
          type: "checkbox",
          ref: this.touchInputRef,
        }),
        dom.span(
          { className: "device-form-label" },
          getStr("responsive.deviceAdderTouch2")
        )
      ),
      dom.div(
        { className: "device-form-buttons" },
        dom.button(
          { id: "device-form-save", onClick: this.onDeviceFormSave },
          formType === "add"
            ? getStr("responsive.deviceAdderSave")
            : getStr("responsive.deviceFormUpdate")
        ),
        dom.button(
          { id: "device-form-cancel", onClick: this.onDeviceFormHide },
          getStr("responsive.deviceAdderCancel")
        )
      )
    );
  }
}

module.exports = DeviceForm;

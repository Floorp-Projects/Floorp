/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env browser */

"use strict";

const { createFactory, createRef, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ViewportDimension = createFactory(require("./ViewportDimension"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class DeviceForm extends PureComponent {
  static get propTypes() {
    return {
      buttonText: PropTypes.string,
      formType: PropTypes.string.isRequired,
      device: PropTypes.shape(Types.device).isRequired,
      onSave: PropTypes.func.isRequired,
      validateName: PropTypes.func.isRequired,
      viewportTemplate: PropTypes.shape(Types.viewport).isRequired,
      onDeviceFormHide: PropTypes.func.isRequired,
      onDeviceFormShow: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    const {
      height,
      width,
    } = this.props.viewportTemplate;

    this.state = {
      isShown: false,
      height,
      width,
    };

    this.nameInputRef = createRef();
    this.pixelRatioInputRef = createRef();
    this.touchInputRef = createRef();
    this.userAgentInputRef = createRef();

    this.onChangeSize = this.onChangeSize.bind(this);
    this.onDeviceFormHide  = this.onDeviceFormHide.bind(this);
    this.onDeviceFormShow = this.onDeviceFormShow.bind(this);
    this.onDeviceFormSave = this.onDeviceFormSave.bind(this);
  }

  onChangeSize(_, width, height) {
    this.setState({
      width,
      height,
    });
  }

  onDeviceFormSave() {
    if (!this.pixelRatioInputRef.current.checkValidity()) {
      return;
    }

    if (!this.props.validateName(this.nameInputRef.current.value)) {
      this.nameInputRef.current
        .setCustomValidity(getStr("responsive.deviceNameAlreadyInUse"));
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
    this.setState({ isShown: false });

    // Ensure that we have onDeviceFormHide before calling it.
    if (this.props.onDeviceFormHide) {
      this.props.onDeviceFormHide();
    }
  }

  onDeviceFormShow() {
    this.setState({ isShown: true });

    // Ensure that we have onDeviceFormShow before calling it.
    if (this.props.onDeviceFormShow) {
      this.props.onDeviceFormShow();
    }
  }

  render() {
    const { buttonText, device, formType } = this.props;
    const { isShown, width, height } = this.state;

    if (!isShown) {
      return (
        dom.button(
          {
            id: `device-${formType}-button`,
            className: "devtools-button",
            onClick: this.onDeviceFormShow,
          },
          buttonText,
        )
      );
    }

    return (
      dom.form({ id: "device-form" },
        dom.label({ id: "device-form-name", className: formType },
          dom.span({ className: "device-form-label" },
            getStr("responsive.deviceAdderName")
          ),
          dom.input({
            defaultValue: device.name,
            ref: this.nameInputRef,
          })
        ),
        dom.label({ id: "device-form-size" },
          dom.span({ className: "device-form-label" },
            getStr("responsive.deviceAdderSize")
          ),
          ViewportDimension({
            viewport: { width, height },
            doResizeViewport: this.onChangeSize,
            onRemoveDeviceAssociation: () => {},
          })
        ),
        dom.label({ id: "device-form-pixel-ratio" },
          dom.span({ className: "device-form-label" },
            getStr("responsive.deviceAdderPixelRatio2")
          ),
          dom.input({
            type: "number",
            step: "any",
            defaultValue: device.pixelRatio,
            ref: this.pixelRatioInputRef,
          })
        ),
        dom.label({ id: "device-form-user-agent" },
          dom.span({ className: "device-form-label" },
            getStr("responsive.deviceAdderUserAgent2")
          ),
          dom.input({
            defaultValue: device.userAgent,
            ref: this.userAgentInputRef,
          })
        ),
        dom.label({ id: "device-form-touch" },
          dom.input({
            defaultChecked: device.touch,
            type: "checkbox",
            ref: this.touchInputRef,
          }),
          dom.span({ className: "device-form-label" },
            getStr("responsive.deviceAdderTouch2")
          )
        ),
        dom.div({ className: "device-form-buttons" },
          dom.button({ id: "device-form-save", onClick: this.onDeviceFormSave },
            getStr("responsive.deviceAdderSave")
          ),
          dom.button({ id: "device-form-cancel", onClick: this.onDeviceFormHide },
            getStr("responsive.deviceAdderCancel")
          )
        )
      )
    );
  }
}

module.exports = DeviceForm;

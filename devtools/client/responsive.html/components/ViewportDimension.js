/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { isKeyIn } = require("../utils/key");

const Constants = require("../constants");
const Types = require("../types");

/**
 * Get the increment/decrement step to use for the provided key event.
 */
function getIncrement(event) {
  const defaultIncrement = 1;
  const largeIncrement = 100;
  const mediumIncrement = 10;

  let increment = 0;
  const key = event.keyCode;

  if (isKeyIn(key, "UP", "PAGE_UP")) {
    increment = 1 * defaultIncrement;
  } else if (isKeyIn(key, "DOWN", "PAGE_DOWN")) {
    increment = -1 * defaultIncrement;
  }

  if (event.shiftKey) {
    if (isKeyIn(key, "PAGE_UP", "PAGE_DOWN")) {
      increment *= largeIncrement;
    } else {
      increment *= mediumIncrement;
    }
  }

  return increment;
}

class ViewportDimension extends Component {
  static get propTypes() {
    return {
      viewport: PropTypes.shape(Types.viewport).isRequired,
      onChangeSize: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    const { width, height } = props.viewport;

    this.state = {
      width,
      height,
      isEditing: false,
      isInvalid: false,
    };

    this.validateInput = this.validateInput.bind(this);
    this.onInputBlur = this.onInputBlur.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onInputFocus = this.onInputFocus.bind(this);
    this.onInputKeyDown = this.onInputKeyDown.bind(this);
    this.onInputKeyUp = this.onInputKeyUp.bind(this);
    this.onInputSubmit = this.onInputSubmit.bind(this);
  }

  componentWillReceiveProps(nextProps) {
    const { width, height } = nextProps.viewport;

    this.setState({
      width,
      height,
    });
  }

  validateInput(value) {
    let isInvalid = true;

    // Check the value is a number and greater than MIN_VIEWPORT_DIMENSION
    if (/^\d{3,4}$/.test(value) &&
        parseInt(value, 10) >= Constants.MIN_VIEWPORT_DIMENSION) {
      isInvalid = false;
    }

    this.setState({
      isInvalid,
    });
  }

  onInputBlur() {
    const { width, height } = this.props.viewport;

    if (this.state.width != width || this.state.height != height) {
      this.onInputSubmit();
    }

    this.setState({
      isEditing: false,
      inInvalid: false,
    });
  }

  onInputChange({ target }, callback) {
    if (target.value.length > 4) {
      return;
    }

    if (this.refs.widthInput == target) {
      this.setState({ width: target.value }, callback);
      this.validateInput(target.value);
    }

    if (this.refs.heightInput == target) {
      this.setState({ height: target.value }, callback);
      this.validateInput(target.value);
    }
  }

  onInputFocus() {
    this.setState({
      isEditing: true,
    });
  }

  onInputKeyDown(event) {
    const { target } = event;
    const increment = getIncrement(event);
    if (!increment) {
      return;
    }
    target.value = parseInt(target.value, 10) + increment;
    this.onInputChange(event, this.onInputSubmit);
  }

  onInputKeyUp({ target, keyCode }) {
    // On Enter, submit the input
    if (keyCode == 13) {
      this.onInputSubmit();
    }

    // On Esc, blur the target
    if (keyCode == 27) {
      target.blur();
    }
  }

  onInputSubmit() {
    if (this.state.isInvalid) {
      const { width, height } = this.props.viewport;

      this.setState({
        width,
        height,
        isInvalid: false,
      });

      return;
    }

    // Change the device selector back to an unselected device
    // TODO: Bug 1332754: Logic like this probably belongs in the action creator.
    if (this.props.viewport.device) {
      this.props.onRemoveDeviceAssociation();
    }
    this.props.onChangeSize(parseInt(this.state.width, 10),
                            parseInt(this.state.height, 10));
  }

  render() {
    let editableClass = "viewport-dimension-editable";
    let inputClass = "viewport-dimension-input";

    if (this.state.isEditing) {
      editableClass += " editing";
      inputClass += " editing";
    }

    if (this.state.isInvalid) {
      editableClass += " invalid";
    }

    return dom.div(
      {
        className: "viewport-dimension",
      },
      dom.div(
        {
          className: editableClass,
        },
        dom.input({
          ref: "widthInput",
          className: inputClass,
          size: 4,
          value: this.state.width,
          onBlur: this.onInputBlur,
          onChange: this.onInputChange,
          onFocus: this.onInputFocus,
          onKeyDown: this.onInputKeyDown,
          onKeyUp: this.onInputKeyUp,
        }),
        dom.span({
          className: "viewport-dimension-separator",
        }, "×"),
        dom.input({
          ref: "heightInput",
          className: inputClass,
          size: 4,
          value: this.state.height,
          onBlur: this.onInputBlur,
          onChange: this.onInputChange,
          onFocus: this.onInputFocus,
          onKeyDown: this.onInputKeyDown,
          onKeyUp: this.onInputKeyUp,
        })
      )
    );
  }
}

module.exports = ViewportDimension;

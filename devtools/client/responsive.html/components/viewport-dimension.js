/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM: dom, createClass, PropTypes } =
  require("devtools/client/shared/vendor/react");

const Constants = require("../constants");
const Types = require("../types");

module.exports = createClass({
  displayName: "ViewportDimension",

  propTypes: {
    viewport: PropTypes.shape(Types.viewport).isRequired,
    onChangeViewportDevice: PropTypes.func.isRequired,
    onResizeViewport: PropTypes.func.isRequired,
  },

  getInitialState() {
    let { width, height } = this.props.viewport;

    return {
      width,
      height,
      isEditing: false,
      isInvalid: false,
    };
  },

  componentWillReceiveProps(nextProps) {
    let { width, height } = nextProps.viewport;

    this.setState({
      width,
      height,
    });
  },

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
  },

  onInputBlur() {
    let { width, height } = this.props.viewport;

    if (this.state.width != width || this.state.height != height) {
      this.onInputSubmit();
    }

    this.setState({
      isEditing: false,
      inInvalid: false,
    });
  },

  onInputChange({ target }) {
    if (target.value.length > 4) {
      return;
    }

    if (this.refs.widthInput == target) {
      this.setState({ width: target.value });
      this.validateInput(target.value);
    }

    if (this.refs.heightInput == target) {
      this.setState({ height: target.value });
      this.validateInput(target.value);
    }
  },

  onInputFocus() {
    this.setState({
      isEditing: true,
    });
  },

  onInputKeyUp({ target, keyCode }) {
    // On Enter, submit the input
    if (keyCode == 13) {
      this.onInputSubmit();
    }

    // On Esc, blur the target
    if (keyCode == 27) {
      target.blur();
    }
  },

  onInputSubmit() {
    if (this.state.isInvalid) {
      let { width, height } = this.props.viewport;

      this.setState({
        width,
        height,
        isInvalid: false,
      });

      return;
    }

    // Change the device selector back to an unselected device
    this.props.onChangeViewportDevice("");
    this.props.onResizeViewport(parseInt(this.state.width, 10),
                                parseInt(this.state.height, 10));
  },

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
          onKeyUp: this.onInputKeyUp,
        })
      )
    );
  },

});

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { isKeyIn } = require("devtools/client/responsive/utils/key");
const {
  MIN_VIEWPORT_DIMENSION,
} = require("devtools/client/responsive/constants");
const Types = require("devtools/client/responsive/types");

class ViewportDimension extends PureComponent {
  static get propTypes() {
    return {
      doResizeViewport: PropTypes.func.isRequired,
      onRemoveDeviceAssociation: PropTypes.func.isRequired,
      viewport: PropTypes.shape(Types.viewport).isRequired,
    };
  }

  constructor(props) {
    super(props);

    const { width, height } = props.viewport;

    this.state = {
      width,
      height,
      isEditing: false,
      isWidthValid: true,
      isHeightValid: true,
    };

    this.isInputValid = this.isInputValid.bind(this);
    this.onInputBlur = this.onInputBlur.bind(this);
    this.onInputChange = this.onInputChange.bind(this);
    this.onInputFocus = this.onInputFocus.bind(this);
    this.onInputKeyDown = this.onInputKeyDown.bind(this);
    this.onInputKeyUp = this.onInputKeyUp.bind(this);
    this.onInputSubmit = this.onInputSubmit.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(nextProps) {
    const { width, height } = nextProps.viewport;

    this.setState({
      width,
      height,
    });
  }

  /**
   * Return true if the given value is a number and greater than MIN_VIEWPORT_DIMENSION
   * and false otherwise.
   */
  isInputValid(value) {
    return (
      /^\d{2,4}$/.test(value) && parseInt(value, 10) >= MIN_VIEWPORT_DIMENSION
    );
  }

  onInputBlur() {
    const { width, height } = this.props.viewport;

    if (this.state.width != width || this.state.height != height) {
      this.onInputSubmit();
    }

    this.setState({ isEditing: false });
  }

  onInputChange({ target }, callback) {
    if (target.value.length > 4) {
      return;
    }

    if (this.widthInput == target) {
      this.setState(
        {
          width: target.value,
          isWidthValid: this.isInputValid(target.value),
        },
        callback
      );
    }

    if (this.heightInput == target) {
      this.setState(
        {
          height: target.value,
          isHeightValid: this.isInputValid(target.value),
        },
        callback
      );
    }
  }

  onInputFocus(e) {
    this.setState({ isEditing: true });
    e.target.select();
  }

  onInputKeyDown(event) {
    const increment = getIncrement(event);
    if (!increment) {
      return;
    }

    const { target } = event;
    target.value = parseInt(target.value, 10) + increment;
    this.onInputChange(event, this.onInputSubmit);

    // Keep this event from having default processing. Since the field is a
    // number field, default processing would trigger additional manipulations
    // of the value, and we've already applied the desired amount.
    event.preventDefault();
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
    const {
      viewport,
      onRemoveDeviceAssociation,
      doResizeViewport,
    } = this.props;

    if (!this.state.isWidthValid || !this.state.isHeightValid) {
      const { width, height } = viewport;

      this.setState({
        width,
        height,
        isWidthValid: true,
        isHeightValid: true,
      });

      return;
    }

    // Change the device selector back to an unselected device
    // TODO: Bug 1332754: Logic like this probably belongs in the action creator.
    if (viewport.device) {
      onRemoveDeviceAssociation(viewport.id);
    }

    doResizeViewport(
      viewport.id,
      parseInt(this.state.width, 10),
      parseInt(this.state.height, 10)
    );
  }

  render() {
    return dom.div(
      {
        className:
          "viewport-dimension" +
          (this.state.isEditing ? " editing" : "") +
          (!this.state.isWidthValid || !this.state.isHeightValid
            ? " invalid"
            : ""),
      },
      dom.input({
        ref: input => {
          this.widthInput = input;
        },
        className:
          "text-input viewport-dimension-input" +
          (this.state.isWidthValid ? "" : " invalid"),
        size: 4,
        type: "number",
        value: this.state.width,
        onBlur: this.onInputBlur,
        onChange: this.onInputChange,
        onFocus: this.onInputFocus,
        onKeyDown: this.onInputKeyDown,
        onKeyUp: this.onInputKeyUp,
      }),
      dom.span(
        {
          className: "viewport-dimension-separator",
        },
        "×"
      ),
      dom.input({
        ref: input => {
          this.heightInput = input;
        },
        className:
          "text-input viewport-dimension-input" +
          (this.state.isHeightValid ? "" : " invalid"),
        size: 4,
        type: "number",
        value: this.state.height,
        onBlur: this.onInputBlur,
        onChange: this.onInputChange,
        onFocus: this.onInputFocus,
        onKeyDown: this.onInputKeyDown,
        onKeyUp: this.onInputKeyUp,
      })
    );
  }
}

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

module.exports = ViewportDimension;

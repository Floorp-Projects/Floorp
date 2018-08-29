/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { KeyCodes } = require("devtools/client/shared/keycodes");

// Milliseconds between auto-increment interval iterations.
const AUTOINCREMENT_DELAY = 1000;

class FontPropertyValue extends PureComponent {
  static get propTypes() {
    return {
      autoIncrement: PropTypes.bool,
      className: PropTypes.string,
      defaultValue: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      label: PropTypes.string.isRequired,
      min: PropTypes.number.isRequired,
      max: PropTypes.number.isRequired,
      name: PropTypes.string.isRequired,
      onChange: PropTypes.func.isRequired,
      step: PropTypes.number,
      unit: PropTypes.oneOfType([ PropTypes.string, null ]),
      unitOptions: PropTypes.array,
      value: PropTypes.number,
    };
  }

  static get defaultProps() {
    return {
      autoIncrement: false,
      className: "",
      step: 1,
      unitOptions: []
    };
  }

  constructor(props) {
    super(props);
    // Interval ID for the auto-increment operation.
    this.interval = null;
    this.state = {
      // Whether the user is dragging the slider thumb or pressing on the numeric stepper.
      interactive: false,
      // Snapshot of the value from props before the user starts editing the number input.
      // Used to restore the value when the input is left invalid.
      initialValue: this.props.value,
      // Snapshot of the value from props. Reconciled with props on blur.
      // Used while the user is interacting with the inputs.
      value: this.props.value,
    };

    this.autoIncrement = this.autoIncrement.bind(this);
    this.onBlur = this.onBlur.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onFocus = this.onFocus.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onKeyUp = this.onKeyUp.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseMove = this.onMouseMove.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);
    this.onUnitChange = this.onUnitChange.bind(this);
    this.stopAutoIncrement = this.stopAutoIncrement.bind(this);
  }

  componentDidUpdate(prevProps, prevState) {
    // Clear the auto-increment interval if interactive state changed from true to false.
    if (prevState.interactive && !this.state.interactive) {
      this.stopAutoIncrement();
    }
  }

  componentWillUnmount() {
    this.stopAutoIncrement();
  }

  /**
   * Increment the current value with a step of the next order of magnitude.
   */
  autoIncrement() {
    const value = this.props.value + this.props.step * 10;
    this.updateValue(value);
  }

  /**
   * Check if the given value is at or exceeds the maximum value for the slider and number
   * inputs. Using Math.floor() on maximum value because unit conversion can yield numbers
   * with decimals that can't be reached with the step increment for the converted unit.
   * For example: max = 1000.75% and step = 1
   *
   * @param  {Number} value
   *         Numeric value.
   * @return {Boolean}
   */
  isAtUpperBound(value) {
    return value >= Math.floor(this.props.max);
  }

  /**
   * Check if the given value is valid according to the constraints of this component.
   * Ensure it is a number and that it does not go outside the min/max limits, unless
   * allowed by the `autoIncrement` props flag.
   *
   * @param  {Number} value
   *         Numeric value
   * @return {Boolean}
   *         Whether the value conforms to the components contraints.
   */
  isValueValid(value) {
    const { autoIncrement, min, max } = this.props;

    if (typeof value !== "number" || isNaN(value)) {
      return false;
    }

    if (min !== undefined && value < min) {
      return false;
    }

    // Ensure it does not exceed maximum value, unless auto-incrementing is permitted.
    if (max !== undefined && value > this.props.max && !autoIncrement) {
      return false;
    }

    return true;
  }

  /**
   * Handler for "blur" events from the range and number input fields.
   * Reconciles the value between internal state and props.
   * Marks the input as non-interactive so it may update in response to changes in props.
   */
  onBlur() {
    const isValid = this.isValueValid(this.state.value);
    let value;

    if (isValid) {
      value = this.state.value;
    } else if (this.state.value !== null) {
      value = Math.max(this.props.min, Math.min(this.state.value, this.props.max));
    } else {
      value = this.state.initialValue;
    }

    this.updateValue(value);
    this.toggleInteractiveState(false);
  }

  /**
   * Handler for "change" events from the range and number input fields. Calls the change
   * handler provided with props and updates internal state with the current value.
   *
   * Number inputs in Firefox can't be trusted to filter out non-digit characters,
   * therefore we must implement our own validation.
   * @see https://bugzilla.mozilla.org/show_bug.cgi?id=1398528
   *
   * @param {Event} e
   *        Change event.
   */
  onChange(e) {
    // Regular expresion to check for positive floating point or integer numbers.
    // Whitespace and non-digit characters are invalid (aside from a single dot).
    const regex = /^[0-9]+(.[0-9]+)?$/;
    let string = e.target.value.trim();

    if (e.target.validity.badInput) {
      return;
    }

    // Prefix with zero if the string starts with a dot: .5 => 0.5
    if (string.charAt(0) === "." && string.length > 1) {
      string = "0" + string;
      e.target.value = string;
    }

    // Accept empty strings to allow the input value to be completely erased while typing.
    // A null value will be handled on blur. @see this.onBlur()
    if (string === "") {
      this.setState((prevState) => {
        return {
          ...prevState,
          value: null,
        };
      });

      return;
    }

    // Catch any negative or irrational numbers.
    if (!regex.test(string)) {
      return;
    }

    const value = parseFloat(string);
    this.updateValue(value);
  }

  onFocus(e) {
    if (e.target.type === "number") {
      e.target.select();
    }

    this.setState((prevState) => {
      return {
        ...prevState,
        interactive: true,
        initialValue: this.props.value,
      };
    });
  }

  /**
   * Handler for "keydown" events from the range input field.
   * Begin auto-incrementing if the slider value is already at the upper boun and the
   * keyboard gesture requests a higher value.
   *
   * @param {Event} e
   *        KeyDown event.
   */
  onKeyDown(e) {
    if (this.isAtUpperBound(this.props.value) &&
        e.keyCode === KeyCodes.DOM_VK_UP || e.keyCode === KeyCodes.DOM_VK_RIGHT) {
      this.startAutoIncrement();
    }
  }

  onKeyUp(e) {
    if (e.keyCode === KeyCodes.DOM_VK_UP || e.keyCode === KeyCodes.DOM_VK_RIGHT) {
      this.stopAutoIncrement();
    }
  }

  onUnitChange(e) {
    this.props.onChange(this.props.name, this.props.value, this.props.unit,
       e.target.value);
    // Reset internal state value and wait for converted value from props.
    this.setState((prevState) => {
      return {
        ...prevState,
        value: null
      };
    });
  }

  onMouseDown() {
    this.toggleInteractiveState(true);
  }

  /**
   * Handler for "mousemove" event from range input. If the user is actively interacting
   * by dragging the slider thumb, start or stop the auto-incrementing behavior depending
   * on whether the input value is at the upper bound or not.
   *
   * @param {MouseEvent} e
   */
  onMouseMove(e) {
    if (this.state.interactive && e.buttons) {
      this.isAtUpperBound(this.props.value)
        ? this.startAutoIncrement()
        : this.stopAutoIncrement();
    }
  }

  onMouseUp() {
    this.stopAutoIncrement();
    this.toggleInteractiveState(false);
  }

  startAutoIncrement() {
    // Do not set auto-increment interval if not allowed to or if one is already set.
    if (!this.props.autoIncrement || this.interval) {
      return;
    }

    this.interval = setInterval(this.autoIncrement, AUTOINCREMENT_DELAY);
  }

  stopAutoIncrement() {
    clearInterval(this.interval);
    this.interval = null;
  }

  /**
   * Toggle the "interactive" state which causes render() to use `value` fom internal
   * state instead of from props to prevent jittering during continous dragging of the
   * range input thumb or incrementing from the number input.
   *
   * @param {Boolean} isInteractive
   *        Whether to mark the interactive state on or off.
   */
  toggleInteractiveState(isInteractive) {
    this.setState((prevState) => {
      return {
        ...prevState,
        interactive: isInteractive
      };
    });
  }

  /**
   * Calls the given `onChange` callback with the current property name, value and unit
   * if the value is valid according to the constraints of this component (min, max).
   * Updates the internal state with the current value. This will be used to render the
   * UI while the input is interactive and the user may be typing a value that's not yet
   * valid.
   *
   * @see this.onBlur() for logic reconciling the internal state with props.
   *
   * @param {Number} value
   *        Numeric property value.
   */
  updateValue(value) {
    if (this.isValueValid(value)) {
      this.props.onChange(this.props.name, value, this.props.unit);
    }

    this.setState((prevState) => {
      return {
        ...prevState,
        value
      };
    });
  }

  renderUnitSelect() {
    if (!this.props.unitOptions.length) {
      return null;
    }

    // Ensure the select element has the current unit type even if we don't recognize it.
    // The unit conversion function will use a 1-to-1 scale for unrecognized units.
    const options = this.props.unitOptions.includes(this.props.unit) ?
      this.props.unitOptions
      :
      this.props.unitOptions.concat([this.props.unit]);

    return dom.select(
      {
        className: "font-value-select",
        onChange: this.onUnitChange,
      },
      options.map(unit => {
        return dom.option(
          {
            selected: unit === this.props.unit,
            value: unit,
          },
          unit
        );
      })
    );
  }

  render() {
    // Guard against bad axis data.
    if (this.props.min === this.props.max) {
      return null;
    }

    const propsValue = this.props.value !== null
      ? this.props.value
      : this.props.defaultValue;

    const defaults = {
      min: this.props.min,
      max: this.props.max,
      onBlur: this.onBlur,
      onChange: this.onChange,
      onFocus: this.onFocus,
      step: this.props.step,
      // While interacting with the range and number inputs, prevent updating value from
      // outside props which is debounced and causes jitter on successive renders.
      value: this.state.interactive ? this.state.value : propsValue,
    };

    const range = dom.input(
      {
        ...defaults,
        onKeyDown: this.onKeyDown,
        onKeyUp: this.onKeyUp,
        onMouseDown: this.onMouseDown,
        onMouseMove: this.onMouseMove,
        onMouseUp: this.onMouseUp,
        className: "font-value-slider",
        name: this.props.name,
        title: this.props.label,
        type: "range",
      }
    );

    const input = dom.input(
      {
        ...defaults,
        // Remove upper limit from number input if it is allowed to auto-increment.
        max: this.props.autoIncrement ? null : this.props.max,
        name: this.props.name,
        className: "font-value-input",
        type: "number",
      }
    );

    return dom.label(
      {
        className: `font-control ${this.props.className}`,
      },
      dom.span(
        {
          className: "font-control-label",
        },
        this.props.label
      ),
      dom.div(
        {
          className: "font-control-input"
        },
        range,
        input,
        this.renderUnitSelect()
      )
    );
  }
}

module.exports = FontPropertyValue;

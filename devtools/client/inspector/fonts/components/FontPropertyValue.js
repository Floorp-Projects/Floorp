/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { KeyCodes } = require("devtools/client/shared/keycodes");

// Milliseconds between auto-increment interval iterations.
const AUTOINCREMENT_DELAY = 300;
const UNITS = ["em", "rem", "%", "px", "vh", "vw"];

class FontPropertyValue extends PureComponent {
  static get propTypes() {
    return {
      allowAutoIncrement: PropTypes.bool,
      defaultValue: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      label: PropTypes.string.isRequired,
      min: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      max: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      name: PropTypes.string.isRequired,
      onChange: PropTypes.func.isRequired,
      showUnit: PropTypes.bool,
      step: PropTypes.oneOfType([ PropTypes.string, PropTypes.number ]),
      unit: PropTypes.oneOfType([ PropTypes.string, null ]),
      value: PropTypes.number,
    };
  }

  constructor(props) {
    super(props);
    // Interval ID for the auto-increment operation.
    this.interval = null;
    this.state = {
      // Whether the user is dragging the slider thumb or pressing on the numeric stepper.
      interactive: false,
      value: null,
    };

    this.autoIncrement = this.autoIncrement.bind(this);
    this.onBlur = this.onBlur.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onKeyDown = this.onKeyDown.bind(this);
    this.onKeyUp = this.onKeyUp.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
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

  onBlur() {
    this.toggleInteractiveState(false);
  }

  /**
   * Handler for "change" events from the range and number input fields. Calls the change
   * handler provided with props and updates internal state with the current value.
   * Begins auto-incrementing if the value is already at the upper bound.
   *
   * @param {Event} e
   *        Change event.
   */
  onChange(e) {
    const value = parseFloat(e.target.value);
    this.updateValue(value);

    // Stop auto-incrementing when dragging back down from the upper bound.
    if (value < this.props.max && this.interval) {
      this.stopAutoIncrement();
    }

    // Begin auto-incrementing when reaching the upper bound.
    if (this.isAtUpperBound(value) && this.state.interactive) {
      this.startAutoIncrement();
    }
  }

  /**
   * Handler for "keydown" events from the range and number input fields.
   * Toggles on the "interactive" state. @See toggleInteractiveState();
   * Begins auto-incrementing if the value is already at the upper bound.
   *
   * @param {Event} e
   *        KeyDown event.
   */
  onKeyDown(e) {
    const inputType = e.target.type;

    if ([
      KeyCodes.DOM_VK_UP,
      KeyCodes.DOM_VK_DOWN,
      KeyCodes.DOM_VK_RIGHT,
      KeyCodes.DOM_VK_LEFT
    ].includes(e.keyCode)) {
      this.toggleInteractiveState(true);
    }

    // Begin auto-incrementing if the value is already at the upper bound
    // and the user gesture requests a higher value.
    if (this.isAtUpperBound(this.props.value)) {
      if ((inputType === "range" &&
            e.keyCode === KeyCodes.DOM_VK_UP || e.keyCode === KeyCodes.DOM_VK_RIGHT) ||
          (inputType === "number" &&
            e.keyCode === KeyCodes.DOM_VK_UP)) {
        this.startAutoIncrement();
      }
    }
  }

  onKeyUp(e) {
    if ([
      KeyCodes.DOM_VK_UP,
      KeyCodes.DOM_VK_DOWN,
      KeyCodes.DOM_VK_RIGHT,
      KeyCodes.DOM_VK_LEFT
    ].includes(e.keyCode)) {
      this.toggleInteractiveState(false);
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

  /**
   * Handler for "keydown" events from the sider and input fields.
   * Toggles on the "interactive" state. @See toggleInteractiveState();
   * Begins auto-incrementing if the value is already at the upper bound.
   *
   * @param {Event} e
   *        MouseDown event.
   */
  onMouseDown(e) {
    // Begin auto-incrementing if the value is already at the upper bound.
    if (this.isAtUpperBound(this.props.value) && e.target.type === "range") {
      this.startAutoIncrement();
    }
    this.toggleInteractiveState(true);
  }

  onMouseUp(e) {
    this.toggleInteractiveState(false);
  }

  startAutoIncrement() {
    // Do not set auto-increment interval if not allowed to or if one is already set.
    if (!this.props.allowAutoIncrement || this.interval) {
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
   * Calls the given `onChange` callback with the current property, value and unit.
   * Updates the internal state with the current value which will be used while
   * interactive to prevent jittering when receiving debounced props from outside.
   *
   * @param {Number} value
   *        Numeric property value.
   */
  updateValue(value) {
    this.props.onChange(this.props.name, value, this.props.unit);
    this.setState((prevState) => {
      return {
        ...prevState,
        value
      };
    });
  }

  render() {
    // Guard against bad axis data.
    if (this.props.min === this.props.max) {
      return null;
    }

    const defaults = {
      min: this.props.min,
      max: this.props.max,
      onBlur: this.onBlur,
      onChange: this.onChange,
      onKeyUp: this.onKeyUp,
      onKeyDown: this.onKeyDown,
      step: this.props.step || 1,
      // While interacting with the slider or numeric stepper, prevent updating value from
      // outside props which may be debounced and could cause jitter when rendering.
      value: this.state.interactive
        ? this.state.value
        : this.props.value || this.props.defaultValue,
    };

    const range = dom.input(
      {
        ...defaults,
        onMouseDown: this.onMouseDown,
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
        name: this.props.name,
        className: "font-value-input",
        type: "number",
      }
    );

    let unitDropdown = null;
    if (this.props.showUnit) {
      // Ensure the dropdown has the current unit type even if we don't recognize it.
      // The unit conversion function will use a 1-to-1 scale for unrecognized units.
      const options = UNITS.includes(this.props.unit) ?
        UNITS
        :
        UNITS.concat([this.props.unit]);

      unitDropdown = dom.select(
        {
          className: "font-unit-select",
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

    return dom.label(
      {
        className: "font-control",
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
        this.props.showUnit && unitDropdown
      )
    );
  }
}

module.exports = FontPropertyValue;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { toFixed } = require("../utils/font-utils");

class FontPropertyValue extends PureComponent {
  static get propTypes() {
    return {
      // Whether to allow input values above the value defined by the `max` prop.
      allowOverflow: PropTypes.bool,
      // Whether to allow input values below the value defined by the `min` prop.
      allowUnderflow: PropTypes.bool,
      className: PropTypes.string,
      defaultValue: PropTypes.number,
      disabled: PropTypes.bool.isRequired,
      label: PropTypes.string.isRequired,
      min: PropTypes.number.isRequired,
      // Whether to show the `min` prop value as a label.
      minLabel: PropTypes.bool,
      max: PropTypes.number.isRequired,
      // Whether to show the `max` prop value as a label.
      maxLabel: PropTypes.bool,
      name: PropTypes.string.isRequired,
      // Whether to show the `name` prop value as an extra label (used to show axis tags).
      nameLabel: PropTypes.bool,
      onChange: PropTypes.func.isRequired,
      step: PropTypes.number,
      // Whether to show the value input field.
      showInput: PropTypes.bool,
      // Whether to show the unit select dropdown.
      showUnit: PropTypes.bool,
      unit: PropTypes.string,
      unitOptions: PropTypes.array,
      value: PropTypes.number,
      valueLabel: PropTypes.string,
    };
  }

  static get defaultProps() {
    return {
      allowOverflow: false,
      allowUnderflow: false,
      className: "",
      minLabel: false,
      maxLabel: false,
      nameLabel: false,
      step: 1,
      showInput: true,
      showUnit: true,
      unit: null,
      unitOptions: [],
    };
  }

  constructor(props) {
    super(props);
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

    this.onBlur = this.onBlur.bind(this);
    this.onChange = this.onChange.bind(this);
    this.onFocus = this.onFocus.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);
    this.onUnitChange = this.onUnitChange.bind(this);
  }

  /**
   * Given a `prop` key found on the component's props, check the matching `propLabel`.
   * If `propLabel` is true, return the `prop` value; Otherwise, return null.
   *
   * @param {String} prop
   *        Key found on the component's props.
   * @return {Number|null}
   */
  getPropLabel(prop) {
    const label = this.props[`${prop}Label`];
    // Decimal count used to limit numbers in labels.
    const decimals = Math.abs(Math.log10(this.props.step));

    return label ? toFixed(this.props[prop], decimals) : null;
  }

  /**
   * Check if the given value is valid according to the constraints of this component.
   * Ensure it is a number and that it does not go outside the min/max limits, unless
   * allowed by the `allowOverflow` and `allowUnderflow` props.
   *
   * @param  {Number} value
   *         Numeric value
   * @return {Boolean}
   *         Whether the value conforms to the components contraints.
   */
  isValueValid(value) {
    const { allowOverflow, allowUnderflow, min, max } = this.props;

    if (typeof value !== "number" || isNaN(value)) {
      return false;
    }

    // Ensure it does not go below minimum value, unless underflow is allowed.
    if (min !== undefined && value < min && !allowUnderflow) {
      return false;
    }

    // Ensure it does not go over maximum value, unless overflow is allowed.
    if (max !== undefined && value > max && !allowOverflow) {
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
    // Regular expresion to check for floating point or integer numbers. Accept negative
    // numbers only if the min value is negative. Otherwise, expect positive numbers.
    // Whitespace and non-digit characters are invalid (aside from a single dot).
    const regex = (this.props.min && this.props.min < 0)
      ? /^-?[0-9]+(.[0-9]+)?$/
      : /^[0-9]+(.[0-9]+)?$/;
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

  onUnitChange(e) {
    this.props.onChange(this.props.name, this.props.value, this.props.unit,
       e.target.value);
    // Reset internal state value and wait for converted value from props.
    this.setState((prevState) => {
      return {
        ...prevState,
        value: null,
      };
    });
  }

  onMouseDown() {
    this.toggleInteractiveState(true);
  }

  onMouseUp() {
    this.toggleInteractiveState(false);
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
        interactive: isInteractive,
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
        value,
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
        disabled: this.props.disabled,
        onChange: this.onUnitChange,
        value: this.props.unit,
      },
      options.map(unit => {
        return dom.option(
          {
            key: unit,
            value: unit,
          },
          unit
        );
      })
    );
  }

  renderLabelContent() {
    const {
      label,
      name,
      nameLabel,
    } = this.props;

    const labelEl = dom.span(
      {
        className: "font-control-label-text",
        "aria-describedby": nameLabel ? `detail-${name}` : null,
      },
      label
    );

    // Show the `name` prop value as an additional label if the `nameLabel` prop is true.
    const detailEl = nameLabel ?
      dom.span(
        {
          className: "font-control-label-detail",
          id: `detail-${name}`,
        },
        this.getPropLabel("name")
      )
      :
      null;

    return createElement(Fragment, null, labelEl, detailEl);
  }

  renderValueLabel() {
    if (!this.props.valueLabel) {
      return null;
    }

    return dom.div({ className: "font-value-label" }, this.props.valueLabel);
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
        onMouseDown: this.onMouseDown,
        onMouseUp: this.onMouseUp,
        className: "font-value-slider",
        disabled: this.props.disabled,
        name: this.props.name,
        title: this.props.label,
        type: "range",
      }
    );

    const input = dom.input(
      {
        ...defaults,
        // Remove lower limit from number input if it is allowed to underflow.
        min: this.props.allowUnderflow ? null : this.props.min,
        // Remove upper limit from number input if it is allowed to overflow.
        max: this.props.allowOverflow ? null : this.props.max,
        name: this.props.name,
        className: "font-value-input",
        disabled: this.props.disabled,
        type: "number",
      }
    );

    return dom.label(
      {
        className: `font-control ${this.props.className}`,
        disabled: this.props.disabled,
      },
      dom.div(
        {
          className: "font-control-label",
          title: this.props.label,
        },
        this.renderLabelContent()
      ),
      dom.div(
        {
          className: "font-control-input",
        },
        dom.div(
          {
            className: "font-value-slider-container",
            "data-min": this.getPropLabel("min"),
            "data-max": this.getPropLabel("max"),
          },
          range
        ),
        this.renderValueLabel(),
        this.props.showInput && input,
        this.props.showUnit && this.renderUnitSelect()
      )
    );
  }
}

module.exports = FontPropertyValue;

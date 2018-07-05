/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const UNITS = ["px", "%", "em", "rem", "vh", "vw", "pt"];

class FontPropertyValue extends PureComponent {
  static get propTypes() {
    return {
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
    this.state = {
      // Whether the user is dragging the slider thumb or pressing on the numeric stepper.
      interactive: false
    };
    this.onChange = this.onChange.bind(this);
    this.onMouseDown = this.onMouseDown.bind(this);
    this.onMouseUp = this.onMouseUp.bind(this);
    this.onUnitChange = this.onUnitChange.bind(this);
  }

  onChange(e) {
    this.props.onChange(this.props.name, e.target.value, this.props.unit);
    const value = e.target.value;
    this.setState((prevState) => {
      return { ...prevState, value };
    });
  }

  onUnitChange(e) {
    this.props.onChange(this.props.name, this.props.value, this.props.unit,
       e.target.value);
  }

  onMouseDown(e) {
    this.setState((prevState, props) => {
      return { ...prevState, interactive: true, value: props.value };
    });
  }

  onMouseUp(e) {
    this.setState((prevState, props) => {
      return { ...prevState, interactive: false, value: props.value };
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
      onChange: this.onChange,
      onMouseDown: this.onMouseDown,
      onMouseUp: this.onMouseUp,
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

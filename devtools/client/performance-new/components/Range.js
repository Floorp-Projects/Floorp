/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { div, input, label } = require("devtools/client/shared/vendor/react-dom-factories");

class Range extends PureComponent {
  static get propTypes() {
    return {
      value: PropTypes.number.isRequired,
      label: PropTypes.string.isRequired,
      id: PropTypes.string.isRequired,
      scale: PropTypes.object.isRequired,
      onChange: PropTypes.func.isRequired,
      display: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.handleInput = this.handleInput.bind(this);
  }

  handleInput(e) {
    e.preventDefault();
    const { scale, onChange } = this.props;
    const frac = e.target.value / 100;
    onChange(scale.fromFractionToSingleDigitValue(frac));
  }

  render() {
    const { label: labelText, scale, id, value, display } = this.props;
    return (
      div(
        { className: "perf-settings-row" },
        label(
          {
            className: "perf-settings-label",
            htmlFor: id
          },
          labelText
        ),
        div(
          { className: "perf-settings-value" },
          div(
            { className: "perf-settings-range-input" },
            input({
              type: "range",
              className: `perf-settings-range-input-el`,
              min: "0",
              max: "100",
              value: scale.fromValueToFraction(value) * 100,
              onChange: this.handleInput,
              id,
            })
          ),
          div(
            { className: `perf-settings-range-value`},
            display(value)
          )
        )
      )
    );
  }
}

module.exports = Range;

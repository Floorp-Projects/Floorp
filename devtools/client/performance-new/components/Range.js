/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// @ts-check

/**
 * @typedef {import("../@types/perf").ScaleFunctions} ScaleFunctions
 */

/**
 * @typedef {Object} Props
 * @property {number} value
 * @property {React.ReactNode} label
 * @property {string} id
 * @property {ScaleFunctions} scale
 * @property {(value: number) => unknown} onChange
 * @property {(value: number) => React.ReactNode} display
 */
"use strict";
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  div,
  input,
  label,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * Provide a numeric range slider UI that works off of custom numeric scales.
 * @extends React.PureComponent<Props>
 */
class Range extends PureComponent {
  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  handleInput = event => {
    event.preventDefault();
    const { scale, onChange } = this.props;
    const frac = Number(event.target.value) / scale.steps;
    onChange(scale.fromFractionToSingleDigitValue(frac));
  };

  render() {
    const { label: labelText, scale, id, value, display } = this.props;

    const min = "0";
    const max = scale.steps;
    // Convert the value to the current range.
    const rangeValue = scale.fromValueToFraction(value) * max;

    return div(
      { className: "perf-settings-range-row" },
      label(
        {
          className: "perf-settings-label",
          htmlFor: id,
        },
        labelText
      ),
      input({
        type: "range",
        className: `perf-settings-range-input`,
        min,
        "aria-valuemin": scale.fromFractionToValue(0),
        max,
        "aria-valuemax": scale.fromFractionToValue(1),
        value: rangeValue,
        "aria-valuenow": value,
        onChange: this.handleInput,
        id,
      }),
      div({ className: `perf-settings-range-value` }, display(value))
    );
  }
}

module.exports = Range;

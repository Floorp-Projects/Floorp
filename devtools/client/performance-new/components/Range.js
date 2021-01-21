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
  /** @param {Props} props */
  constructor(props) {
    super(props);
    this.handleInput = this.handleInput.bind(this);
  }

  /**
   * @param {React.ChangeEvent<HTMLInputElement>} event
   */
  handleInput(event) {
    event.preventDefault();
    const { scale, onChange } = this.props;
    const frac = Number(event.target.value) / 100;
    onChange(scale.fromFractionToSingleDigitValue(frac));
  }

  render() {
    const { label: labelText, scale, id, value, display } = this.props;
    return div(
      { className: "perf-settings-row" },
      label(
        {
          className: "perf-settings-label",
          htmlFor: id,
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
        div({ className: `perf-settings-range-value` }, display(value))
      )
    );
  }
}

module.exports = Range;

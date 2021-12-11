/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPropertyValue = createFactory(
  require("devtools/client/inspector/fonts/components/FontPropertyValue")
);

const Types = require("devtools/client/inspector/fonts/types");

class FontAxis extends PureComponent {
  static get propTypes() {
    return {
      axis: PropTypes.shape(Types.fontVariationAxis),
      disabled: PropTypes.bool.isRequired,
      onChange: PropTypes.func.isRequired,
      value: PropTypes.number.isRequired,
    };
  }

  /**
   * Naive implementation to get increment step for variable font axis that ensures
   * fine grained control based on range of values between min and max.
   *
   * @param  {Number} min
   *         Minumum value for range.
   * @param  {Number} max
   *         Maximum value for range.
   * @return {Number}
   *         Step value used in range input for font axis.
   */
  getAxisStep(min, max) {
    let step = 1;
    const delta = parseInt(max, 10) - parseInt(min, 10);

    if (delta <= 1) {
      step = 0.001;
    } else if (delta <= 10) {
      step = 0.01;
    } else if (delta <= 100) {
      step = 0.1;
    }

    return step;
  }

  render() {
    const { axis, value, onChange } = this.props;

    return FontPropertyValue({
      className: "font-control-axis",
      disabled: this.props.disabled,
      label: axis.name,
      min: axis.minValue,
      minLabel: true,
      max: axis.maxValue,
      maxLabel: true,
      name: axis.tag,
      nameLabel: true,
      onChange,
      step: this.getAxisStep(axis.minValue, axis.maxValue),
      value,
    });
  }
}

module.exports = FontAxis;

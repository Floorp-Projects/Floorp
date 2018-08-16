/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPropertyValue = createFactory(require("./FontPropertyValue"));

const { getStr } = require("../utils/l10n");
const { getUnitFromValue, getStepForUnit } = require("../utils/font-utils");

class LineHeight extends PureComponent {
  static get propTypes() {
    return {
      onChange: PropTypes.func.isRequired,
      value: PropTypes.string.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.historicMax = {};
  }

  render() {
    const value = parseFloat(this.props.value);
    // When values for line-height are be unitless, getUnitFromValue() returns null.
    // In that case, set the unit to an empty string for special treatment in conversion.
    const unit = getUnitFromValue(this.props.value) || "";
    let max;
    switch (unit) {
      case "":
      case "em":
      case "rem":
        max = 2;
        break;
      case "vh":
      case "vw":
      case "vmin":
      case "vmax":
        max = 15;
        break;
      case "%":
        max = 200;
        break;
      default:
        max = 108;
        break;
    }

    // Allow the upper bound to increase so it accomodates the out-of-bounds value.
    max = Math.max(max, value);
    // Ensure we store the max value ever reached for this unit type. This will be the
    // max value of the input and slider. Without this memoization, the value and slider
    // thumb get clamped at the upper bound while decrementing an out-of-bounds value.
    this.historicMax[unit] = this.historicMax[unit]
      ? Math.max(this.historicMax[unit], max)
      : max;

    return FontPropertyValue({
      allowAutoIncrement: true,
      label: getStr("fontinspector.lineHeightLabel"),
      min: 0,
      max: this.historicMax[unit],
      name: "line-height",
      onChange: this.props.onChange,
      step: getStepForUnit(unit),
      unit,
      unitOptions: ["", "em", "%", "px"],
      value,
    });
  }
}

module.exports = LineHeight;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPropertyValue = createFactory(require("./FontPropertyValue"));

const { getStr } = require("../utils/l10n");
const { getUnitFromValue, getStepForUnit } = require("../utils/font-utils");

class FontSize extends PureComponent {
  static get propTypes() {
    return {
      onChange: PropTypes.func.isRequired,
      value: PropTypes.string.isRequired,
    };
  }

  render() {
    const unit = getUnitFromValue(this.props.value);
    let max;
    switch (unit) {
      case "em":
      case "rem":
        max = 10;
        break;
      case "vh":
      case "vw":
      case "vmin":
      case "vmax":
        max = 100;
        break;
      case "%":
        max = 500;
        break;
      default:
        max = 300;
        break;
    }

    return FontPropertyValue({
      label: getStr("fontinspector.fontSizeLabel"),
      min: 0,
      max,
      name: "font-size",
      onChange: this.props.onChange,
      showUnit: true,
      step: getStepForUnit(unit),
      unit,
      value: parseFloat(this.props.value),
    });
  }
}

module.exports = FontSize;

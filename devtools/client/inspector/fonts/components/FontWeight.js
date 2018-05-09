/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPropertyValue = createFactory(require("./FontPropertyValue"));

const { getStr } = require("../utils/l10n");

class FontWeight extends PureComponent {
  static get propTypes() {
    return {
      onChange: PropTypes.func.isRequired,
      value: PropTypes.string.isRequired,
    };
  }

  render() {
    return FontPropertyValue({
      label: getStr("fontinspector.fontWeightLabel"),
      min: 100,
      max: 900,
      name: "font-weight",
      onChange: this.props.onChange,
      step: 100,
      unit: null,
      value: parseFloat(this.props.value),
    });
  }
}

module.exports = FontWeight;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { PureComponent } = require("devtools/client/shared/vendor/react");
const {
  tr,
  td,
  th,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * This component
 */
class ManifestItemText extends PureComponent {
  static get propTypes() {
    return {
      name: PropTypes.string.isRequired,
      val: PropTypes.string.isRequired,
    };
  }
  render() {
    const { name, val } = this.props;

    return tr(
      { className: "manifest__row" },
      th({ className: "manifest__col-label", scope: "row" }, name),
      td({ className: "manifest__col-value" }, val)
    );
  }
}

// Exports
module.exports = ManifestItemText;

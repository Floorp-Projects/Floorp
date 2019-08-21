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
class ManifestItemIcon extends PureComponent {
  static get propTypes() {
    return {
      icon: PropTypes.object.isRequired,
    };
  }
  render() {
    const { icon } = this.props;

    return tr(
      { className: "manifest-view__row" },
      th({ className: "manifest-view__col-label", scope: "row" }, icon.size),
      td({ className: "manifest-view__col-value" }, icon.src)
    );
  }
}

// Exports
module.exports = ManifestItemIcon;

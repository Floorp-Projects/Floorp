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
  img,
} = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * This component
 */
class ManifestItemWarning extends PureComponent {
  static get propTypes() {
    return {
      warning: PropTypes.object.isRequired,
    };
  }
  render() {
    const { warning } = this.props;

    return tr(
      { className: "manifest__row manifest__row-error" },
      th(
        { className: "manifest__col-label", scope: "row" },
        img({
          src: "chrome://global/skin/icons/warning.svg",
          alt: "Warning icon",
        })
      ),
      td({ className: "manifest__col-value" }, warning.warn)
    );
  }
}

// Exports
module.exports = ManifestItemWarning;

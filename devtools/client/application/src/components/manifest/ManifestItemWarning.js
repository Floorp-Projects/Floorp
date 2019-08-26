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
 * A Manifest warning validation message
 */
class ManifestItemWarning extends PureComponent {
  // TODO: this probably should not be a table, but a list. It might also make
  // more sense to rename to ManifestIssue. Address in:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1575872

  static get propTypes() {
    // TODO: pass multiple props instead of just a single object
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1575872
    return {
      warning: PropTypes.object.isRequired,
    };
  }

  render() {
    const { warning } = this.props;
    return tr(
      { className: "manifest-warning" },
      th(
        { scope: "row" },
        img({
          // TODO: Add a localized string in
          // https://bugzilla.mozilla.org/show_bug.cgi?id=1575872
          alt: "Warning icon",
          src: "chrome://global/skin/icons/warning.svg",
        })
      ),
      td({}, warning.warn)
    );
  }
}

// Exports
module.exports = ManifestItemWarning;

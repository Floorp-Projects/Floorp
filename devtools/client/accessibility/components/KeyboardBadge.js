/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// React
const {
  createFactory,
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

const {
  L10N,
} = require("resource://devtools/client/accessibility/utils/l10n.js");

const {
  accessibility: {
    SCORES: { BEST_PRACTICES, FAIL, WARNING },
  },
} = require("resource://devtools/shared/constants.js");

loader.lazyGetter(this, "Badge", () =>
  createFactory(
    require("resource://devtools/client/accessibility/components/Badge.js")
  )
);

/**
 * Component for rendering a badge for keyboard accessibliity check failures
 * association with a given accessibility object in the accessibility tree.
 */
class KeyboardBadge extends PureComponent {
  static get propTypes() {
    return {
      error: PropTypes.string,
      score: PropTypes.string,
    };
  }

  render() {
    const { error, score } = this.props;
    if (error || ![BEST_PRACTICES, FAIL, WARNING].includes(score)) {
      return null;
    }

    return Badge({
      score,
      label: L10N.getStr("accessibility.badge.keyboard"),
      tooltip: L10N.getStr("accessibility.badge.keyboard.tooltip"),
    });
  }
}

module.exports = KeyboardBadge;

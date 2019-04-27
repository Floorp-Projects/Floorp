/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { L10N } = require("../utils/l10n");

const { accessibility: { SCORES } } = require("devtools/shared/constants");

loader.lazyGetter(this, "Badge", () => createFactory(require("./Badge")));

const { FILTERS } = require("../constants");

/**
* Component for rendering a badge for contrast accessibliity check
* failures association with a given accessibility object in the accessibility
* tree.
*/

class ContrastBadge extends Component {
  static get propTypes() {
    return {
      error: PropTypes.string,
      score: PropTypes.string,
      walker: PropTypes.object.isRequired,
    };
  }

  render() {
    const { error, score, walker } = this.props;
    if (error) {
      return null;
    }

    if (score !== SCORES.FAIL) {
      return null;
    }

    return Badge({
      label: L10N.getStr("accessibility.badge.contrast"),
      tooltip: L10N.getStr("accessibility.badge.contrast.tooltip"),
      filterKey: FILTERS.CONTRAST,
      walker,
    });
  }
}

module.exports = ContrastBadge;

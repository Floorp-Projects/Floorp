/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component } = require("devtools/client/shared/vendor/react");
const { span } = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class Badge extends Component {
  static get propTypes() {
    return {
      label: PropTypes.string.isRequired,
      ariaLabel: PropTypes.string,
      tooltip: PropTypes.string,
    };
  }

  render() {
    const { label, ariaLabel, tooltip } = this.props;

    return span(
      {
        className: "audit-badge badge",
        title: tooltip,
        "aria-label": ariaLabel || label,
      },
      label
    );
  }
}

module.exports = Badge;

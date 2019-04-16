/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const ToggleButton = createFactory(require("./Button").ToggleButton);

class Badge extends Component {
  static get propTypes() {
    return {
      label: PropTypes.string.isRequired,
      tooltip: PropTypes.string,
    };
  }

  render() {
    const { label, tooltip } = this.props;

    return ToggleButton({
      className: "audit-badge badge",
      label,
      tooltip,
    });
  }
}

module.exports = Badge;

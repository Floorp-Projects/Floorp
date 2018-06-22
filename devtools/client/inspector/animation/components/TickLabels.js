/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * This component is intended to show tick labels on the header.
 */
class TickLabels extends PureComponent {
  static get propTypes() {
    return {
      ticks: PropTypes.array.isRequired,
    };
  }

  render() {
    const { ticks } = this.props;

    return dom.div(
      {
        className: "tick-labels"
      },
      ticks.map(tick =>
        dom.div(
          {
            className: "tick-label",
            style: { marginInlineStart: `${ tick.position }%` },
          },
          tick.label
        )
      )
    );
  }
}

module.exports = TickLabels;

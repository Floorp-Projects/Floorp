/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");

/**
 * This component is intended to show the tick line as the background.
 */
class TickLines extends PureComponent {
  static get propTypes() {
    return {
      ticks: PropTypes.array.isRequired,
    };
  }

  render() {
    const { ticks } = this.props;

    return dom.div(
      {
        className: "tick-lines",
      },
      ticks.map(tick =>
        dom.div({
          className: "tick-line",
          style: { marginInlineStart: `${tick.position}%` },
        })
      )
    );
  }
}

module.exports = TickLines;

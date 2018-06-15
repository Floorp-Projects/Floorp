/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

class KeyframesProgressTickItem extends PureComponent {
  static get propTypes() {
    return {
      direction: PropTypes.string.isRequired,
      position: PropTypes.number.isRequired,
      progressTickLabel: PropTypes.string.isRequired,
    };
  }

  render() {
    const {
      direction,
      position,
      progressTickLabel,
    } = this.props;

    return dom.div(
      {
        className: `keyframes-progress-tick-item ${ direction }`,
        style: { [direction]: `${ position }%` }
      },
      progressTickLabel
    );
  }
}

module.exports = KeyframesProgressTickItem;

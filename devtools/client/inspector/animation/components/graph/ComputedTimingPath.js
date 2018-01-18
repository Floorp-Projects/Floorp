/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

class ComputedTimingPath extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      durationPerPixel: PropTypes.number.isRequired,
      keyframes: PropTypes.object.isRequired,
      totalDisplayedDuration: PropTypes.number.isRequired,
    };
  }

  render() {
    return dom.g({});
  }
}

module.exports = ComputedTimingPath;

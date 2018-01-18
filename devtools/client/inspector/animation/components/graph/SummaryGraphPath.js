/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

class SummaryGraphPath extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const {
      animation,
      timeScale,
    } = this.props;

    const totalDisplayedDuration = animation.state.playbackRate * timeScale.getDuration();
    const startTime = timeScale.minStartTime;

    return dom.svg(
      {
        className: "animation-summary-graph-path",
        preserveAspectRatio: "none",
        viewBox: `${ startTime } -1 ${ totalDisplayedDuration } 1`
      }
    );
  }
}

module.exports = SummaryGraphPath;

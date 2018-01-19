/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const NegativePath = require("./NegativePath");

class NegativeEndDelayPath extends NegativePath {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      durationPerPixel: PropTypes.number.isRequired,
      keyframes: PropTypes.object.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      totalDuration: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    props.className = "animation-negative-end-delay-path";
    super(props);
  }

  renderGraph(state, helper) {
    const endTime = state.delay + state.iterationCount * state.duration;
    const startTime = endTime + state.endDelay;
    const segments = helper.createPathSegments(startTime, endTime);

    return dom.path(
      {
        d: helper.toPathString(segments),
      }
    );
  }
}

module.exports = NegativeEndDelayPath;

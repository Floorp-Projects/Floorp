/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { SummaryGraphHelper, toPathString } = require("../../utils/graph-helper");
const TimingPath = require("./TimingPath");

class EffectTimingPath extends TimingPath {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      durationPerPixel: PropTypes.number.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      totalDuration: PropTypes.number.isRequired,
    };
  }

  render() {
    const {
      animation,
      durationPerPixel,
      simulateAnimation,
      totalDuration,
    } = this.props;

    const { state } = animation;
    const effectTiming = Object.assign({}, state, {
      iterations: state.iterationCount ? state.iterationCount : Infinity
    });

    const simulatedAnimation = simulateAnimation(null, effectTiming, false);
    const endTime = simulatedAnimation.effect.getComputedTiming().endTime;

    const getValueFunc = time => {
      if (time < 0) {
        return { x: time, y: 0 };
      }

      simulatedAnimation.currentTime = time < endTime ? time : endTime;
      return Math.max(simulatedAnimation.effect.getComputedTiming().progress, 0);
    };

    const toPathStringFunc = segments => {
      const firstSegment = segments[0];
      let pathString = `M${ firstSegment.x },0 `;
      pathString += toPathString(segments);
      const lastSegment = segments[segments.length - 1];
      pathString += `L${ lastSegment.x },0`;
      return pathString;
    };

    const helper = new SummaryGraphHelper(state, null,
                                          totalDuration, durationPerPixel,
                                          getValueFunc, toPathStringFunc);
    const offset = state.previousStartTime ? state.previousStartTime : 0;

    return dom.g(
      {
        className: "animation-effect-timing-path",
        transform: `translate(${ offset })`
      },
      super.renderGraph(state, helper)
    );
  }
}

module.exports = EffectTimingPath;

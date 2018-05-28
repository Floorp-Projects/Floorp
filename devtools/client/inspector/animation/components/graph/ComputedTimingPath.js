/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { SummaryGraphHelper, toPathString } = require("../../utils/graph-helper");
const TimingPath = require("./TimingPath");

class ComputedTimingPath extends TimingPath {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      durationPerPixel: PropTypes.number.isRequired,
      keyframes: PropTypes.object.isRequired,
      offset: PropTypes.number.isRequired,
      opacity: PropTypes.number.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      totalDuration: PropTypes.number.isRequired,
    };
  }

  render() {
    const {
      animation,
      durationPerPixel,
      keyframes,
      offset,
      opacity,
      simulateAnimation,
      totalDuration,
    } = this.props;

    const { state } = animation;
    const effectTiming = Object.assign({}, state, {
      iterations: state.iterationCount ? state.iterationCount : Infinity
    });

    // Create new keyframes for opacity as computed style.
    // The reason why we use computed value instead of computed timing progress is to
    // include the easing in keyframes as well. Although the computed timing progress
    // is not affected by the easing in keyframes at all, computed value reflects that.
    const frames = keyframes.map(keyframe => {
      return {
        opacity: keyframe.offset,
        offset: keyframe.offset,
        easing: keyframe.easing
      };
    });

    const simulatedAnimation = simulateAnimation(frames, effectTiming, true);

    if (!simulatedAnimation) {
      return null;
    }

    const simulatedElement = simulatedAnimation.effect.target;
    const win = simulatedElement.ownerGlobal;
    const endTime = simulatedAnimation.effect.getComputedTiming().endTime;

    // Set the underlying opacity to zero so that if we sample the animation's output
    // during the delay phase and it is not filling backwards, we get zero.
    simulatedElement.style.opacity = 0;

    const getValueFunc = time => {
      if (time < 0) {
        return { x: time, y: 0 };
      }

      simulatedAnimation.currentTime = time < endTime ? time : endTime;
      return win.getComputedStyle(simulatedElement).opacity;
    };

    const toPathStringFunc = segments => {
      const firstSegment = segments[0];
      let pathString = `M${ firstSegment.x },0 `;
      pathString += toPathString(segments);
      const lastSegment = segments[segments.length - 1];
      pathString += `L${ lastSegment.x },0 Z`;
      return pathString;
    };

    const helper = new SummaryGraphHelper(state, keyframes,
                                          totalDuration, durationPerPixel,
                                          getValueFunc, toPathStringFunc);

    return dom.g(
      {
        className: "animation-computed-timing-path",
        style: { opacity },
        transform: `translate(${ offset })`
      },
      super.renderGraph(state, helper)
    );
  }
}

module.exports = ComputedTimingPath;

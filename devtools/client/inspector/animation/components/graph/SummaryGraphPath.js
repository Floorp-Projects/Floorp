/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const ReactDOM = require("devtools/client/shared/vendor/react-dom");

const ComputedTimingPath = createFactory(require("./ComputedTimingPath"));
const EffectTimingPath = createFactory(require("./EffectTimingPath"));
const NegativeDelayPath = createFactory(require("./NegativeDelayPath"));
const { DEFAULT_GRAPH_HEIGHT } = require("../../utils/graph-helper");

// Minimum opacity for semitransparent fill color for keyframes's easing graph.
const MIN_KEYFRAMES_EASING_OPACITY = 0.5;

class SummaryGraphPath extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      durationPerPixel: 0,
    };
  }

  componentDidMount() {
    this.updateDurationPerPixel();
  }

  /**
   * Return animatable keyframes list which has only offset and easing.
   * Also, this method remove duplicate keyframes.
   * For example, if the given animatedPropertyMap is,
   * [
   *   {
   *     key: "color",
   *     values: [
   *       {
   *         offset: 0,
   *         easing: "ease",
   *         value: "rgb(255, 0, 0)",
   *       },
   *       {
   *         offset: 1,
   *         value: "rgb(0, 255, 0)",
   *       },
   *     ],
   *   },
   *   {
   *     key: "opacity",
   *     values: [
   *       {
   *         offset: 0,
   *         easing: "ease",
   *         value: 0,
   *       },
   *       {
   *         offset: 1,
   *         value: 1,
   *       },
   *     ],
   *   },
   * ]
   *
   * then this method returns,
   * [
   *   [
   *     {
   *       offset: 0,
   *       easing: "ease",
   *     },
   *     {
   *       offset: 1,
   *     },
   *   ],
   * ]
   *
   * @param {Map} animated property map
   *        which can get form getAnimatedPropertyMap in animation.js
   * @return {Array} list of keyframes which has only easing and offset.
   */
  getOffsetAndEasingOnlyKeyframes(animatedPropertyMap) {
    return [...animatedPropertyMap.values()].filter((keyframes1, i, self) => {
      return i !== self.findIndex((keyframes2, j) => {
        return this.isOffsetAndEasingKeyframesEqual(keyframes1, keyframes2) ? j : -1;
      });
    }).map(keyframes => {
      return keyframes.map(keyframe => {
        return { easing: keyframe.easing, offset: keyframe.offset };
      });
    });
  }

  getTotalDuration(animation, timeScale) {
    return animation.state.playbackRate * timeScale.getDuration();
  }

  /**
   * Return true if given keyframes have same length, offset and easing.
   *
   * @param {Array} keyframes1
   * @param {Array} keyframes2
   * @return {Boolean} true: equals
   */
  isOffsetAndEasingKeyframesEqual(keyframes1, keyframes2) {
    if (keyframes1.length !== keyframes2.length) {
      return false;
    }

    for (let i = 0; i < keyframes1.length; i++) {
      const keyframe1 = keyframes1[i];
      const keyframe2 = keyframes2[i];

      if (keyframe1.offset !== keyframe2.offset ||
          keyframe1.easing !== keyframe2.easing) {
        return false;
      }
    }

    return true;
  }

  updateDurationPerPixel() {
    const {
      animation,
      timeScale,
    } = this.props;

    const thisEl = ReactDOM.findDOMNode(this);
    const totalDuration = this.getTotalDuration(animation, timeScale);
    const durationPerPixel = totalDuration / thisEl.parentNode.clientWidth;

    this.setState({ durationPerPixel });
  }

  render() {
    const { durationPerPixel } = this.state;

    if (!durationPerPixel) {
      return dom.svg();
    }

    const {
      animation,
      simulateAnimation,
      timeScale,
    } = this.props;

    const totalDuration = this.getTotalDuration(animation, timeScale);
    const startTime = timeScale.minStartTime;
    const keyframesList =
      this.getOffsetAndEasingOnlyKeyframes(animation.animatedPropertyMap);
    const opacity = Math.max(1 / keyframesList.length, MIN_KEYFRAMES_EASING_OPACITY);

    return dom.svg(
      {
        className: "animation-summary-graph-path",
        preserveAspectRatio: "none",
        viewBox: `${ startTime } -${ DEFAULT_GRAPH_HEIGHT } `
                 + `${ totalDuration } ${ DEFAULT_GRAPH_HEIGHT }`,
      },
      keyframesList.map(keyframes =>
        ComputedTimingPath(
          {
            animation,
            durationPerPixel,
            keyframes,
            opacity,
            simulateAnimation,
            totalDuration,
          }
        )
      ),
      animation.state.easing !== "linear" ?
        EffectTimingPath(
          {
            animation,
            durationPerPixel,
            simulateAnimation,
            totalDuration,
          }
        )
      :
      null,
      animation.state.delay < 0 ?
        keyframesList.map(keyframes => {
          return NegativeDelayPath(
            {
              animation,
              durationPerPixel,
              keyframes,
              simulateAnimation,
              totalDuration,
            }
          );
        })
      :
      null
    );
  }
}

module.exports = SummaryGraphPath;

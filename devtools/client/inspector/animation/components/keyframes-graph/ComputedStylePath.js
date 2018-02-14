/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const {
  createPathSegments,
  DEFAULT_DURATION_RESOLUTION,
  getPreferredProgressThresholdByKeyframes,
  toPathString,
} = require("../../utils/graph-helper");

/*
 * This class is an abstraction for computed style path of keyframes.
 * Subclass of this should implement the following methods:
 *
 * getPropertyName()
 *   Returns property name which will be animated.
 *   @return {String}
 *           e.g. opacity
 *
 * getPropertyValue(keyframe)
 *   Returns value which uses as animated keyframe value from given parameter.
 *   @param {Object} keyframe
 *   @return {String||Number}
 *           e.g. 0
 *
 * toSegmentValue(computedStyle)
 *   Convert computed style to segment value of graph.
 *   @param {String||Number}
 *          e.g. 0
 *   @return {Number}
 *          e.g. 0 (should be 0 - 1.0)
 */
class ComputedStylePath extends PureComponent {
  static get propTypes() {
    return {
      componentWidth: PropTypes.number.isRequired,
      graphHeight: PropTypes.number.isRequired,
      simulateAnimation: PropTypes.func.isRequired,
      totalDuration: PropTypes.number.isRequired,
      values: PropTypes.array.isRequired,
    };
  }

  /**
   * Return an array containing the path segments between the given start and
   * end keyframe values.
   *
   * @param {Object} startValue
   *        Starting keyframe.
   * @param {Object} startValue
   *        Ending keyframe.
   * @return {Array}
   *         Array of path segment.
   *         [{x: {Number} time, y: {Number} segment value}, ...]
   */
  getPathSegments(startValue, endValue) {
    const {
      componentWidth,
      simulateAnimation,
      totalDuration,
    } = this.props;

    const propertyName = this.getPropertyName();
    const offsetDistance = endValue.offset - startValue.offset;
    const duration = offsetDistance * totalDuration;

    const keyframes = [startValue, endValue].map((keyframe, index) => {
      return {
        offset: index,
        easing: keyframe.easing,
        [getJsPropertyName(propertyName)]: this.getPropertyValue(keyframe),
      };
    });
    const effect = {
      duration,
      fill: "forwards",
    };
    const simulatedAnimation = simulateAnimation(keyframes, effect, true);
    const simulatedElement = simulatedAnimation.effect.target;
    const win = simulatedElement.ownerGlobal;
    const threshold = getPreferredProgressThresholdByKeyframes(keyframes);

    const getSegment = time => {
      simulatedAnimation.currentTime = time;
      const computedStyle =
        win.getComputedStyle(simulatedElement).getPropertyValue(propertyName);

      return {
        computedStyle,
        x: time,
        y: this.toSegmentValue(computedStyle),
      };
    };

    const segments = createPathSegments(0, duration, duration / componentWidth, threshold,
                                        DEFAULT_DURATION_RESOLUTION, getSegment);
    const offset = startValue.offset * totalDuration;

    for (const segment of segments) {
      segment.x += offset;
    }

    return segments;
  }

  /**
   * Render graph. This method returns React dom.
   *
   * @return {Element}
   */
  renderGraph() {
    const { values } = this.props;

    const segments = [];

    for (let i = 0; i < values.length - 1; i++) {
      const startValue = values[i];
      const endValue = values[i + 1];
      segments.push(...this.getPathSegments(startValue, endValue));
    }

    return this.renderPathSegments(segments);
  }

  /**
   * Return react dom fron given path segments.
   *
   * @param {Array} segments
   * @param {Object} style
   * @return {Element}
   */
  renderPathSegments(segments, style) {
    const { graphHeight } = this.props;

    for (const segment of segments) {
      segment.y *= graphHeight;
    }

    let d = `M${ segments[0].x },0 `;
    d += toPathString(segments);
    d += `L${ segments[segments.length - 1].x },0 Z`;

    return dom.path({ d, style });
  }
}

/**
 * Convert given CSS property name to JavaScript CSS name.
 *
 * @param {String} cssPropertyName
 *        CSS property name (e.g. background-color).
 * @return {String}
 *         JavaScript CSS property name (e.g. backgroundColor).
 */
function getJsPropertyName(cssPropertyName) {
  if (cssPropertyName == "float") {
    return "cssFloat";
  }
  // https://drafts.csswg.org/cssom/#css-property-to-idl-attribute
  return cssPropertyName.replace(/-([a-z])/gi, (str, group) => {
    return group.toUpperCase();
  });
}

module.exports = ComputedStylePath;

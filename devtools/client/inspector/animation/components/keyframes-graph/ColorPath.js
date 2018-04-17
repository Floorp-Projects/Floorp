/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dom = require("devtools/client/shared/vendor/react-dom-factories");

const {colorUtils} = require("devtools/shared/css/color.js");

const ComputedStylePath = require("./ComputedStylePath");

/* Count for linearGradient ID */
let LINEAR_GRADIENT_ID_COUNT = 0;

class ColorPath extends ComputedStylePath {
  constructor(props) {
    super(props);

    this.state = this.propToState(props);
  }

  componentWillReceiveProps(nextProps) {
    this.setState(this.propToState(nextProps));
  }

  getPropertyName() {
    return "color";
  }

  getPropertyValue(keyframe) {
    return keyframe.value;
  }

  propToState({ keyframes }) {
    const maxObject = { distance: 0 };

    for (let i = 0; i < keyframes.length - 1; i++) {
      const value1 = getRGBA(keyframes[i].value);
      for (let j = i + 1; j < keyframes.length; j++) {
        const value2 = getRGBA(keyframes[j].value);
        const distance = getRGBADistance(value1, value2);

        if (maxObject.distance >= distance) {
          continue;
        }

        maxObject.distance = distance;
        maxObject.value1 = value1;
        maxObject.value2 = value2;
      }
    }

    const maxDistance = maxObject.distance;
    const baseValue =
      maxObject.value1 < maxObject.value2 ? maxObject.value1 : maxObject.value2;

    return { baseValue, maxDistance };
  }

  toSegmentValue(computedStyle) {
    const { baseValue, maxDistance } = this.state;
    const value = getRGBA(computedStyle);
    return getRGBADistance(baseValue, value) / maxDistance;
  }

  /**
   * Overide parent's method.
   */
  renderEasingHint() {
    const {
      easingHintStrokeWidth,
      graphHeight,
      keyframes,
      totalDuration,
    } = this.props;

    const hints = [];

    for (let i = 0; i < keyframes.length - 1; i++) {
      const startKeyframe = keyframes[i];
      const endKeyframe = keyframes[i + 1];
      const startTime = startKeyframe.offset * totalDuration;
      const endTime = endKeyframe.offset * totalDuration;

      const g = dom.g(
        {
          className: "hint"
        },
        dom.title({}, startKeyframe.easing),
        dom.rect(
          {
            x: startTime,
            y: -graphHeight,
            height: graphHeight,
            width: endTime - startTime,
          }
        ),
        dom.line(
          {
            x1: startTime,
            y1: -graphHeight,
            x2: endTime,
            y2: -graphHeight,
            style: {
              "stroke-width": easingHintStrokeWidth,
            },
          }
        )
      );
      hints.push(g);
    }

    return hints;
  }

  /**
   * Overide parent's method.
   */
  renderPathSegments(segments) {
    for (const segment of segments) {
      segment.y = 1;
    }

    const lastSegment = segments[segments.length - 1];
    const id = `color-property-${ LINEAR_GRADIENT_ID_COUNT++ }`;
    const path = super.renderPathSegments(segments, { fill: `url(#${ id })` });
    const linearGradient = dom.linearGradient(
      { id },
      segments.map(segment => {
        return dom.stop(
          {
            "stopColor": segment.computedStyle,
            "offset": segment.x / lastSegment.x,
          }
        );
      })
    );

    return [path, linearGradient];
  }

  render() {
    return dom.g(
      {
        className: "color-path",
      },
      super.renderGraph()
    );
  }
}

/**
 * Parse given RGBA string.
 *
 * @param {String} colorString
 *        e.g. rgb(0, 0, 0) or rgba(0, 0, 0, 0.5) and so on.
 * @return {Object}
 *         RGBA {r: r, g: g, b: b, a: a}.
 */
function getRGBA(colorString) {
  const color = new colorUtils.CssColor(colorString);
  return color.getRGBATuple();
}

/**
 * Return the distance from give two RGBA.
 *
 * @param {Object} rgba1
 *        RGBA (format is same to getRGBA)
 * @param {Object} rgba2
 *        RGBA (format is same to getRGBA)
 * @return {Number}
 *         The range is 0 - 1.0.
 */
function getRGBADistance(rgba1, rgba2) {
  const startA = rgba1.a;
  const startR = rgba1.r * startA;
  const startG = rgba1.g * startA;
  const startB = rgba1.b * startA;
  const endA = rgba2.a;
  const endR = rgba2.r * endA;
  const endG = rgba2.g * endA;
  const endB = rgba2.b * endA;
  const diffA = startA - endA;
  const diffR = startR - endR;
  const diffG = startG - endG;
  const diffB = startB - endB;
  return Math.sqrt(diffA * diffA + diffR * diffR + diffG * diffG + diffB * diffB);
}

module.exports = ColorPath;

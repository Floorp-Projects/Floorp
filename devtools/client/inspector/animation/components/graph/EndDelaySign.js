/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class EndDelaySign extends PureComponent {
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
    const {
      createdTime,
      delay,
      duration,
      endDelay,
      fill,
      iterationCount,
      playbackRate,
    } = animation.state;

    const toRate = v => v / playbackRate;
    // If createdTime is not defined (which happens when connected to server older
    // than FF62), use previousStartTime instead. See bug 1454392
    const baseTime = typeof createdTime === "undefined"
                       ? (animation.state.previousStartTime || 0)
                       : createdTime;
    const startTime = baseTime - timeScale.minStartTime;
    const endTime = toRate(delay + duration * iterationCount + Math.min(endDelay, 0));
    const offset = (startTime + endTime) / timeScale.getDuration() * 100;
    const width = Math.abs(toRate(endDelay)) / timeScale.getDuration() * 100;

    return dom.div(
      {
        className: "animation-end-delay-sign" +
                   (endDelay < 0 ? " negative" : "") +
                   (fill === "both" || fill === "forwards" ? " fill" : ""),
        style: {
          width: `${ width }%`,
          left: `${ offset }%`,
        },
      }
    );
  }
}

module.exports = EndDelaySign;

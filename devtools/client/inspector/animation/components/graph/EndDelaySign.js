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
      delay,
      duration,
      fill,
      iterationCount,
      playbackRate,
      previousStartTime = 0,
    } = animation.state;

    const endDelay = animation.state.endDelay / playbackRate;
    const startTime = previousStartTime - timeScale.minStartTime;
    const endTime =
      (duration * iterationCount + delay) / playbackRate + (endDelay < 0 ? endDelay : 0);
    const offset = (startTime + endTime) / timeScale.getDuration() * 100;
    const width = Math.abs(endDelay) / timeScale.getDuration() * 100;

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

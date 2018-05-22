/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class DelaySign extends PureComponent {
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
      fill,
      playbackRate,
    } = animation.state;

    const toRate = v => v / playbackRate;
    // If createdTime is not defined (which happens when connected to server older
    // than FF62), use previousStartTime instead. See bug 1454392
    const baseTime = typeof createdTime === "undefined"
                       ? (animation.state.previousStartTime || 0)
                       : createdTime;
    const startTime = baseTime + toRate(Math.min(delay, 0)) - timeScale.minStartTime;
    const offset = startTime / timeScale.getDuration() * 100;
    const width = Math.abs(toRate(delay)) / timeScale.getDuration() * 100;

    return dom.div(
      {
        className: "animation-delay-sign" +
                   (delay < 0 ? " negative" : "") +
                   (fill === "both" || fill === "backwards" ? " fill" : ""),
        style: {
          width: `${ width }%`,
          left: `${ offset }%`,
        },
      }
    );
  }
}

module.exports = DelaySign;

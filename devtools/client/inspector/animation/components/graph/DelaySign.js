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
      fill,
      playbackRate,
      previousStartTime = 0,
    } = animation.state;

    const delay = animation.state.delay / playbackRate;
    const startTime =
      previousStartTime - timeScale.minStartTime + (delay < 0 ? delay : 0);
    const offset = startTime / timeScale.getDuration() * 100;
    const width = Math.abs(delay) / timeScale.getDuration() * 100;

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

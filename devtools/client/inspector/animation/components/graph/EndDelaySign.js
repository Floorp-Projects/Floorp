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

    const { state } = animation;
    const startTime = (state.previousStartTime || 0) - timeScale.minStartTime;
    const endTime = state.duration * state.iterationCount + state.delay;
    const endDelay = state.endDelay < 0 ? state.endDelay : 0;
    const offset = (startTime + endTime + endDelay) / timeScale.getDuration() * 100;
    const width = Math.abs(state.endDelay) / timeScale.getDuration() * 100;

    const endDelayClass = state.endDelay < 0 ? "negative" : "";
    const fillClass = state.fill === "both" || state.fill === "forwards" ? "fill" : "";

    return dom.div(
      {
        className: `animation-end-delay-sign ${ endDelayClass } ${ fillClass }`,
        style: {
          width: `${ width }%`,
          left: `${ offset }%`,
        },
      }
    );
  }
}

module.exports = EndDelaySign;

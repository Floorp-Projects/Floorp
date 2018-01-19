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

    const { state } = animation;
    const startTime = (state.previousStartTime || 0) - timeScale.minStartTime
                      + (state.delay < 0 ? state.delay : 0);
    const offset = startTime / timeScale.getDuration() * 100;
    const width = Math.abs(state.delay) / timeScale.getDuration() * 100;

    const delayClass = state.delay < 0 ? "negative" : "";
    const fillClass = state.fill === "both" || state.fill === "backwards" ? "fill" : "";

    return dom.div(
      {
        className: `animation-delay-sign ${ delayClass } ${ fillClass }`,
        style: {
          width: `${ width }%`,
          left: `${ offset }%`,
        },
      }
    );
  }
}

module.exports = DelaySign;

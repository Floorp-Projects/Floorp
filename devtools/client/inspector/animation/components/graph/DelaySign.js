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
    const { animation, timeScale } = this.props;
    const { delay, isDelayFilled, startTime } = animation.state.absoluteValues;

    const toPercentage = v => (v / timeScale.getDuration()) * 100;
    const offset = toPercentage(startTime - timeScale.minStartTime);
    const width = toPercentage(Math.abs(delay));

    return dom.div({
      className:
        "animation-delay-sign" +
        (delay < 0 ? " negative" : "") +
        (isDelayFilled ? " fill" : ""),
      style: {
        width: `${width}%`,
        marginInlineStart: `${offset}%`,
      },
    });
  }
}

module.exports = DelaySign;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  PureComponent,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

class EndDelaySign extends PureComponent {
  static get propTypes() {
    return {
      animation: PropTypes.object.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  render() {
    const { animation, timeScale } = this.props;
    const { endDelay, endTime, isEndDelayFilled } =
      animation.state.absoluteValues;

    const toPercentage = v => (v / timeScale.getDuration()) * 100;
    const absEndDelay = Math.abs(endDelay);
    const offset = toPercentage(endTime - absEndDelay - timeScale.minStartTime);
    const width = toPercentage(absEndDelay);

    return dom.div({
      className:
        "animation-end-delay-sign" +
        (endDelay < 0 ? " negative" : "") +
        (isEndDelayFilled ? " fill" : ""),
      style: {
        width: `${width}%`,
        marginInlineStart: `${offset}%`,
      },
    });
  }
}

module.exports = EndDelaySign;

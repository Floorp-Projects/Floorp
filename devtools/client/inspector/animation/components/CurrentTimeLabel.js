/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

class CurrentTimeLabel extends PureComponent {
  static get propTypes() {
    return {
      addAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      removeAnimationsCurrentTimeListener: PropTypes.func.isRequired,
      timeScale: PropTypes.object.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this._ref = createRef();

    const { addAnimationsCurrentTimeListener } = props;
    this.onCurrentTimeUpdated = this.onCurrentTimeUpdated.bind(this);

    addAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  componentWillUnmount() {
    const { removeAnimationsCurrentTimeListener } = this.props;
    removeAnimationsCurrentTimeListener(this.onCurrentTimeUpdated);
  }

  onCurrentTimeUpdated(currentTime) {
    const { timeScale } = this.props;
    const text = formatStopwatchTime(currentTime - timeScale.zeroPositionTime);
    // onCurrentTimeUpdated is bound to requestAnimationFrame.
    // As to update the component too frequently has performance issue if React controlled,
    // update raw component directly. See Bug 1699039.
    this._ref.current.textContent = text;
  }

  render() {
    return dom.label({ className: "current-time-label", ref: this._ref });
  }
}

/**
 * Format a timestamp (in ms) as a mm:ss.mmm string.
 *
 * @param {Number} time
 * @return {String}
 */
function formatStopwatchTime(time) {
  // Format falsy values as 0
  if (!time) {
    return "00:00.000";
  }

  const sign = time < 0 ? "-" : "";
  let milliseconds = parseInt(Math.abs(time % 1000), 10);
  let seconds = parseInt(Math.abs(time / 1000) % 60, 10);
  let minutes = parseInt(Math.abs(time / (1000 * 60)), 10);

  minutes = minutes.toString().padStart(2, "0");
  seconds = seconds.toString().padStart(2, "0");
  milliseconds = milliseconds.toString().padStart(3, "0");
  return `${sign}${minutes}:${seconds}.${milliseconds}`;
}

module.exports = CurrentTimeLabel;

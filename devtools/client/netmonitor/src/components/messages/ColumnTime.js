/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");

/**
 * Renders the "Time" column of a message.
 */
class ColumnTime extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return (
      this.props.item.type !== nextProps.item.type ||
      this.props.item.timeStamp !== nextProps.item.timeStamp
    );
  }

  /**
   * Format a DOMHighResTimeStamp (in microseconds) as HH:mm:ss.SSS
   * @param {number} highResTimeStamp
   */
  formatTime(highResTimeStamp) {
    const date = new Date(highResTimeStamp / 1000);
    const hh = date.getHours().toString().padStart(2, "0");
    const mm = date.getMinutes().toString().padStart(2, "0");
    const ss = date.getSeconds().toString().padStart(2, "0");
    const mmm = date.getMilliseconds().toString().padStart(3, "0");
    return `${hh}:${mm}:${ss}.${mmm}`;
  }

  render() {
    const label = this.formatTime(this.props.item.timeStamp);

    return dom.td(
      {
        className: "message-list-column message-list-time",
        title: label,
      },
      label
    );
  }
}

module.exports = ColumnTime;

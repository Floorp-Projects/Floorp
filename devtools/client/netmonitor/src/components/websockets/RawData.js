/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

/**
 * Shows raw data of a WebSocket frame.
 */
class RawData extends Component {
  static get propTypes() {
    return {
      payload: PropTypes.string.isRequired,
    };
  }

  render() {
    const { payload } = this.props;
    return dom.div(
      {
        className: "ws-frame-rawData-payload",
      },
      payload
    );
  }
}

module.exports = RawData;

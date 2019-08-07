/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Renders the "OpCode" column of a WebSocket frame.
 */
class FrameListColumnOpCode extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.opCode !== nextProps.item.opCode;
  }

  render() {
    const { opCode } = this.props.item;

    return dom.td(
      {
        className: "ws-frames-list-column ws-frames-list-opCode",
        title: opCode,
      },
      opCode
    );
  }
}

module.exports = FrameListColumnOpCode;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Renders the "FinBit" column of a WebSocket frame.
 */
class FrameListColumnFinBit extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.finBit !== nextProps.item.finBit;
  }

  render() {
    const { finBit } = this.props.item;

    return dom.td(
      {
        className: "ws-frames-list-column ws-frames-list-finBit",
        title: finBit.toString(),
      },
      finBit.toString()
    );
  }
}

module.exports = FrameListColumnFinBit;

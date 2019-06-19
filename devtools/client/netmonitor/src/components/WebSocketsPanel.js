/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  connect,
} = require("devtools/client/shared/redux/visibility-handler-connect");
const { getFramesByChannelId } = require("../selectors/index");

const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { table, tbody, thead, tr, td, th, div } = dom;

const { L10N } = require("../utils/l10n");
const FRAMES_EMPTY_TEXT = L10N.getStr("webSocketsEmptyText");

class WebSocketsPanel extends Component {
  static get propTypes() {
    return {
      channelId: PropTypes.number,
      frames: PropTypes.array,
    };
  }

  constructor(props) {
    super(props);
  }

  render() {
    const { frames } = this.props;

    if (!frames) {
      return div({ className: "empty-notice" },
        FRAMES_EMPTY_TEXT
      );
    }

    const rows = [];
    frames.forEach((frame, index) => {
      rows.push(
        tr(
          { key: index,
            className: "frames-row" },
          td({ className: "frames-cell" }, frame.type),
          td({ className: "frames-cell" }, frame.httpChannelId),
          td({ className: "frames-cell" }, frame.payload),
          td({ className: "frames-cell" }, frame.opCode),
          td({ className: "frames-cell" }, frame.maskBit.toString()),
          td({ className: "frames-cell" }, frame.finBit.toString()),
          td({ className: "frames-cell" }, frame.timeStamp)
        )
      );
    });

    return table(
      { className: "frames-list-table" },
      thead(
        { className: "frames-head" },
        tr(
          { className: "frames-row" },
          th({ className: "frames-headerCell" }, "Type"),
          th({ className: "frames-headerCell" }, "Channel ID"),
          th({ className: "frames-headerCell" }, "Payload"),
          th({ className: "frames-headerCell" }, "OpCode"),
          th({ className: "frames-headerCell" }, "MaskBit"),
          th({ className: "frames-headerCell" }, "FinBit"),
          th({ className: "frames-headerCell" }, "Time")
        )
      ),
      tbody(
        {
          className: "frames-list-tableBody",
        },
        rows
      )
    );
  }
}

module.exports = connect((state, props) => ({
  frames: getFramesByChannelId(state, props.channelId),
}))(WebSocketsPanel);

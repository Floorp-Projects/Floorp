/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("../../utils/l10n");

/**
 * Renders the "Data" column of a WebSocket frame.
 */
class FrameListColumnData extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      connector: PropTypes.object.isRequired,
    };
  }

  render() {
    const { type, payload } = this.props.item;
    const typeLabel = L10N.getStr(`netmonitor.ws.type.${type}`);

    // If payload is a LongStringActor object, we show the first 1000 characters
    const displayedPayload = payload.initial ? payload.initial : payload;

    return dom.td(
      {
        className: "ws-frames-list-column ws-frames-list-payload",
        title: typeLabel + " " + displayedPayload,
      },
      dom.img({
        alt: typeLabel,
        className: `ws-frames-list-type-icon ws-frames-list-type-icon-${type}`,
        src: `chrome://devtools/content/netmonitor/src/assets/icons/arrow-up.svg`,
      }),
      " " + displayedPayload
    );
  }
}

module.exports = FrameListColumnData;

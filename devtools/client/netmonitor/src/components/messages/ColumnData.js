/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
} = require("resource://devtools/client/shared/vendor/react.js");
const dom = require("resource://devtools/client/shared/vendor/react-dom-factories.js");
const PropTypes = require("resource://devtools/client/shared/vendor/react-prop-types.js");
const {
  L10N,
} = require("resource://devtools/client/netmonitor/src/utils/l10n.js");
const {
  limitTooltipLength,
} = require("resource://devtools/client/netmonitor/src/utils/tooltips.js");

/**
 * Renders the "Data" column of a message.
 */
class ColumnData extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
      connector: PropTypes.object.isRequired,
    };
  }

  render() {
    const { type, payload } = this.props.item;
    // type could be undefined for sse channel.
    const typeLabel = type ? L10N.getStr(`netmonitor.ws.type.${type}`) : null;

    // If payload is a LongStringActor object, we show the first 1000 characters
    const displayedPayload = payload.initial ? payload.initial : payload;

    const frameTypeImg = type
      ? dom.img({
          alt: typeLabel,
          className: `message-list-type-icon message-list-type-icon-${type}`,
          src: `chrome://devtools/content/netmonitor/src/assets/icons/arrow-up.svg`,
        })
      : null;

    let title = limitTooltipLength(displayedPayload);
    title = type ? typeLabel + " " + title : title;

    return dom.td(
      {
        className: "message-list-column message-list-payload",
        title,
      },
      frameTypeImg,
      " " + displayedPayload
    );
  }
}

module.exports = ColumnData;

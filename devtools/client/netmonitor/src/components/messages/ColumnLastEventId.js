/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Renders the "LastEventId" column of a message.
 */
class ColumnLastEventId extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.lastEventId !== nextProps.item.lastEventId;
  }

  render() {
    const { lastEventId } = this.props.item;

    return dom.td(
      {
        className: "message-list-column message-list-lastEventId",
        title: lastEventId,
      },
      lastEventId
    );
  }
}

module.exports = ColumnLastEventId;

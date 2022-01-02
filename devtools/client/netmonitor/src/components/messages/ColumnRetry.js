/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

/**
 * Renders the "Retry" column of a message.
 */
class ColumnRetry extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.retry !== nextProps.item.retry;
  }

  render() {
    const { retry } = this.props.item;

    return dom.td(
      {
        className: "message-list-column message-list-retry",
        title: retry,
      },
      retry
    );
  }
}

module.exports = ColumnRetry;

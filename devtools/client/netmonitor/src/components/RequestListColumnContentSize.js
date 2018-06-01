/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { getFormattedSize } = require("../utils/format-utils");

const { div } = dom;

class RequestListColumnContentSize extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return this.props.item.contentSize !== nextProps.item.contentSize;
  }

  render() {
    const { contentSize } = this.props.item;
    const size = typeof contentSize === "number" ? getFormattedSize(contentSize) : null;
    return (
      div({ className: "requests-list-column requests-list-size", title: size }, size)
    );
  }
}

module.exports = RequestListColumnContentSize;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getFormattedSize } = require("../utils/format-utils");

const { div } = DOM;

const RequestListColumnContentSize = createClass({
  displayName: "RequestListColumnContentSize",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.contentSize !== nextProps.item.contentSize;
  },

  render() {
    let { contentSize } = this.props.item;
    let size = typeof contentSize === "number" ? getFormattedSize(contentSize) : null;
    return (
      div({ className: "requests-list-column requests-list-size", title: size }, size)
    );
  }
});

module.exports = RequestListColumnContentSize;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div } = DOM;

const RequestListColumnMethod = createClass({
  displayName: "RequestListColumnMethod",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.method !== nextProps.item.method;
  },

  render() {
    let { method } = this.props.item;
    return div({ className: "requests-list-column requests-list-method" }, method);
  }
});

module.exports = RequestListColumnMethod;

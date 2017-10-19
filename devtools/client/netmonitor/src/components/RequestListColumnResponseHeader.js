/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getResponseHeader } = require("../utils/request-utils");

const { div } = DOM;

/**
 * Renders a response header column in the requests list.  The actual
 * header to show is passed as a prop.
 */
const RequestListColumnResponseHeader = createClass({
  displayName: "RequestListColumnResponseHeader",

  propTypes: {
    item: PropTypes.object.isRequired,
    header: PropTypes.string.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    const currHeader = getResponseHeader(this.props.item, this.props.header);
    const nextHeader = getResponseHeader(nextProps.item, nextProps.header);
    return currHeader !== nextHeader;
  },

  render() {
    let header = getResponseHeader(this.props.item, this.props.header);
    return (
      div({
        className: "requests-list-column requests-list-response-header",
        title: header
      },
        header
      )
    );
  }
});

module.exports = RequestListColumnResponseHeader;

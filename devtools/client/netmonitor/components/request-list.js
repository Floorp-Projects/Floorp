/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PropTypes, DOM } = require("devtools/client/shared/vendor/react");
const { div } = DOM;
const { connect } = require("devtools/client/shared/vendor/react-redux");
const RequestListHeader = createFactory(require("./request-list-header"));
const RequestListEmptyNotice = createFactory(require("./request-list-empty"));
const RequestListContent = createFactory(require("./request-list-content"));

/**
 * Renders the request list - header, empty text, the actual content with rows
 */
const RequestList = function ({ isEmpty }) {
  return div({ className: "request-list-container" },
    RequestListHeader(),
    isEmpty ? RequestListEmptyNotice() : RequestListContent()
  );
};

RequestList.displayName = "RequestList";

RequestList.propTypes = {
  isEmpty: PropTypes.bool.isRequired,
};

module.exports = connect(
  state => ({
    isEmpty: state.requests.requests.isEmpty()
  })
)(RequestList);

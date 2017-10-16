/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

// Components
const RequestListContent = createFactory(require("./request-list-content"));
const RequestListEmptyNotice = createFactory(require("./request-list-empty-notice"));
const RequestListHeader = createFactory(require("./request-list-header"));
const StatusBar = createFactory(require("./status-bar"));

const { div } = DOM;

/**
 * Request panel component
 */
function RequestList({ isEmpty }) {
  return (
    div({ className: "request-list-container" },
      RequestListHeader(),
      isEmpty ? RequestListEmptyNotice() : RequestListContent(),
      StatusBar(),
    )
  );
}

RequestList.displayName = "RequestList";

RequestList.propTypes = {
  isEmpty: PropTypes.bool.isRequired,
};

module.exports = RequestList;

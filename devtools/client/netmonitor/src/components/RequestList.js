/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

// Components
const StatusBar = createFactory(require("./StatusBar"));

loader.lazyGetter(this, "RequestListContent", function () {
  return createFactory(require("./RequestListContent"));
});
loader.lazyGetter(this, "RequestListEmptyNotice", function () {
  return createFactory(require("./RequestListEmptyNotice"));
});

/**
 * Request panel component
 */
function RequestList({
  connector,
  isEmpty,
}) {
  return (
    div({ className: "request-list-container" },
      isEmpty ? RequestListEmptyNotice({ connector }) : RequestListContent({ connector }),
      StatusBar({ connector }),
    )
  );
}

RequestList.displayName = "RequestList";

RequestList.propTypes = {
  connector: PropTypes.object.isRequired,
  isEmpty: PropTypes.bool.isRequired,
};

module.exports = RequestList;

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

const RequestListColumnCause = createClass({
  displayName: "RequestListColumnCause",

  propTypes: {
    item: PropTypes.object.isRequired,
    onCauseBadgeClick: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.cause !== nextProps.item.cause;
  },

  render() {
    let {
      item: { cause },
      onCauseBadgeClick,
    } = this.props;

    let causeType = "unknown";
    let causeHasStack = false;

    if (cause) {
      // Legacy server might send a numeric value. Display it as "unknown"
      causeType = typeof cause.type === "string" ? cause.type : "unknown";
      causeHasStack = cause.stacktrace && cause.stacktrace.length > 0;
    }

    return (
      div({ className: "requests-list-column requests-list-cause", title: causeType },
        causeHasStack && div({
          className: "requests-list-cause-stack",
          onClick: onCauseBadgeClick,
        }, "JS"),
        causeType
      )
    );
  }
});

module.exports = RequestListColumnCause;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");

const { div, span } = DOM;

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
    const {
      item,
      onCauseBadgeClick,
    } = this.props;

    const { cause } = item;

    let causeType = "";
    let causeUri = undefined;
    let causeHasStack = false;

    if (cause) {
      // Legacy server might send a numeric value. Display it as "unknown"
      causeType = typeof cause.type === "string" ? cause.type : "unknown";
      causeUri = cause.loadingDocumentUri;
      causeHasStack = cause.stacktrace && cause.stacktrace.length > 0;
    }

    return (
      div({
        className: "requests-list-subitem requests-list-cause",
        title: causeUri,
      },
        span({
          className: "requests-list-cause-stack",
          hidden: !causeHasStack,
          onClick: onCauseBadgeClick,
        }, "JS"),
        span({ className: "subitem-label" }, causeType),
      )
    );
  }
});

module.exports = RequestListColumnCause;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  createFactory,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const I = require("devtools/client/shared/vendor/immutable");
const { propertiesEqual } = require("../utils/request-utils");

// Components
const RequestListColumnCause = createFactory(require("./request-list-column-cause"));
const RequestListColumnContentSize = createFactory(require("./request-list-column-content-size"));
const RequestListColumnDomain = createFactory(require("./request-list-column-domain"));
const RequestListColumnFile = createFactory(require("./request-list-column-file"));
const RequestListColumnMethod = createFactory(require("./request-list-column-method"));
const RequestListColumnProtocol = createFactory(require("./request-list-column-protocol"));
const RequestListColumnRemoteIP = createFactory(require("./request-list-column-remote-ip"));
const RequestListColumnStatus = createFactory(require("./request-list-column-status"));
const RequestListColumnTransferredSize = createFactory(require("./request-list-column-transferred-size"));
const RequestListColumnType = createFactory(require("./request-list-column-type"));
const RequestListColumnWaterfall = createFactory(require("./request-list-column-waterfall"));

const { div } = DOM;

/**
 * Used by shouldComponentUpdate: compare two items, and compare only properties
 * relevant for rendering the RequestListItem. Other properties (like request and
 * response headers, cookies, bodies) are ignored. These are very useful for the
 * network details, but not here.
 */
const UPDATED_REQ_ITEM_PROPS = [
  "mimeType",
  "eventTimings",
  "securityState",
  "responseContentDataUri",
  "status",
  "statusText",
  "fromCache",
  "fromServiceWorker",
  "method",
  "url",
  "remoteAddress",
  "cause",
  "contentSize",
  "transferredSize",
  "startedMillis",
  "totalTime",
];

const UPDATED_REQ_PROPS = [
  "firstRequestStartedMillis",
  "index",
  "isSelected",
  "waterfallWidth",
];

/**
 * Render one row in the request list.
 */
const RequestListItem = createClass({
  displayName: "RequestListItem",

  propTypes: {
    columns: PropTypes.object.isRequired,
    item: PropTypes.object.isRequired,
    index: PropTypes.number.isRequired,
    isSelected: PropTypes.bool.isRequired,
    firstRequestStartedMillis: PropTypes.number.isRequired,
    fromCache: PropTypes.bool,
    onCauseBadgeClick: PropTypes.func.isRequired,
    onContextMenu: PropTypes.func.isRequired,
    onFocusedNodeChange: PropTypes.func,
    onMouseDown: PropTypes.func.isRequired,
    onSecurityIconClick: PropTypes.func.isRequired,
    waterfallWidth: PropTypes.number,
  },

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.listItem.focus();
    }
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_REQ_ITEM_PROPS, this.props.item, nextProps.item) ||
      !propertiesEqual(UPDATED_REQ_PROPS, this.props, nextProps) ||
      !I.is(this.props.columns, nextProps.columns);
  },

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.listItem.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  },

  render() {
    let {
      columns,
      item,
      index,
      isSelected,
      firstRequestStartedMillis,
      fromCache,
      onContextMenu,
      onMouseDown,
      onCauseBadgeClick,
      onSecurityIconClick,
    } = this.props;

    let classList = ["request-list-item", index % 2 ? "odd" : "even"];
    isSelected && classList.push("selected");
    fromCache && classList.push("fromCache");

    return (
      div({
        ref: "listItem",
        className: classList.join(" "),
        "data-id": item.id,
        tabIndex: 0,
        onContextMenu,
        onMouseDown,
      },
        columns.get("status") && RequestListColumnStatus({ item }),
        columns.get("method") && RequestListColumnMethod({ item }),
        columns.get("file") && RequestListColumnFile({ item }),
        columns.get("protocol") && RequestListColumnProtocol({ item }),
        columns.get("domain") && RequestListColumnDomain({ item, onSecurityIconClick }),
        columns.get("remoteip") && RequestListColumnRemoteIP({ item }),
        columns.get("cause") && RequestListColumnCause({ item, onCauseBadgeClick }),
        columns.get("type") && RequestListColumnType({ item }),
        columns.get("transferred") && RequestListColumnTransferredSize({ item }),
        columns.get("contentSize") && RequestListColumnContentSize({ item }),
        columns.get("waterfall") &&
          RequestListColumnWaterfall({ item, firstRequestStartedMillis }),
      )
    );
  }
});

module.exports = RequestListItem;

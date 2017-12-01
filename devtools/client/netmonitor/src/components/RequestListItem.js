/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const I = require("devtools/client/shared/vendor/immutable");
const { propertiesEqual } = require("../utils/request-utils");
const { RESPONSE_HEADERS } = require("../constants");

// Components
const RequestListColumnCause = createFactory(require("./RequestListColumnCause"));
const RequestListColumnContentSize = createFactory(require("./RequestListColumnContentSize"));
const RequestListColumnCookies = createFactory(require("./RequestListColumnCookies"));
const RequestListColumnDomain = createFactory(require("./RequestListColumnDomain"));
const RequestListColumnDuration = createFactory(require("./RequestListColumnDuration"));
const RequestListColumnEndTime = createFactory(require("./RequestListColumnEndTime"));
const RequestListColumnFile = createFactory(require("./RequestListColumnFile"));
const RequestListColumnLatency = createFactory(require("./RequestListColumnLatency"));
const RequestListColumnMethod = createFactory(require("./RequestListColumnMethod"));
const RequestListColumnProtocol = createFactory(require("./RequestListColumnProtocol"));
const RequestListColumnRemoteIP = createFactory(require("./RequestListColumnRemoteIp"));
const RequestListColumnResponseHeader = createFactory(require("./RequestListColumnResponseHeader"));
const RequestListColumnResponseTime = createFactory(require("./RequestListColumnResponseTime"));
const RequestListColumnScheme = createFactory(require("./RequestListColumnScheme"));
const RequestListColumnSetCookies = createFactory(require("./RequestListColumnSetCookies"));
const RequestListColumnStartTime = createFactory(require("./RequestListColumnStartTime"));
const RequestListColumnStatus = createFactory(require("./RequestListColumnStatus"));
const RequestListColumnTransferredSize = createFactory(require("./RequestListColumnTransferredSize"));
const RequestListColumnType = createFactory(require("./RequestListColumnType"));
const RequestListColumnWaterfall = createFactory(require("./RequestListColumnWaterfall"));

const { div } = dom;

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
class RequestListItem extends Component {
  static get propTypes() {
    return {
      columns: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      isSelected: PropTypes.bool.isRequired,
      firstRequestStartedMillis: PropTypes.number.isRequired,
      fromCache: PropTypes.bool,
      onCauseBadgeMouseDown: PropTypes.func.isRequired,
      onContextMenu: PropTypes.func.isRequired,
      onFocusedNodeChange: PropTypes.func,
      onMouseDown: PropTypes.func.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
      waterfallWidth: PropTypes.number,
    };
  }

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.listItem.focus();
    }
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_REQ_ITEM_PROPS, this.props.item, nextProps.item) ||
      !propertiesEqual(UPDATED_REQ_PROPS, this.props, nextProps) ||
      !I.is(this.props.columns, nextProps.columns);
  }

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.listItem.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  }

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
      onCauseBadgeMouseDown,
      onSecurityIconMouseDown,
      onWaterfallMouseDown,
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
        columns.get("scheme") && RequestListColumnScheme({ item }),
        columns.get("domain") && RequestListColumnDomain({ item,
                                                           onSecurityIconMouseDown }),
        columns.get("remoteip") && RequestListColumnRemoteIP({ item }),
        columns.get("cause") && RequestListColumnCause({ item, onCauseBadgeMouseDown }),
        columns.get("type") && RequestListColumnType({ item }),
        columns.get("cookies") && RequestListColumnCookies({ item }),
        columns.get("setCookies") && RequestListColumnSetCookies({ item }),
        columns.get("transferred") && RequestListColumnTransferredSize({ item }),
        columns.get("contentSize") && RequestListColumnContentSize({ item }),
        columns.get("startTime") &&
          RequestListColumnStartTime({ item, firstRequestStartedMillis }),
        columns.get("endTime") &&
          RequestListColumnEndTime({ item, firstRequestStartedMillis }),
        columns.get("responseTime") &&
          RequestListColumnResponseTime({ item, firstRequestStartedMillis }),
        columns.get("duration") && RequestListColumnDuration({ item }),
        columns.get("latency") && RequestListColumnLatency({ item }),
        ...RESPONSE_HEADERS.filter(header => columns.get(header)).map(
          header => RequestListColumnResponseHeader({ item, header }),
        ),
        columns.get("waterfall") &&
          RequestListColumnWaterfall({ item, firstRequestStartedMillis,
                                       onWaterfallMouseDown }),
      )
    );
  }
}

module.exports = RequestListItem;

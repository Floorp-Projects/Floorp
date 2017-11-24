/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
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
  "requestCookies",
  "responseCookies",
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
      connector: PropTypes.object.isRequired,
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
      this.props.columns !== nextProps.columns;
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
      connector,
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
        columns.status && RequestListColumnStatus({ item }),
        columns.method && RequestListColumnMethod({ item }),
        columns.file && RequestListColumnFile({ item }),
        columns.protocol && RequestListColumnProtocol({ item }),
        columns.scheme && RequestListColumnScheme({ item }),
        columns.domain && RequestListColumnDomain({ item,
                                                    onSecurityIconMouseDown }),
        columns.remoteip && RequestListColumnRemoteIP({ item }),
        columns.cause && RequestListColumnCause({ item, onCauseBadgeMouseDown }),
        columns.type && RequestListColumnType({ item }),
        columns.cookies && RequestListColumnCookies({ connector, item }),
        columns.setCookies && RequestListColumnSetCookies({ connector, item }),
        columns.transferred && RequestListColumnTransferredSize({ item }),
        columns.contentSize && RequestListColumnContentSize({ item }),
        columns.startTime &&
          RequestListColumnStartTime({ item, firstRequestStartedMillis }),
        columns.endTime &&
          RequestListColumnEndTime({ item, firstRequestStartedMillis }),
        columns.responseTime &&
          RequestListColumnResponseTime({ item, firstRequestStartedMillis }),
        columns.duration && RequestListColumnDuration({ item }),
        columns.latency && RequestListColumnLatency({ item }),
        ...RESPONSE_HEADERS.filter(header => columns[header]).map(
          header => RequestListColumnResponseHeader({ item, header }),
        ),
        columns.waterfall &&
          RequestListColumnWaterfall({ item, firstRequestStartedMillis,
                                       onWaterfallMouseDown }),
      )
    );
  }
}

module.exports = RequestListItem;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { div } = dom;
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  fetchNetworkUpdatePacket,
  propertiesEqual,
} = require("../utils/request-utils");
const { RESPONSE_HEADERS } = require("../constants");

// Components
/* global
  RequestListColumnCause,
  RequestListColumnContentSize,
  RequestListColumnCookies,
  RequestListColumnDomain,
  RequestListColumnDuration,
  RequestListColumnEndTime,
  RequestListColumnFile,
  RequestListColumnLatency,
  RequestListColumnMethod,
  RequestListColumnProtocol,
  RequestListColumnRemoteIP,
  RequestListColumnResponseHeader,
  RequestListColumnResponseTime,
  RequestListColumnScheme,
  RequestListColumnSetCookies,
  RequestListColumnStartTime,
  RequestListColumnStatus,
  RequestListColumnTransferredSize,
  RequestListColumnType,
  RequestListColumnWaterfall
*/

const COLUMNS = [
  "Cause",
  "ContentSize",
  "Cookies",
  "Domain",
  "Duration",
  "EndTime",
  "File",
  "Latency",
  "Method",
  "Protocol",
  "RemoteIP",
  "ResponseHeader",
  "ResponseTime",
  "Scheme",
  "SetCookies",
  "StartTime",
  "Status",
  "TransferredSize",
  "Type",
  "Waterfall"
];
for (let name of COLUMNS) {
  loader.lazyGetter(this, "RequestListColumn" + name, function () {
    return createFactory(require("./RequestListColumn" + name));
  });
}

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
  "requestHeaders",
  "responseCookies",
  "responseHeaders",
];

const UPDATED_REQ_PROPS = [
  "firstRequestStartedMillis",
  "index",
  "isSelected",
  "requestFilterTypes",
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
      requestFilterTypes: PropTypes.string.isRequired,
      waterfallWidth: PropTypes.number,
    };
  }

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.listItem.focus();
    }

    let { connector, item, requestFilterTypes } = this.props;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
    }
  }

  componentWillReceiveProps(nextProps) {
    let { connector, item, requestFilterTypes } = nextProps;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
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

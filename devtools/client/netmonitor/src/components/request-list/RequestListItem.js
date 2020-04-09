/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const {
  fetchNetworkUpdatePacket,
  propertiesEqual,
} = require("devtools/client/netmonitor/src/utils/request-utils");
const {
  RESPONSE_HEADERS,
} = require("devtools/client/netmonitor/src/constants");

// Components
/* global
  RequestListColumnInitiator,
  RequestListColumnContentSize,
  RequestListColumnCookies,
  RequestListColumnDomain,
  RequestListColumnFile,
  RequestListColumnMethod,
  RequestListColumnProtocol,
  RequestListColumnRemoteIP,
  RequestListColumnResponseHeader,
  RequestListColumnScheme,
  RequestListColumnSetCookies,
  RequestListColumnStatus,
  RequestListColumnTime,
  RequestListColumnTransferredSize,
  RequestListColumnType,
  RequestListColumnUrl,
  RequestListColumnWaterfall
*/
loader.lazyGetter(this, "RequestListColumnInitiator", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnInitiator")
  );
});
loader.lazyGetter(this, "RequestListColumnContentSize", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnContentSize")
  );
});
loader.lazyGetter(this, "RequestListColumnCookies", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnCookies")
  );
});
loader.lazyGetter(this, "RequestListColumnDomain", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnDomain")
  );
});
loader.lazyGetter(this, "RequestListColumnFile", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnFile")
  );
});
loader.lazyGetter(this, "RequestListColumnUrl", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnUrl")
  );
});
loader.lazyGetter(this, "RequestListColumnMethod", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnMethod")
  );
});
loader.lazyGetter(this, "RequestListColumnProtocol", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnProtocol")
  );
});
loader.lazyGetter(this, "RequestListColumnRemoteIP", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnRemoteIP")
  );
});
loader.lazyGetter(this, "RequestListColumnResponseHeader", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnResponseHeader")
  );
});
loader.lazyGetter(this, "RequestListColumnTime", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnTime")
  );
});
loader.lazyGetter(this, "RequestListColumnScheme", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnScheme")
  );
});
loader.lazyGetter(this, "RequestListColumnSetCookies", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnSetCookies")
  );
});
loader.lazyGetter(this, "RequestListColumnStatus", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnStatus")
  );
});
loader.lazyGetter(this, "RequestListColumnTransferredSize", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnTransferredSize")
  );
});
loader.lazyGetter(this, "RequestListColumnType", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnType")
  );
});
loader.lazyGetter(this, "RequestListColumnWaterfall", function() {
  return createFactory(
    require("devtools/client/netmonitor/src/components/request-list/RequestListColumnWaterfall")
  );
});

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
  "isRacing",
  "fromServiceWorker",
  "method",
  "url",
  "remoteAddress",
  "cause",
  "contentSize",
  "transferredSize",
  "startedMs",
  "totalTime",
  "requestCookies",
  "requestHeaders",
  "responseCookies",
  "responseHeaders",
];

const UPDATED_REQ_PROPS = [
  "firstRequestStartedMs",
  "index",
  "networkDetailsOpen",
  "isSelected",
  "isVisible",
  "requestFilterTypes",
];

/**
 * Used by render: renders the given ColumnComponent if the flag for this column
 * is set in the columns prop. The list of props are used to determine which of
 * RequestListItem's need to be passed to the ColumnComponent. Any objects contained
 * in that list are passed as props verbatim.
 */
const COLUMN_COMPONENTS = [
  { column: "status", ColumnComponent: RequestListColumnStatus },
  { column: "method", ColumnComponent: RequestListColumnMethod },
  {
    column: "domain",
    ColumnComponent: RequestListColumnDomain,
    props: ["onSecurityIconMouseDown"],
  },
  { column: "file", ColumnComponent: RequestListColumnFile },
  {
    column: "url",
    ColumnComponent: RequestListColumnUrl,
    props: ["onSecurityIconMouseDown"],
  },
  { column: "protocol", ColumnComponent: RequestListColumnProtocol },
  { column: "scheme", ColumnComponent: RequestListColumnScheme },
  { column: "remoteip", ColumnComponent: RequestListColumnRemoteIP },
  {
    column: "initiator",
    ColumnComponent: RequestListColumnInitiator,
    props: ["onInitiatorBadgeMouseDown"],
  },
  { column: "type", ColumnComponent: RequestListColumnType },
  {
    column: "cookies",
    ColumnComponent: RequestListColumnCookies,
    props: ["connector"],
  },
  {
    column: "setCookies",
    ColumnComponent: RequestListColumnSetCookies,
    props: ["connector"],
  },
  { column: "transferred", ColumnComponent: RequestListColumnTransferredSize },
  { column: "contentSize", ColumnComponent: RequestListColumnContentSize },
  {
    column: "startTime",
    ColumnComponent: RequestListColumnTime,
    props: ["connector", "firstRequestStartedMs", { type: "start" }],
  },
  {
    column: "endTime",
    ColumnComponent: RequestListColumnTime,
    props: ["connector", "firstRequestStartedMs", { type: "end" }],
  },
  {
    column: "responseTime",
    ColumnComponent: RequestListColumnTime,
    props: ["connector", "firstRequestStartedMs", { type: "response" }],
  },
  {
    column: "duration",
    ColumnComponent: RequestListColumnTime,
    props: ["connector", "firstRequestStartedMs", { type: "duration" }],
  },
  {
    column: "latency",
    ColumnComponent: RequestListColumnTime,
    props: ["connector", "firstRequestStartedMs", { type: "latency" }],
  },
];

/**
 * Render one row in the request list.
 */
class RequestListItem extends Component {
  static get propTypes() {
    return {
      blocked: PropTypes.bool,
      connector: PropTypes.object.isRequired,
      columns: PropTypes.object.isRequired,
      item: PropTypes.object.isRequired,
      index: PropTypes.number.isRequired,
      isSelected: PropTypes.bool.isRequired,
      isVisible: PropTypes.bool.isRequired,
      firstRequestStartedMs: PropTypes.number.isRequired,
      fromCache: PropTypes.bool,
      networkDetailsOpen: PropTypes.bool,
      onInitiatorBadgeMouseDown: PropTypes.func.isRequired,
      onDoubleClick: PropTypes.func.isRequired,
      onContextMenu: PropTypes.func.isRequired,
      onFocusedNodeChange: PropTypes.func,
      onMouseDown: PropTypes.func.isRequired,
      onSecurityIconMouseDown: PropTypes.func.isRequired,
      onWaterfallMouseDown: PropTypes.func.isRequired,
      requestFilterTypes: PropTypes.object.isRequired,
      intersectionObserver: PropTypes.object,
    };
  }

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.listItem.focus();
    }
    if (this.props.intersectionObserver) {
      this.props.intersectionObserver.observe(this.refs.listItem);
    }

    const { connector, item, requestFilterTypes } = this.props;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
    }
  }

  componentWillReceiveProps(nextProps) {
    const { connector, item, requestFilterTypes } = nextProps;
    // Filtering XHR & WS require to lazily fetch requestHeaders & responseHeaders
    if (requestFilterTypes.xhr || requestFilterTypes.ws) {
      fetchNetworkUpdatePacket(connector.requestData, item, [
        "requestHeaders",
        "responseHeaders",
      ]);
    }
  }

  shouldComponentUpdate(nextProps) {
    return (
      !propertiesEqual(
        UPDATED_REQ_ITEM_PROPS,
        this.props.item,
        nextProps.item
      ) ||
      !propertiesEqual(UPDATED_REQ_PROPS, this.props, nextProps) ||
      this.props.columns !== nextProps.columns
    );
  }

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.listItem.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  }

  componentWillUnmount() {
    if (this.props.intersectionObserver) {
      this.props.intersectionObserver.unobserve(this.refs.listItem);
    }
  }

  render() {
    const {
      blocked,
      connector,
      columns,
      item,
      index,
      isSelected,
      isVisible,
      firstRequestStartedMs,
      fromCache,
      onDoubleClick,
      onContextMenu,
      onMouseDown,
      onWaterfallMouseDown,
    } = this.props;

    const classList = ["request-list-item", index % 2 ? "odd" : "even"];
    isSelected && classList.push("selected");
    fromCache && classList.push("fromCache");
    blocked && classList.push("blocked");

    return dom.tr(
      {
        ref: "listItem",
        className: classList.join(" "),
        "data-id": item.id,
        tabIndex: 0,
        onContextMenu,
        onMouseDown,
        onDoubleClick,
      },
      ...COLUMN_COMPONENTS.filter(({ column }) => columns[column]).map(
        ({ column, ColumnComponent, props: columnProps }) => {
          return ColumnComponent({
            key: column,
            item,
            ...(columnProps || []).reduce((acc, keyOrObject) => {
              if (typeof keyOrObject == "string") {
                acc[keyOrObject] = this.props[keyOrObject];
              } else {
                Object.assign(acc, keyOrObject);
              }
              return acc;
            }, {}),
          });
        }
      ),
      ...RESPONSE_HEADERS.filter(header => columns[header]).map(header =>
        RequestListColumnResponseHeader({
          connector,
          item,
          header,
        })
      ),
      // The last column is Waterfall (aka Timeline)
      columns.waterfall &&
        RequestListColumnWaterfall({
          connector,
          firstRequestStartedMs,
          item,
          onWaterfallMouseDown,
          isVisible,
        })
    );
  }
}

module.exports = RequestListItem;

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
const { L10N } = require("../utils/l10n");
const { getAbbreviatedMimeType } = require("../utils/request-utils");
const { getFormattedSize } = require("../utils/format-utils");

const { div, img, span } = DOM;

/**
 * Compare two objects on a subset of their properties
 */
function propertiesEqual(props, item1, item2) {
  return item1 === item2 || props.every(p => item1[p] === item2[p]);
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
  "index",
  "isSelected",
  "firstRequestStartedMillis",
];

/**
 * Render one row in the request list.
 */
const RequestListItem = createClass({
  displayName: "RequestListItem",

  propTypes: {
    item: PropTypes.object.isRequired,
    index: PropTypes.number.isRequired,
    isSelected: PropTypes.bool.isRequired,
    firstRequestStartedMillis: PropTypes.number.isRequired,
    fromCache: PropTypes.bool.isRequired,
    onContextMenu: PropTypes.func.isRequired,
    onFocusedNodeChange: PropTypes.func,
    onMouseDown: PropTypes.func.isRequired,
    onSecurityIconClick: PropTypes.func.isRequired,
  },

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.el.focus();
    }
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_REQ_ITEM_PROPS, this.props.item, nextProps.item) ||
      !propertiesEqual(UPDATED_REQ_PROPS, this.props, nextProps);
  },

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.el.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  },

  render() {
    const {
      item,
      index,
      isSelected,
      firstRequestStartedMillis,
      fromCache,
      onContextMenu,
      onMouseDown,
      onSecurityIconClick
    } = this.props;

    let classList = ["request-list-item"];
    if (isSelected) {
      classList.push("selected");
    }

    if (fromCache) {
      classList.push("fromCache");
    }

    classList.push(index % 2 ? "odd" : "even");

    return (
      div({
        ref: "el",
        className: classList.join(" "),
        "data-id": item.id,
        tabIndex: 0,
        onContextMenu,
        onMouseDown,
      },
        StatusColumn({ item }),
        MethodColumn({ item }),
        FileColumn({ item }),
        DomainColumn({ item, onSecurityIconClick }),
        CauseColumn({ item }),
        TypeColumn({ item }),
        TransferredSizeColumn({ item }),
        ContentSizeColumn({ item }),
        WaterfallColumn({ item, firstRequestStartedMillis }),
      )
    );
  }
});

const UPDATED_STATUS_PROPS = [
  "status",
  "statusText",
  "fromCache",
  "fromServiceWorker",
];

const StatusColumn = createFactory(createClass({
  displayName: "StatusColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_STATUS_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { status, statusText, fromCache, fromServiceWorker } = this.props.item;

    let code, title;

    if (status) {
      if (fromCache) {
        code = "cached";
      } else if (fromServiceWorker) {
        code = "service worker";
      } else {
        code = status;
      }

      if (statusText) {
        title = `${status} ${statusText}`;
        if (fromCache) {
          title += " (cached)";
        }
        if (fromServiceWorker) {
          title += " (service worker)";
        }
      }
    }

    return (
        div({ className: "requests-list-subitem requests-list-status", title },
        div({ className: "requests-list-status-icon", "data-code": code }),
        span({ className: "subitem-label requests-list-status-code" }, status)
      )
    );
  }
}));

const MethodColumn = createFactory(createClass({
  displayName: "MethodColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.method !== nextProps.item.method;
  },

  render() {
    const { method } = this.props.item;
    return (
      div({ className: "requests-list-subitem requests-list-method-box" },
        span({ className: "subitem-label requests-list-method" }, method)
      )
    );
  }
}));

const UPDATED_FILE_PROPS = [
  "urlDetails",
  "responseContentDataUri",
];

const FileColumn = createFactory(createClass({
  displayName: "FileColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_FILE_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { urlDetails, responseContentDataUri } = this.props.item;

    return (
      div({ className: "requests-list-subitem requests-list-icon-and-file" },
        img({
          className: "requests-list-icon",
          src: responseContentDataUri,
          hidden: !responseContentDataUri,
          "data-type": responseContentDataUri ? "thumbnail" : undefined,
        }),
        div({
          className: "subitem-label requests-list-file",
          title: urlDetails.unicodeUrl,
        },
          urlDetails.baseNameWithQuery,
        ),
      )
    );
  }
}));

const UPDATED_DOMAIN_PROPS = [
  "urlDetails",
  "remoteAddress",
  "securityState",
];

const DomainColumn = createFactory(createClass({
  displayName: "DomainColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
    onSecurityIconClick: PropTypes.func.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_DOMAIN_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { item, onSecurityIconClick } = this.props;
    const { urlDetails, remoteAddress, securityState } = item;

    let iconClassList = ["requests-security-state-icon"];
    let iconTitle;
    if (urlDetails.isLocal) {
      iconClassList.push("security-state-local");
      iconTitle = L10N.getStr("netmonitor.security.state.secure");
    } else if (securityState) {
      iconClassList.push(`security-state-${securityState}`);
      iconTitle = L10N.getStr(`netmonitor.security.state.${securityState}`);
    }

    let title = urlDetails.host + (remoteAddress ? ` (${remoteAddress})` : "");

    return (
      div({ className: "requests-list-subitem requests-list-security-and-domain" },
        div({
          className: iconClassList.join(" "),
          title: iconTitle,
          onClick: onSecurityIconClick,
        }),
        span({ className: "subitem-label requests-list-domain", title }, urlDetails.host),
      )
    );
  }
}));

const CauseColumn = createFactory(createClass({
  displayName: "CauseColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.cause !== nextProps.item.cause;
  },

  render() {
    const { cause } = this.props.item;

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
        }, "JS"),
        span({ className: "subitem-label" }, causeType),
      )
    );
  }
}));

const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};

const TypeColumn = createFactory(createClass({
  displayName: "TypeColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.mimeType !== nextProps.item.mimeType;
  },

  render() {
    const { mimeType } = this.props.item;
    let abbrevType;
    if (mimeType) {
      abbrevType = getAbbreviatedMimeType(mimeType);
      abbrevType = CONTENT_MIME_TYPE_ABBREVIATIONS[abbrevType] || abbrevType;
    }

    return (
      div({
        className: "requests-list-subitem requests-list-type",
        title: mimeType,
      },
        span({ className: "subitem-label" }, abbrevType),
      )
    );
  }
}));

const UPDATED_TRANSFERRED_PROPS = [
  "transferredSize",
  "fromCache",
  "fromServiceWorker",
];

const TransferredSizeColumn = createFactory(createClass({
  displayName: "TransferredSizeColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_TRANSFERRED_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { transferredSize, fromCache, fromServiceWorker, status } = this.props.item;

    let text;
    let className = "subitem-label";
    if (fromCache || status === "304") {
      text = L10N.getStr("networkMenu.sizeCached");
      className += " theme-comment";
    } else if (fromServiceWorker) {
      text = L10N.getStr("networkMenu.sizeServiceWorker");
      className += " theme-comment";
    } else if (typeof transferredSize == "number") {
      text = getFormattedSize(transferredSize);
    } else if (transferredSize === null) {
      text = L10N.getStr("networkMenu.sizeUnavailable");
    }

    return (
      div({
        className: "requests-list-subitem requests-list-transferred",
        title: text,
      },
        span({ className }, text),
      )
    );
  }
}));

const ContentSizeColumn = createFactory(createClass({
  displayName: "ContentSizeColumn",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.item.contentSize !== nextProps.item.contentSize;
  },

  render() {
    const { contentSize } = this.props.item;

    let text;
    if (typeof contentSize == "number") {
      text = getFormattedSize(contentSize);
    }

    return (
      div({
        className: "requests-list-subitem subitem-label requests-list-size",
        title: text,
      },
        span({ className: "subitem-label" }, text),
      )
    );
  }
}));

const UPDATED_WATERFALL_PROPS = [
  "eventTimings",
  "totalTime",
  "fromCache",
  "fromServiceWorker",
];

const WaterfallColumn = createFactory(createClass({
  displayName: "WaterfallColumn",

  propTypes: {
    firstRequestStartedMillis: PropTypes.number.isRequired,
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return this.props.firstRequestStartedMillis !== nextProps.firstRequestStartedMillis ||
      !propertiesEqual(UPDATED_WATERFALL_PROPS, this.props.item, nextProps.item);
  },

  render() {
    const { item, firstRequestStartedMillis } = this.props;

    return (
      div({ className: "requests-list-subitem requests-list-waterfall" },
        div({
          className: "requests-list-timings",
          style: {
            paddingInlineStart: `${item.startedMillis - firstRequestStartedMillis}px`,
          },
        },
          timingBoxes(item),
        )
      )
    );
  }
}));

// List of properties of the timing info we want to create boxes for
const TIMING_KEYS = ["blocked", "dns", "connect", "send", "wait", "receive"];

function timingBoxes(item) {
  const { eventTimings, totalTime, fromCache, fromServiceWorker } = item;
  let boxes = [];

  if (fromCache || fromServiceWorker) {
    return boxes;
  }

  if (eventTimings) {
    // Add a set of boxes representing timing information.
    for (let key of TIMING_KEYS) {
      let width = eventTimings.timings[key];

      // Don't render anything if it surely won't be visible.
      // One millisecond == one unscaled pixel.
      if (width > 0) {
        boxes.push(div({
          key,
          className: "requests-list-timings-box " + key,
          style: { width }
        }));
      }
    }
  }

  if (typeof totalTime === "number") {
    let text = L10N.getFormatStr("networkMenu.totalMS", totalTime);
    boxes.push(div({
      key: "total",
      className: "requests-list-timings-total",
      title: text
    }, text));
  }

  return boxes;
}

module.exports = RequestListItem;

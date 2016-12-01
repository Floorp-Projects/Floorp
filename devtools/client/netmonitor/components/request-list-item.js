/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createClass, PropTypes, DOM } = require("devtools/client/shared/vendor/react");
const { div, span, img } = DOM;
const { L10N } = require("../l10n");
const { getFormattedSize } = require("../utils/format-utils");
const { getAbbreviatedMimeType } = require("../request-utils");

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
    onContextMenu: PropTypes.func.isRequired,
    onMouseDown: PropTypes.func.isRequired,
    onSecurityIconClick: PropTypes.func.isRequired,
  },

  componentDidMount() {
    if (this.props.isSelected) {
      this.refs.el.focus();
    }
  },

  shouldComponentUpdate(nextProps) {
    return !relevantPropsEqual(this.props.item, nextProps.item)
      || this.props.index !== nextProps.index
      || this.props.isSelected !== nextProps.isSelected
      || this.props.firstRequestStartedMillis !== nextProps.firstRequestStartedMillis;
  },

  componentDidUpdate(prevProps) {
    if (!prevProps.isSelected && this.props.isSelected) {
      this.refs.el.focus();
      if (this.props.onFocusedNodeChange) {
        this.props.onFocusedNodeChange();
      }
    }
  },

  componentWillUnmount() {
    // If this node is being destroyed and has focus, transfer the focus manually
    // to the parent tree component. Otherwise, the focus will get lost and keyboard
    // navigation in the tree will stop working. This is a workaround for a XUL bug.
    // See bugs 1259228 and 1152441 for details.
    // DE-XUL: Remove this hack once all usages are only in HTML documents.
    if (this.props.isSelected) {
      this.refs.el.blur();
      if (this.props.onFocusedNodeUnmount) {
        this.props.onFocusedNodeUnmount();
      }
    }
  },

  render() {
    const {
      item,
      index,
      isSelected,
      firstRequestStartedMillis,
      onContextMenu,
      onMouseDown,
      onSecurityIconClick
    } = this.props;

    let classList = [ "request-list-item" ];
    if (isSelected) {
      classList.push("selected");
    }
    classList.push(index % 2 ? "odd" : "even");

    return div(
      {
        ref: "el",
        className: classList.join(" "),
        "data-id": item.id,
        tabIndex: 0,
        onContextMenu,
        onMouseDown,
      },
      StatusColumn(item),
      MethodColumn(item),
      FileColumn(item),
      DomainColumn(item, onSecurityIconClick),
      CauseColumn(item),
      TypeColumn(item),
      TransferredSizeColumn(item),
      ContentSizeColumn(item),
      WaterfallColumn(item, firstRequestStartedMillis)
    );
  }
});

/**
 * Used by shouldComponentUpdate: compare two items, and compare only properties
 * relevant for rendering the RequestListItem. Other properties (like request and
 * response headers, cookies, bodies) are ignored. These are very useful for the
 * sidebar details, but not here.
 */
const RELEVANT_ITEM_PROPS = [
  "status",
  "statusText",
  "fromCache",
  "fromServiceWorker",
  "method",
  "url",
  "responseContentDataUri",
  "remoteAddress",
  "securityState",
  "cause",
  "mimeType",
  "contentSize",
  "transferredSize",
  "startedMillis",
  "totalTime",
  "eventTimings",
];

function relevantPropsEqual(item1, item2) {
  return item1 === item2 || RELEVANT_ITEM_PROPS.every(p => item1[p] === item2[p]);
}

function StatusColumn(item) {
  const { status, statusText, fromCache, fromServiceWorker } = item;

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

  return div({ className: "requests-menu-subitem requests-menu-status", title },
    div({ className: "requests-menu-status-icon", "data-code": code }),
    span({ className: "subitem-label requests-menu-status-code" }, status)
  );
}

function MethodColumn(item) {
  const { method } = item;
  return div({ className: "requests-menu-subitem requests-menu-method-box" },
    span({ className: "subitem-label requests-menu-method" }, method)
  );
}

function FileColumn(item) {
  const { urlDetails, responseContentDataUri } = item;

  return div({ className: "requests-menu-subitem requests-menu-icon-and-file" },
    img({
      className: "requests-menu-icon",
      src: responseContentDataUri,
      hidden: !responseContentDataUri,
      "data-type": responseContentDataUri ? "thumbnail" : undefined
    }),
    div(
      {
        className: "subitem-label requests-menu-file",
        title: urlDetails.unicodeUrl
      },
      urlDetails.baseNameWithQuery
    )
  );
}

function DomainColumn(item, onSecurityIconClick) {
  const { urlDetails, remoteAddress, securityState } = item;

  let iconClassList = [ "requests-security-state-icon" ];
  let iconTitle;
  if (urlDetails.isLocal) {
    iconClassList.push("security-state-local");
    iconTitle = L10N.getStr("netmonitor.security.state.secure");
  } else if (securityState) {
    iconClassList.push(`security-state-${securityState}`);
    iconTitle = L10N.getStr(`netmonitor.security.state.${securityState}`);
  }

  let title = urlDetails.host + (remoteAddress ? ` (${remoteAddress})` : "");

  return div(
    { className: "requests-menu-subitem requests-menu-security-and-domain" },
    div({
      className: iconClassList.join(" "),
      title: iconTitle,
      onClick: onSecurityIconClick,
    }),
    span({ className: "subitem-label requests-menu-domain", title }, urlDetails.host)
  );
}

function CauseColumn(item) {
  const { cause } = item;

  let causeType = "";
  let causeUri = undefined;
  let causeHasStack = false;

  if (cause) {
    causeType = cause.type;
    causeUri = cause.loadingDocumentUri;
    causeHasStack = cause.stacktrace && cause.stacktrace.length > 0;
  }

  return div(
    { className: "requests-menu-subitem requests-menu-cause", title: causeUri },
    span({ className: "requests-menu-cause-stack", hidden: !causeHasStack }, "JS"),
    span({ className: "subitem-label" }, causeType)
  );
}

const CONTENT_MIME_TYPE_ABBREVIATIONS = {
  "ecmascript": "js",
  "javascript": "js",
  "x-javascript": "js"
};

function TypeColumn(item) {
  const { mimeType } = item;
  let abbrevType;
  if (mimeType) {
    abbrevType = getAbbreviatedMimeType(mimeType);
    abbrevType = CONTENT_MIME_TYPE_ABBREVIATIONS[abbrevType] || abbrevType;
  }

  return div(
    { className: "requests-menu-subitem requests-menu-type", title: mimeType },
    span({ className: "subitem-label" }, abbrevType)
  );
}

function TransferredSizeColumn(item) {
  const { transferredSize, fromCache, fromServiceWorker } = item;

  let text;
  let className = "subitem-label";
  if (fromCache) {
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

  return div(
    { className: "requests-menu-subitem requests-menu-transferred", title: text },
    span({ className }, text)
  );
}

function ContentSizeColumn(item) {
  const { contentSize } = item;

  let text;
  if (typeof contentSize == "number") {
    text = getFormattedSize(contentSize);
  }

  return div(
    { className: "requests-menu-subitem subitem-label requests-menu-size", title: text },
    span({ className: "subitem-label" }, text)
  );
}

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
          className: "requests-menu-timings-box " + key,
          style: { width }
        }));
      }
    }
  }

  if (typeof totalTime == "number") {
    let text = L10N.getFormatStr("networkMenu.totalMS", totalTime);
    boxes.push(div({
      key: "total",
      className: "requests-menu-timings-total",
      title: text
    }, text));
  }

  return boxes;
}

function WaterfallColumn(item, firstRequestStartedMillis) {
  const startedDeltaMillis = item.startedMillis - firstRequestStartedMillis;
  const paddingInlineStart = `${startedDeltaMillis}px`;

  return div({ className: "requests-menu-subitem requests-menu-waterfall" },
    div(
      { className: "requests-menu-timings", style: { paddingInlineStart } },
      timingBoxes(item)
    )
  );
}

module.exports = RequestListItem;

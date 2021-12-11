/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { L10N } = require("devtools/client/netmonitor/src/utils/l10n");
const {
  propertiesEqual,
} = require("devtools/client/netmonitor/src/utils/request-utils");

const { div } = dom;

const UPDATED_STATUS_PROPS = [
  "fromCache",
  "fromServiceWorker",
  "status",
  "statusText",
];

/**
 * Status code component
 * Displays HTTP status code icon
 * Used in RequestListColumnStatus and HeadersPanel
 */
class StatusCode extends Component {
  static get propTypes() {
    return {
      item: PropTypes.object.isRequired,
    };
  }

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(
      UPDATED_STATUS_PROPS,
      this.props.item,
      nextProps.item
    );
  }

  render() {
    const { item } = this.props;
    const {
      fromCache,
      fromServiceWorker,
      status,
      statusText,
      blockedReason,
    } = item;
    let code;

    if (status) {
      if (fromCache) {
        code = "cached";
      } else if (fromServiceWorker) {
        code = "service worker";
      } else {
        code = status;
      }
    }

    if (blockedReason) {
      return div(
        {
          className:
            "requests-list-status-code status-code status-code-blocked",
          title: L10N.getStr("networkMenu.blockedTooltip"),
        },
        div({
          className: "status-code-blocked-icon",
        })
      );
    }

    // `data-code` refers to the status-code
    // `data-status-code` can be one of "cached", "service worker"
    // or the status-code itself
    // For example - if a resource is cached, `data-code` would be 200
    // and the `data-status-code` would be "cached"
    return div(
      {
        className: "requests-list-status-code status-code",
        onMouseOver: function({ target }) {
          if (status && statusText && !target.title) {
            target.title = getStatusTooltip(item);
          }
        },
        "data-status-code": code,
        "data-code": status,
      },
      status
    );
  }
}

function getStatusTooltip(item) {
  const { fromCache, fromServiceWorker, status, statusText } = item;
  let title;
  if (fromCache && fromServiceWorker) {
    title = L10N.getFormatStr(
      "netmonitor.status.tooltip.cachedworker",
      status,
      statusText
    );
  } else if (fromCache) {
    title = L10N.getFormatStr(
      "netmonitor.status.tooltip.cached",
      status,
      statusText
    );
  } else if (fromServiceWorker) {
    title = L10N.getFormatStr(
      "netmonitor.status.tooltip.worker",
      status,
      statusText
    );
  } else {
    title = L10N.getFormatStr(
      "netmonitor.status.tooltip.simple",
      status,
      statusText
    );
  }
  return title;
}

module.exports = StatusCode;

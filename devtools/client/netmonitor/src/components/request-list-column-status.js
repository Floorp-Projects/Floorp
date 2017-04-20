/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { L10N } = require("../utils/l10n");
const { propertiesEqual } = require("../utils/request-utils");

const { div, span } = DOM;

const UPDATED_STATUS_PROPS = [
  "fromCache",
  "fromServiceWorker",
  "status",
  "statusText",
];

const RequestListColumnStatus = createClass({
  displayName: "RequestListColumnStatus",

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
        if (fromCache && fromServiceWorker) {
          title = L10N.getFormatStr("netmonitor.status.tooltip.cachedworker",
            status, statusText);
        } else if (fromCache) {
          title = L10N.getFormatStr("netmonitor.status.tooltip.cached",
            status, statusText);
        } else if (fromServiceWorker) {
          title = L10N.getFormatStr("netmonitor.status.tooltip.worker",
            status, statusText);
        } else {
          title = L10N.getFormatStr("netmonitor.status.tooltip.simple",
            status, statusText);
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
});

module.exports = RequestListColumnStatus;

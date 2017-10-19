/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createClass,
  DOM,
  PropTypes,
} = require("devtools/client/shared/vendor/react");
const { getFormattedSize } = require("../utils/format-utils");
const { L10N } = require("../utils/l10n");
const { propertiesEqual } = require("../utils/request-utils");

const { div } = DOM;

const UPDATED_TRANSFERRED_PROPS = [
  "transferredSize",
  "fromCache",
  "fromServiceWorker",
];

const RequestListColumnTransferredSize = createClass({
  displayName: "RequestListColumnTransferredSize",

  propTypes: {
    item: PropTypes.object.isRequired,
  },

  shouldComponentUpdate(nextProps) {
    return !propertiesEqual(UPDATED_TRANSFERRED_PROPS, this.props.item, nextProps.item);
  },

  render() {
    let { fromCache, fromServiceWorker, status, transferredSize } = this.props.item;
    let text;

    if (fromCache || status === "304") {
      text = L10N.getStr("networkMenu.sizeCached");
    } else if (fromServiceWorker) {
      text = L10N.getStr("networkMenu.sizeServiceWorker");
    } else if (typeof transferredSize == "number") {
      text = getFormattedSize(transferredSize);
    } else if (transferredSize === null) {
      text = L10N.getStr("networkMenu.sizeUnavailable");
    }

    return (
      div({ className: "requests-list-column requests-list-transferred", title: text },
        text
      )
    );
  }
});

module.exports = RequestListColumnTransferredSize;

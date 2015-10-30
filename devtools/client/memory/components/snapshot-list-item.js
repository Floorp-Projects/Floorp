/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N, getSnapshotTitle, getSnapshotTotals, getSnapshotStatusText } = require("../utils");
const { snapshotState: states } = require("../constants");
const { snapshot: snapshotModel } = require("../models");

const SnapshotListItem = module.exports = createClass({
  displayName: "snapshot-list-item",

  propTypes: {
    onClick: PropTypes.func,
    item: snapshotModel.isRequired,
    index: PropTypes.number.isRequired,
  },

  render() {
    let { index, item: snapshot, onClick } = this.props;
    let className = `snapshot-list-item ${snapshot.selected ? " selected" : ""}`;
    let statusText = getSnapshotStatusText(snapshot);
    let title = getSnapshotTitle(snapshot);

    let details;
    if (snapshot.state === states.SAVED_CENSUS) {
      let { bytes } = getSnapshotTotals(snapshot);
      let formatBytes = L10N.getFormatStr("aggregate.mb", L10N.numberWithDecimals(bytes / 1000000, 2));

      details = dom.span({ className: "snapshot-totals" },
        dom.span({ className: "total-bytes" }, formatBytes)
      );
    } else {
      details = dom.span({ className: "snapshot-state" }, statusText);
    }

    return (
      dom.li({ className, onClick },
        dom.span({
          className: `snapshot-title ${statusText ? " devtools-throbber" : ""}`
        }, title),
        details
      )
    );
  }
});


/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { DOM: dom, createClass, PropTypes } = require("devtools/client/shared/vendor/react");
const { L10N, getSnapshotStatusText } = require("../utils");
const { snapshot: snapshotModel } = require("../models");

const SnapshotListItem = module.exports = createClass({
  displayName: "snapshot-list-item",

  propTypes: {
    onClick: PropTypes.func,
    item: snapshotModel.isRequired,
    index: PropTypes.number.isRequired,
  },

  render() {
    let { index, item, onClick } = this.props;
    let className = `snapshot-list-item ${item.selected ? " selected" : ""}`;
    let statusText = getSnapshotStatusText(item);

    return (
      dom.li({ className, onClick },
        dom.span({
          className: `snapshot-title ${statusText ? " devtools-throbber" : ""}`
        }, `Snapshot #${index}`),

        statusText ? dom.span({ className: "snapshot-state" }, statusText) : void 0
      )
    );
  }
});


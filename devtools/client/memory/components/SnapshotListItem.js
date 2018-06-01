/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Component } = require("devtools/client/shared/vendor/react");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const {
  L10N,
  getSnapshotTitle,
  getSnapshotTotals,
  getStatusText,
  snapshotIsDiffable,
  getSavedCensus
} = require("../utils");
const { diffingState } = require("../constants");
const { snapshot: snapshotModel, app: appModel } = require("../models");

class SnapshotListItem extends Component {
  static get propTypes() {
    return {
      onClick: PropTypes.func.isRequired,
      onSave: PropTypes.func.isRequired,
      onDelete: PropTypes.func.isRequired,
      item: snapshotModel.isRequired,
      index: PropTypes.number.isRequired,
      diffing: appModel.diffing,
    };
  }

  render() {
    const { item: snapshot, onClick, onSave, onDelete, diffing } = this.props;
    let className = `snapshot-list-item ${snapshot.selected ? " selected" : ""}`;
    let statusText = getStatusText(snapshot.state);
    let wantThrobber = !!statusText;
    const title = getSnapshotTitle(snapshot);

    const selectedForDiffing = diffing
          && (diffing.firstSnapshotId === snapshot.id
              || diffing.secondSnapshotId === snapshot.id);

    let checkbox;
    if (diffing && snapshotIsDiffable(snapshot)) {
      if (diffing.state === diffingState.SELECTING) {
        wantThrobber = false;
      }

      const checkboxAttrs = {
        type: "checkbox",
        checked: false,
      };

      if (selectedForDiffing) {
        checkboxAttrs.checked = true;
        checkboxAttrs.disabled = true;
        className += " selected";
        statusText = L10N.getStr(diffing.firstSnapshotId === snapshot.id
                                   ? "diffing.baseline"
                                   : "diffing.comparison");
      }

      if (selectedForDiffing || diffing.state == diffingState.SELECTING) {
        checkbox = dom.input(checkboxAttrs);
      }
    }

    let details;
    if (!selectedForDiffing) {
      // See if a tree map or census is in the read state.
      const census = getSavedCensus(snapshot);

      // If there is census data, fill in the total bytes.
      if (census) {
        const { bytes } = getSnapshotTotals(census);
        const formatBytes = L10N.getFormatStr("aggregate.mb",
          L10N.numberWithDecimals(bytes / 1000000, 2));

        details = dom.span({ className: "snapshot-totals" },
          dom.span({ className: "total-bytes" }, formatBytes)
        );
      }
    }
    if (!details) {
      details = dom.span({ className: "snapshot-state" }, statusText);
    }

    const saveLink = !snapshot.path ? void 0 : dom.a({
      onClick: () => onSave(snapshot),
      className: "save",
    }, L10N.getStr("snapshot.io.save"));

    const deleteButton = !snapshot.path ? void 0 : dom.button({
      onClick: () => onDelete(snapshot),
      className: "delete",
      title: L10N.getStr("snapshot.io.delete")
    });

    return (
      dom.li({ className, onClick },
        dom.span({
          className: `snapshot-title ${wantThrobber ? " devtools-throbber" : ""}`
        },
          checkbox,
          title,
          deleteButton
        ),
        dom.span({ className: "snapshot-info" },
          details,
          saveLink
        )
      )
    );
  }
}

module.exports = SnapshotListItem;

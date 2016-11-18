/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { L10N } = require("../l10n");
const Actions = require("../actions/index");

const { button, div } = DOM;

function FilterButtons({
  filterTypes,
  triggerFilterType,
}) {
  const buttons = filterTypes.entrySeq().map(([type, checked]) => {
    let classList = ["menu-filter-button"];
    checked && classList.push("checked");

    return button({
      id: `requests-menu-filter-${type}-button`,
      className: classList.join(" "),
      "data-key": type,
      onClick: triggerFilterType,
      onKeyDown: triggerFilterType,
    }, L10N.getStr(`netmonitor.toolbar.filter.${type}`));
  }).toArray();

  return div({ id: "requests-menu-filter-buttons" }, buttons);
}

FilterButtons.PropTypes = {
  state: PropTypes.object.isRequired,
  triggerFilterType: PropTypes.func.iRequired,
};

module.exports = connect(
  (state) => ({ filterTypes: state.filters.types }),
  (dispatch) => ({
    triggerFilterType: (evt) => {
      if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
        return;
      }
      dispatch(Actions.toggleFilterType(evt.target.dataset.key));
    },
  })
)(FilterButtons);

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
  requestFilterTypes,
  toggleRequestFilterType,
}) {
  const buttons = requestFilterTypes.entrySeq().map(([type, checked]) => {
    let classList = ["menu-filter-button"];
    checked && classList.push("checked");

    return button({
      id: `requests-menu-filter-${type}-button`,
      className: classList.join(" "),
      "data-key": type,
      onClick: toggleRequestFilterType,
      onKeyDown: toggleRequestFilterType,
    }, L10N.getStr(`netmonitor.toolbar.filter.${type}`));
  }).toArray();

  return div({ id: "requests-menu-filter-buttons" }, buttons);
}

FilterButtons.propTypes = {
  state: PropTypes.object.isRequired,
  toggleRequestFilterType: PropTypes.func.iRequired,
};

module.exports = connect(
  (state) => ({ requestFilterTypes: state.filters.requestFilterTypes }),
  (dispatch) => ({
    toggleRequestFilterType: (evt) => {
      if (evt.type === "keydown" && (evt.key !== "" || evt.key !== "Enter")) {
        return;
      }
      dispatch(Actions.toggleRequestFilterType(evt.target.dataset.key));
    },
  })
)(FilterButtons);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { L10N } = require("../l10n");
const Actions = require("../actions/index");
const { isSidebarToggleButtonDisabled } = require("../selectors/index");

const { button } = DOM;

function ToggleButton({
  disabled,
  open,
  onToggle,
}) {
  let className = ["devtools-button"];
  if (!open) {
    className.push("pane-collapsed");
  }

  const title = open ? L10N.getStr("collapseDetailsPane") :
                       L10N.getStr("expandDetailsPane");

  return button({
    id: "details-pane-toggle",
    className: className.join(" "),
    title,
    disabled,
    tabIndex: "0",
    onMouseDown: onToggle,
  });
}

ToggleButton.propTypes = {
  disabled: PropTypes.bool.isRequired,
  onToggle: PropTypes.func.isRequired,
};

module.exports = connect(
  (state) => ({
    disabled: isSidebarToggleButtonDisabled(state),
    open: state.ui.sidebarOpen,
  }),
  (dispatch) => ({
    onToggle: () => dispatch(Actions.toggleSidebar())
  })
)(ToggleButton);

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals NetMonitorView */

"use strict";

const { DOM, PropTypes } = require("devtools/client/shared/vendor/react");
const { connect } = require("devtools/client/shared/vendor/react-redux");
const { L10N } = require("../l10n");
const Actions = require("../actions/index");

const { button } = DOM;

function ToggleButton({
  disabled,
  open,
  triggerSidebar,
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
    onMouseDown: triggerSidebar,
  });
}

ToggleButton.propTypes = {
  disabled: PropTypes.bool.isRequired,
  triggerSidebar: PropTypes.func.isRequired,
};

module.exports = connect(
  (state) => ({
    disabled: state.requests.items.length === 0,
    open: state.ui.sidebar.open,
  }),
  (dispatch) => ({
    triggerSidebar: () => {
      dispatch(Actions.toggleSidebar());

      let requestsMenu = NetMonitorView.RequestsMenu;
      let selectedIndex = requestsMenu.selectedIndex;

      // Make sure there's a selection if the button is pressed, to avoid
      // showing an empty network details pane.
      if (selectedIndex == -1 && requestsMenu.itemCount) {
        requestsMenu.selectedIndex = 0;
      } else {
        requestsMenu.selectedIndex = -1;
      }
    },
  })
)(ToggleButton);

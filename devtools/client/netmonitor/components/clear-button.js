/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals NetMonitorView */

"use strict";

const { DOM } = require("devtools/client/shared/vendor/react");
const { L10N } = require("../l10n");

const { button } = DOM;

/*
 * Clear button component
 * A type of tool button is responsible for cleaning network requests.
 */
function ClearButton() {
  return button({
    id: "requests-menu-clear-button",
    className: "devtools-button devtools-clear-icon",
    title: L10N.getStr("netmonitor.toolbar.clear"),
    onClick: () => {
      NetMonitorView.RequestsMenu.clear();
    },
  });
}

module.exports = ClearButton;

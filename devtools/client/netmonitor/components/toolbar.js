/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  DOM,
} = require("devtools/client/shared/vendor/react");
const ClearButton = createFactory(require("./clear-button"));
const FilterButtons = createFactory(require("./filter-buttons"));
const ToolbarSearchBox = createFactory(require("./search-box"));
const SummaryButton = createFactory(require("./summary-button"));
const ToggleButton = createFactory(require("./toggle-button"));

const { span } = DOM;

/*
 * Network monitor toolbar component
 * Toolbar contains a set of useful tools to control network requests
 */
function Toolbar() {
  return span({ className: "devtools-toolbar devtools-toolbar-container" },
    span({ className: "devtools-toolbar-group" },
      ClearButton(),
      FilterButtons()
    ),
    span({ className: "devtools-toolbar-group" },
      SummaryButton(),
      ToolbarSearchBox(),
      ToggleButton()
    )
  );
}

module.exports = Toolbar;

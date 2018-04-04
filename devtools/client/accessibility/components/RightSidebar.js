/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

// React
const { Component, createFactory } = require("devtools/client/shared/vendor/react");
const { div } = require("devtools/client/shared/vendor/react-dom-factories");

const { L10N } = require("../utils/l10n");
const Accessible = createFactory(require("./Accessible"));

// Component that is responsible for rendering accessible panel's sidebar.
class RightSidebar extends Component {
  /**
   * Render the sidebar component.
   * @returns Sidebar React component.
   */
  render() {
    const headerID = "accessibility-right-sidebar-header";
    return (
      div({
        className: "right-sidebar",
        role: "presentation"
      },
        div({
          className: "_header",
          id: headerID,
          role: "heading"
        }, L10N.getStr("accessibility.properties")),
        div({
          className: "_content accessible",
          role: "presentation"
        }, Accessible({ labelledby: headerID }))
      )
    );
  }
}

module.exports = RightSidebar;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

class ChangesApp extends PureComponent {
  render() {
    return dom.div(
      {
        className: "theme-sidebar inspector-tabpanel",
        id: "sidebar-panel-changes"
      }
    );
  }
}

module.exports = connect(state => state)(ChangesApp);

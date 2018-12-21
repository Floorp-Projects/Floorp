/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const { connect } = require("devtools/client/shared/vendor/react-redux");

const { getStr } = require("../utils/l10n");

class RulesApp extends PureComponent {
  static get propTypes() {
    return {};
  }

  render() {
    return dom.div(
      {
        id: "sidebar-panel-ruleview",
        className: "theme-sidebar inspector-tabpanel",
      },
      dom.div(
        {
          id: "ruleview-no-results",
          className: "devtools-sidepanel-no-result",
        },
        getStr("rule.empty")
      )
    );
  }
}

module.exports = connect(state => state)(RulesApp);

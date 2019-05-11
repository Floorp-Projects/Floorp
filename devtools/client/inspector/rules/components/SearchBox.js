/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { getStr } = require("../utils/l10n");

class SearchBox extends PureComponent {
  static get propTypes() {
    return {};
  }

  render() {
    return (
      dom.div({ className: "devtools-searchbox" },
        dom.input({
          id: "ruleview-searchbox",
          className: "devtools-filterinput",
          placeholder: getStr("rule.filterStyles.placeholder"),
          type: "search",
        }),
        dom.button({
          id: "ruleview-searchinput-clear",
          className: "devtools-searchinput-clear",
        })
      )
    );
  }
}

module.exports = SearchBox;

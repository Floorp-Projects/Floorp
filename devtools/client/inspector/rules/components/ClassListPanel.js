/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const { getStr } = require("../utils/l10n");

class ClassListPanel extends PureComponent {
  static get propTypes() {
    return {};
  }

  render() {
    return (
      dom.div(
        {
          id: "ruleview-class-panel",
          className: "ruleview-reveal-panel",
        },
        dom.input({
          className: "devtools-textinput add-class",
          placeholder: getStr("rule.classPanel.newClass.placeholder"),
        }),
        dom.div({ className: "classes" },
          dom.p({ className: "no-classes" }, getStr("rule.classPanel.noClasses"))
        )
      )
    );
  }
}

module.exports = ClassListPanel;

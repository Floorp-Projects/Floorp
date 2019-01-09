/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

class PseudoClassPanel extends PureComponent {
  static get propTypes() {
    return {};
  }

  render() {
    return (
      dom.div(
        {
          id: "pseudo-class-panel",
          className: "ruleview-reveal-panel",
        },
        dom.label({},
          dom.input({
            id: "pseudo-hover-toggle",
            checked: false,
            tabIndex: -1,
            type: "checkbox",
            value: ":hover",
          }),
          ":hover"
        ),
        dom.label({},
          dom.input({
            id: "pseudo-active-toggle",
            checked: false,
            tabIndex: -1,
            type: "checkbox",
            value: ":active",
          }),
          ":active"
        ),
        dom.label({},
          dom.input({
            id: "pseudo-focus-toggle",
            checked: false,
            tabIndex: -1,
            type: "checkbox",
            value: ":focus",
          }),
          ":focus"
        )
      )
    );
  }
}

module.exports = PseudoClassPanel;

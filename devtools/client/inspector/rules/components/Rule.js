/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Declarations = createFactory(require("./Declarations"));
const Selector = createFactory(require("./Selector"));
const SourceLink = createFactory(require("./SourceLink"));

const Types = require("../types");

class Rule extends PureComponent {
  static get propTypes() {
    return {
      onToggleDeclaration: PropTypes.func.isRequired,
      rule: PropTypes.shape(Types.rule).isRequired,
    };
  }

  render() {
    const {
      onToggleDeclaration,
      rule,
    } = this.props;
    const {
      declarations,
      selector,
      sourceLink,
      type,
    } = rule;

    return (
      dom.div(
        { className: "ruleview-rule devtools-monospace" },
        SourceLink({ sourceLink }),
        dom.div({ className: "ruleview-code" },
          dom.div({},
            Selector({
              selector,
              type,
            }),
            dom.span({ className: "ruleview-ruleopen" }, " {")
          ),
          Declarations({
            declarations,
            onToggleDeclaration,
          }),
          dom.div({ className: "ruleview-ruleclose" }, "}")
        )
      )
    );
  }
}

module.exports = Rule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");

class Declaration extends PureComponent {
  static get propTypes() {
    return {
      declaration: PropTypes.shape(Types.declaration).isRequired,
    };
  }

  render() {
    const { declaration } = this.props;
    const {
      isEnabled,
      isKnownProperty,
      isOverridden,
      name,
      value,
    } = declaration;

    return (
      dom.li(
        {
          className: "ruleview-property" +
                     (!isEnabled || !isKnownProperty || isOverridden ?
                      " ruleview-overridden" : ""),
        },
        dom.div({ className: "ruleview-propertycontainer" },
          dom.div({
            className: "ruleview-enableproperty theme-checkbox" +
                        (isEnabled ? " checked" : ""),
            tabIndex: -1,
          }),
          dom.span({ className: "ruleview-namecontainer" },
            dom.span({ className: "ruleview-propertyname theme-fg-color5" }, name),
            ": "
          ),
          dom.span({ className: "ruleview-propertyvaluecontainer" },
            dom.span({ className: "ruleview-propertyvalue theme-fg-color1" }, value),
            ";"
          )
        )
      )
    );
  }
}

module.exports = Declaration;

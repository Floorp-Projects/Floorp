/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class Declaration extends PureComponent {
  static get propTypes() {
    return {
      declaration: PropTypes.shape(Types.declaration).isRequired,
      onToggleDeclaration: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the computed property list is expanded.
      isComputedListExpanded: false,
    };

    this.onComputedExpanderClick = this.onComputedExpanderClick.bind(this);
    this.onToggleDeclarationClick = this.onToggleDeclarationClick.bind(this);
  }

  onComputedExpanderClick(event) {
    event.stopPropagation();

    this.setState(prevState => {
      return { isComputedListExpanded: !prevState.isComputedListExpanded };
    });
  }

  onToggleDeclarationClick(event) {
    event.stopPropagation();
    const { id, ruleId } = this.props.declaration;
    this.props.onToggleDeclaration(ruleId, id);
  }

  renderComputedPropertyList() {
    if (!this.state.isComputedListExpanded) {
      return null;
    }

    return (
      dom.ul(
        {
          className: "ruleview-computedlist",
          style: {
            display: "block",
          },
        },
        this.props.declaration.computedProperties.map(({ name, value, isOverridden }) => {
          return (
            dom.li(
              {
                key: `${name}${value}`,
                className: "ruleview-computed" +
                           (isOverridden ? " ruleview-overridden" : ""),
              },
              dom.span({ className: "ruleview-namecontainer" },
                dom.span({ className: "ruleview-propertyname theme-fg-color5" }, name),
                ": "
              ),
              dom.span({ className: "ruleview-propertyvaluecontainer" },
                dom.span({ className: "ruleview-propertyvalue theme-fg-color1" }, value),
                ";"
              )
            )
          );
        })
      )
    );
  }

  renderShorthandOverriddenList() {
    if (this.state.isComputedListExpanded || this.props.declaration.isOverridden) {
      return null;
    }

    const overriddenComputedProperties = this.props.declaration.computedProperties
      .filter(prop => prop.isOverridden);

    if (!overriddenComputedProperties.length) {
      return null;
    }

    return (
      dom.ul({ className: "ruleview-overridden-items" },
        overriddenComputedProperties.map(({ name, value }) => {
          return (
            dom.li(
              {
                key: `${name}${value}`,
                className: "ruleview-overridden-item ruleview-overridden",
              },
              dom.span({ className: "ruleview-namecontainer" },
                dom.span({ className: "ruleview-propertyname theme-fg-color5" }, name),
                ": "
              ),
              dom.span({ className: "ruleview-propertyvaluecontainer" },
                dom.span({ className: "ruleview-propertyvalue theme-fg-color1" }, value),
                ";"
              )
            )
          );
        })
      )
    );
  }

  render() {
    const {
      computedProperties,
      isDeclarationValid,
      isEnabled,
      isKnownProperty,
      isNameValid,
      isOverridden,
      name,
      value,
    } = this.props.declaration;

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
            onClick: this.onToggleDeclarationClick,
            tabIndex: -1,
          }),
          dom.span({ className: "ruleview-namecontainer" },
            dom.span({ className: "ruleview-propertyname theme-fg-color5" }, name),
            ": "
          ),
          dom.span({
            className: "ruleview-expander theme-twisty" +
                       (this.state.isComputedListExpanded ? " open" : ""),
            onClick: this.onComputedExpanderClick,
            style: { display: computedProperties.length ? "inline-block" : "none" },
          }),
          dom.span({ className: "ruleview-propertyvaluecontainer" },
            dom.span({ className: "ruleview-propertyvalue theme-fg-color1" }, value),
            ";"
          ),
          dom.div({
            className: "ruleview-warning" +
                       (isDeclarationValid ? " hidden" : ""),
            title: isNameValid ?
                   getStr("rule.warningName.title") : getStr("rule.warning.title"),
          }),
          dom.div({
            className: "ruleview-overridden-rule-filter" +
                       (!isDeclarationValid || !isOverridden ? " hidden" : ""),
            title: getStr("rule.filterProperty.title"),
          })
        ),
        this.renderComputedPropertyList(),
        this.renderShorthandOverriddenList()
      )
    );
  }
}

module.exports = Declaration;

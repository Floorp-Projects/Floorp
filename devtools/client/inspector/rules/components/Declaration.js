/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createRef, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { editableItem } = require("devtools/client/shared/inplace-editor");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class Declaration extends PureComponent {
  static get propTypes() {
    return {
      declaration: PropTypes.shape(Types.declaration).isRequired,
      isUserAgentStyle: PropTypes.bool.isRequired,
      onToggleDeclaration: PropTypes.func.isRequired,
      showDeclarationNameEditor: PropTypes.func.isRequired,
      showDeclarationValueEditor: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Whether or not the computed property list is expanded.
      isComputedListExpanded: false,
    };

    this.nameSpanRef = createRef();
    this.valueSpanRef = createRef();

    this.onComputedExpanderClick = this.onComputedExpanderClick.bind(this);
    this.onToggleDeclarationClick = this.onToggleDeclarationClick.bind(this);
  }

  componentDidMount() {
    if (this.props.isUserAgentStyle) {
      // Declaration is not editable.
      return;
    }

    const { ruleId, id } = this.props.declaration;

    editableItem({
      element: this.nameSpanRef.current,
    }, element => {
      this.props.showDeclarationNameEditor(element, ruleId, id);
    });

    editableItem({
      element: this.valueSpanRef.current,
    }, element => {
      this.props.showDeclarationValueEditor(element, ruleId, id);
    });
  }

  get hasComputed() {
    // Only show the computed list expander or the shorthand overridden list if:
    // - The computed properties are actually different from the current property
    //   (i.e these are longhands while the current property is the shorthand).
    // - All of the computed properties have defined values. In case the current property
    //   value contains CSS variables, then the computed properties will be missing and we
    //   want to avoid showing them.
    const { computedProperties } = this.props.declaration;
    return computedProperties.some(c => c.name !== this.props.declaration.name) &&
           !computedProperties.every(c => !c.value);
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
                dom.span({ className: "ruleview-propertyname theme-fg-color3" }, name),
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

  renderOverriddenFilter() {
    if (this.props.declaration.isDeclarationValid) {
      return null;
    }

    return (
      dom.div({
        className: "ruleview-warning",
        title: this.props.declaration.isNameValid ?
               getStr("rule.warningName.title") : getStr("rule.warning.title"),
      })
    );
  }

  renderShorthandOverriddenList() {
    if (this.state.isComputedListExpanded ||
        this.props.declaration.isOverridden ||
        !this.hasComputed) {
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
                dom.span({ className: "ruleview-propertyname theme-fg-color3" }, name),
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

  renderWarning() {
    if (!this.props.declaration.isDeclarationValid ||
        !this.props.declaration.isOverridden) {
      return null;
    }

    return (
      dom.div({
        className: "ruleview-overridden-rule-filter",
        title: getStr("rule.filterProperty.title"),
      })
    );
  }

  render() {
    const {
      isEnabled,
      isKnownProperty,
      isOverridden,
      isPropertyChanged,
      name,
      value,
    } = this.props.declaration;

    let declarationClassName = "ruleview-property";

    if (!isEnabled || !isKnownProperty || isOverridden) {
      declarationClassName += " ruleview-overridden";
    }

    if (isPropertyChanged) {
      declarationClassName += " ruleview-changed";
    }

    return (
      dom.li({ className: declarationClassName },
        dom.div({ className: "ruleview-propertycontainer" },
          dom.div({
            className: "ruleview-enableproperty theme-checkbox" +
                        (isEnabled ? " checked" : ""),
            onClick: this.onToggleDeclarationClick,
            tabIndex: -1,
          }),
          dom.span({ className: "ruleview-namecontainer" },
            dom.span(
              {
                className: "ruleview-propertyname theme-fg-color3",
                ref: this.nameSpanRef,
                tabIndex: 0,
              },
              name
            ),
            ": "
          ),
          dom.span({
            className: "ruleview-expander theme-twisty" +
                       (this.state.isComputedListExpanded ? " open" : ""),
            onClick: this.onComputedExpanderClick,
            style: { display: this.hasComputed ? "inline-block" : "none" },
          }),
          dom.span({ className: "ruleview-propertyvaluecontainer" },
            dom.span(
              {
                className: "ruleview-propertyvalue theme-fg-color1",
                ref: this.valueSpanRef,
                tabIndex: 0,
              },
              value
            ),
            ";"
          ),
          this.renderWarning(),
          this.renderOverriddenFilter()
        ),
        this.renderComputedPropertyList(),
        this.renderShorthandOverriddenList()
      )
    );
  }
}

module.exports = Declaration;

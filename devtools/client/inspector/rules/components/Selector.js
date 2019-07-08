/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { editableItem } = require("devtools/client/shared/inplace-editor");
const { PSEUDO_CLASSES } = require("devtools/shared/css/constants");
const {
  parsePseudoClassesAndAttributes,
  SELECTOR_ATTRIBUTE,
  SELECTOR_ELEMENT,
  SELECTOR_PSEUDO_CLASS,
} = require("devtools/shared/css/parsing-utils");
const { ELEMENT_STYLE } = require("devtools/client/inspector/rules/constants");

const Types = require("../types");

class Selector extends PureComponent {
  static get propTypes() {
    return {
      id: PropTypes.string.isRequired,
      isUserAgentStyle: PropTypes.bool.isRequired,
      selector: PropTypes.shape(Types.selector).isRequired,
      showSelectorEditor: PropTypes.func.isRequired,
      type: PropTypes.number.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.selectorRef = createRef();
  }

  componentDidMount() {
    if (
      this.props.isUserAgentStyle ||
      this.props.type === ELEMENT_STYLE ||
      this.props.type === CSSRule.KEYFRAME_RULE
    ) {
      // Selector is not editable.
      return;
    }

    editableItem(
      {
        element: this.selectorRef.current,
      },
      element => {
        this.props.showSelectorEditor(element, this.props.id);
      }
    );
  }

  renderSelector() {
    // Show the text directly for custom selector text (such as the inline "element"
    // style and Keyframes rules).
    if (
      this.props.type === ELEMENT_STYLE ||
      this.props.type === CSSRule.KEYFRAME_RULE
    ) {
      return this.props.selector.selectorText;
    }

    const { matchedSelectors, selectors } = this.props.selector;
    const output = [];

    // Go through the CSS rule's selectors and highlight the selectors that actually
    // matches.
    for (let i = 0; i < selectors.length; i++) {
      const selector = selectors[i];
      // Parse the selector for pseudo classes and attributes, and apply different
      // CSS classes for the parsed values.
      // NOTE: parsePseudoClassesAndAttributes is a good candidate for memoization.
      output.push(
        dom.span(
          {
            className:
              matchedSelectors.indexOf(selector) > -1
                ? "ruleview-selector-matched"
                : "ruleview-selector-unmatched",
          },
          parsePseudoClassesAndAttributes(selector).map(({ type, value }) => {
            let selectorSpanClass = "";

            switch (type) {
              case SELECTOR_ATTRIBUTE:
                selectorSpanClass += " ruleview-selector-attribute";
                break;
              case SELECTOR_ELEMENT:
                selectorSpanClass += " ruleview-selector";
                break;
              case SELECTOR_PSEUDO_CLASS:
                selectorSpanClass += PSEUDO_CLASSES.some(p => value === p)
                  ? " ruleview-selector-pseudo-class-lock"
                  : " ruleview-selector-pseudo-class";
                break;
            }

            return dom.span(
              {
                key: value,
                className: selectorSpanClass,
              },
              value
            );
          })
        )
      );

      // Append a comma separator unless this is the last selector.
      if (i < selectors.length - 1) {
        output.push(
          dom.span({ className: "ruleview-selector-separator" }, ", ")
        );
      }
    }

    return output;
  }

  render() {
    return dom.span(
      {
        className: "ruleview-selectorcontainer",
        ref: this.selectorRef,
        tabIndex: 0,
      },
      this.renderSelector()
    );
  }
}

module.exports = Selector;

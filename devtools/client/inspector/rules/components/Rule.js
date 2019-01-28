/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Declarations = createFactory(require("./Declarations"));
const Selector = createFactory(require("./Selector"));
const SelectorHighlighter = createFactory(require("./SelectorHighlighter"));
const SourceLink = createFactory(require("./SourceLink"));

const Types = require("../types");

class Rule extends PureComponent {
  static get propTypes() {
    return {
      onToggleDeclaration: PropTypes.func.isRequired,
      onToggleSelectorHighlighter: PropTypes.func.isRequired,
      rule: PropTypes.shape(Types.rule).isRequired,
      showDeclarationNameEditor: PropTypes.func.isRequired,
      showDeclarationValueEditor: PropTypes.func.isRequired,
      showSelectorEditor: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      onToggleDeclaration,
      onToggleSelectorHighlighter,
      rule,
      showDeclarationNameEditor,
      showDeclarationValueEditor,
      showSelectorEditor,
    } = this.props;
    const {
      declarations,
      id,
      isUnmatched,
      isUserAgentStyle,
      selector,
      sourceLink,
      type,
    } = rule;

    return (
      dom.div(
        {
          className: "ruleview-rule devtools-monospace" +
                     (isUnmatched ? " unmatched" : "") +
                     (isUserAgentStyle ? " uneditable" : ""),
        },
        SourceLink({ sourceLink }),
        dom.div({ className: "ruleview-code" },
          dom.div({},
            Selector({
              id,
              isUserAgentStyle,
              selector,
              showSelectorEditor,
              type,
            }),
            type !== CSSRule.KEYFRAME_RULE ?
              SelectorHighlighter({
                onToggleSelectorHighlighter,
                selector,
              })
              :
              null,
            dom.span({ className: "ruleview-ruleopen" }, " {")
          ),
          Declarations({
            declarations,
            isUserAgentStyle,
            onToggleDeclaration,
            showDeclarationNameEditor,
            showDeclarationValueEditor,
          }),
          dom.div({ className: "ruleview-ruleclose" }, "}")
        )
      )
    );
  }
}

module.exports = Rule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  createRef,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const { editableItem } = require("devtools/client/shared/inplace-editor");

const Declarations = createFactory(require("./Declarations"));
const Selector = createFactory(require("./Selector"));
const SelectorHighlighter = createFactory(require("./SelectorHighlighter"));
const SourceLink = createFactory(require("./SourceLink"));

const Types = require("../types");

class Rule extends PureComponent {
  static get propTypes() {
    return {
      onOpenSourceLink: PropTypes.func.isRequired,
      onToggleDeclaration: PropTypes.func.isRequired,
      onToggleSelectorHighlighter: PropTypes.func.isRequired,
      rule: PropTypes.shape(Types.rule).isRequired,
      showDeclarationNameEditor: PropTypes.func.isRequired,
      showDeclarationValueEditor: PropTypes.func.isRequired,
      showNewDeclarationEditor: PropTypes.func.isRequired,
      showSelectorEditor: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.closeBraceSpan = createRef();
    this.newDeclarationSpan = createRef();

    this.state = {
      // Whether or not the new declaration editor is visible.
      isNewDeclarationEditorVisible: false,
    };

    this.onEditorBlur = this.onEditorBlur.bind(this);
  }

  componentDidMount() {
    if (this.props.rule.isUserAgentStyle) {
      return;
    }

    editableItem(
      {
        element: this.closeBraceSpan.current,
      },
      () => {
        this.setState({ isNewDeclarationEditorVisible: true });
        this.props.showNewDeclarationEditor(
          this.newDeclarationSpan.current,
          this.props.rule.id,
          this.onEditorBlur
        );
      }
    );
  }

  onEditorBlur() {
    this.setState({ isNewDeclarationEditorVisible: false });
  }

  render() {
    const {
      onOpenSourceLink,
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

    return dom.div(
      {
        className:
          "ruleview-rule devtools-monospace" +
          (isUnmatched ? " unmatched" : "") +
          (isUserAgentStyle ? " uneditable" : ""),
      },
      SourceLink({
        id,
        isUserAgentStyle,
        onOpenSourceLink,
        sourceLink,
        type,
      }),
      dom.div(
        { className: "ruleview-code" },
        dom.div(
          {},
          Selector({
            id,
            isUserAgentStyle,
            selector,
            showSelectorEditor,
            type,
          }),
          type !== CSSRule.KEYFRAME_RULE
            ? SelectorHighlighter({
                onToggleSelectorHighlighter,
                selector,
              })
            : null,
          dom.span({ className: "ruleview-ruleopen" }, " {")
        ),
        Declarations({
          declarations,
          isUserAgentStyle,
          onToggleDeclaration,
          showDeclarationNameEditor,
          showDeclarationValueEditor,
        }),
        dom.li(
          {
            className: "ruleview-property ruleview-newproperty",
            style: {
              display: this.state.isNewDeclarationEditorVisible
                ? "block"
                : "none",
            },
          },
          dom.span({
            className: "ruleview-propertyname",
            ref: this.newDeclarationSpan,
          })
        ),
        dom.div(
          {
            className: "ruleview-ruleclose",
            ref: this.closeBraceSpan,
            tabIndex:
              !isUserAgentStyle && !this.state.isNewDeclarationEditorVisible
                ? 0
                : -1,
          },
          "}"
        )
      )
    );
  }
}

module.exports = Rule;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createFactory,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontName = createFactory(
  require("devtools/client/inspector/fonts/components/FontName")
);
const FontOrigin = createFactory(
  require("devtools/client/inspector/fonts/components/FontOrigin")
);
const FontPreview = createFactory(
  require("devtools/client/inspector/fonts/components/FontPreview")
);

const Types = require("devtools/client/inspector/fonts/types");

class Font extends PureComponent {
  static get propTypes() {
    return {
      font: PropTypes.shape(Types.font).isRequired,
      onPreviewClick: PropTypes.func,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isFontFaceRuleExpanded: false,
    };

    this.onFontFaceRuleToggle = this.onFontFaceRuleToggle.bind(this);
  }

  // FIXME: https://bugzilla.mozilla.org/show_bug.cgi?id=1774507
  UNSAFE_componentWillReceiveProps(newProps) {
    if (this.props.font.name === newProps.font.name) {
      return;
    }

    this.setState({
      isFontFaceRuleExpanded: false,
    });
  }

  onFontFaceRuleToggle(event) {
    this.setState({
      isFontFaceRuleExpanded: !this.state.isFontFaceRuleExpanded,
    });
    event.stopPropagation();
  }

  renderFontCSSCode(rule, ruleText) {
    if (!rule) {
      return null;
    }

    // Cut the rule text in 3 parts: the selector, the declarations, the closing brace.
    // This way we can collapse the declarations by default and display an expander icon
    // to expand them again.
    const leading = ruleText.substring(0, ruleText.indexOf("{") + 1);
    const body = ruleText.substring(
      ruleText.indexOf("{") + 1,
      ruleText.lastIndexOf("}")
    );
    const trailing = ruleText.substring(ruleText.lastIndexOf("}"));

    const { isFontFaceRuleExpanded } = this.state;

    return dom.pre(
      {
        className: "font-css-code",
      },
      this.renderFontCSSCodeTwisty(),
      leading,
      isFontFaceRuleExpanded
        ? body
        : dom.span({
            className: "font-css-code-expander",
            onClick: this.onFontFaceRuleToggle,
          }),
      trailing
    );
  }

  renderFontCSSCodeTwisty() {
    const { isFontFaceRuleExpanded } = this.state;

    const attributes = {
      className: "theme-twisty",
      onClick: this.onFontFaceRuleToggle,
    };
    if (isFontFaceRuleExpanded) {
      attributes.open = "true";
    }

    return dom.span(attributes);
  }

  renderFontFamilyName(family) {
    if (!family) {
      return null;
    }

    return dom.div({ className: "font-family-name" }, family);
  }

  render() {
    const { font, onPreviewClick, onToggleFontHighlight } = this.props;

    const { CSSFamilyName, previewUrl, rule, ruleText } = font;

    return dom.li(
      {
        className: "font",
      },
      dom.div(
        {},
        this.renderFontFamilyName(CSSFamilyName),
        FontName({ font, onToggleFontHighlight })
      ),
      FontOrigin({ font }),
      FontPreview({ onPreviewClick, previewUrl }),
      this.renderFontCSSCode(rule, ruleText)
    );
  }
}

module.exports = Font;

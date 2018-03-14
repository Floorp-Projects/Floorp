/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPreview = createFactory(require("./FontPreview"));

loader.lazyRequireGetter(this, "clipboardHelper", "devtools/shared/platform/clipboard");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class Font extends PureComponent {
  static get propTypes() {
    return {
      font: PropTypes.shape(Types.font).isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      isFontFaceRuleExpanded: false,
    };

    this.onFontFaceRuleToggle = this.onFontFaceRuleToggle.bind(this);
    this.onCopyURL = this.onCopyURL.bind(this);
  }

  componentWillReceiveProps(newProps) {
    if (this.props.font.name === newProps.font.name) {
      return;
    }

    this.setState({
      isFontFaceRuleExpanded: false,
    });
  }

  onCopyURL() {
    clipboardHelper.copyString(this.props.font.URI);
  }

  onFontFaceRuleToggle(event) {
    this.setState({
      isFontFaceRuleExpanded: !this.state.isFontFaceRuleExpanded
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
    let leading = ruleText.substring(0, ruleText.indexOf("{") + 1);
    let body = ruleText.substring(ruleText.indexOf("{") + 1, ruleText.lastIndexOf("}"));
    let trailing = ruleText.substring(ruleText.lastIndexOf("}"));

    let { isFontFaceRuleExpanded } = this.state;

    return dom.pre(
      {
        className: "font-css-code",
      },
      this.renderFontCSSCodeTwisty(),
      leading,
      isFontFaceRuleExpanded ?
        body :
        dom.span(
          {
            className: "font-css-code-expander",
            onClick: this.onFontFaceRuleToggle,
          }
        ),
      trailing
    );
  }

  renderFontOrigin(url) {
    if (!url) {
      return dom.p(
        {
          className: "font-origin system"
        },
        getStr("fontinspector.system")
      );
    }

    return dom.p(
      {
        className: "font-origin remote",
      },
      dom.span(
        {
          className: "url",
          title: url
        },
        url
      ),
      dom.button(
        {
          className: "copy-icon",
          onClick: this.onCopyURL,
          title: getStr("fontinspector.copyURL"),
        }
      )
    );
  }

  renderFontName(name) {
    return dom.h1(
      {
        className: "font-name"
      },
      name
    );
  }

  renderFontCSSCodeTwisty() {
    let { isFontFaceRuleExpanded } = this.state;

    let attributes = {
      className: "theme-twisty",
      onClick: this.onFontFaceRuleToggle,
    };
    if (isFontFaceRuleExpanded) {
      attributes.open = "true";
    }

    return dom.span(attributes);
  }

  render() {
    let {
      font,
      fontOptions,
      onPreviewFonts,
    } = this.props;

    let { previewText } = fontOptions;

    let {
      name,
      previewUrl,
      rule,
      ruleText,
      URI,
    } = font;

    return dom.li(
      {
        className: "font",
      },
      this.renderFontName(name),
      FontPreview({ previewText, previewUrl, onPreviewFonts }),
      this.renderFontOrigin(URI),
      this.renderFontCSSCode(rule, ruleText)
    );
  }
}

module.exports = Font;

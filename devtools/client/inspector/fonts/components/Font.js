/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const FontPreview = createFactory(require("./FontPreview"));

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
      isFontExpanded: false
    };

    this.onFontToggle = this.onFontToggle.bind(this);
  }

  componentWillReceiveProps(newProps) {
    if (this.props.font.name === newProps.font.name) {
      return;
    }

    this.setState({
      isFontExpanded: false
    });
  }

  onFontToggle() {
    this.setState({
      isFontExpanded: !this.state.isFontExpanded
    });
  }

  renderFontCSS(cssFamilyName) {
    return dom.p(
      {
        className: "font-css-name"
      },
      `${getStr("fontinspector.usedAs")} "${cssFamilyName}"`
    );
  }

  renderFontCSSCode(rule, ruleText) {
    if (!rule) {
      return null;
    }

    return dom.pre(
      {
        className: "font-css-code",
      },
      ruleText
    );
  }

  renderFontTypeAndURL(url, format) {
    if (!url) {
      return dom.p(
        {
          className: "font-format-url"
        },
        getStr("fontinspector.system")
      );
    }

    return dom.p(
      {
        className: "font-format-url"
      },
      getStr("fontinspector.remote"),
      dom.a(
        {
          className: "font-url",
          href: url
        },
        format
      )
    );
  }

  renderFontName(name) {
    return dom.h1(
      {
        className: "font-name",
        onClick: this.onFontToggle,
      },
      name
    );
  }

  renderTwisty() {
    let { isFontExpanded } = this.state;

    let attributes = {
      className: "theme-twisty",
      onClick: this.onFontToggle,
    };
    if (isFontExpanded) {
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
      CSSFamilyName,
      format,
      name,
      previewUrl,
      rule,
      ruleText,
      URI,
    } = font;

    let { isFontExpanded } = this.state;

    return dom.li(
      {
        className: "font" + (isFontExpanded ? " expanded" : ""),
      },
      this.renderTwisty(),
      this.renderFontName(name),
      FontPreview({ previewText, previewUrl, onPreviewFonts }),
      dom.div(
        {
          className: "font-details"
        },
        this.renderFontTypeAndURL(URI, format),
        this.renderFontCSSCode(rule, ruleText),
        this.renderFontCSS(CSSFamilyName)
      )
    );
  }
}

module.exports = Font;

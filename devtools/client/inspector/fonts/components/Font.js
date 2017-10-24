/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  DOM: dom,
  PropTypes,
  PureComponent,
} = require("devtools/client/shared/vendor/react");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class Font extends PureComponent {
  static get propTypes() {
    return PropTypes.shape(Types.font).isRequired;
  }

  constructor(props) {
    super(props);
    this.getSectionClasses = this.getSectionClasses.bind(this);
    this.renderFontCSS = this.renderFontCSS.bind(this);
    this.renderFontCSSCode = this.renderFontCSSCode.bind(this);
    this.renderFontFormatURL = this.renderFontFormatURL.bind(this);
    this.renderFontName = this.renderFontName.bind(this);
    this.renderFontPreview = this.renderFontPreview.bind(this);
  }

  getSectionClasses() {
    let { font } = this.props;

    let classes = ["font"];
    classes.push((font.URI) ? "is-remote" : "is-local");

    if (font.rule) {
      classes.push("has-code");
    }

    return classes.join(" ");
  }

  renderFontCSS(cssFamilyName) {
    return dom.p(
      {
        className: "font-css"
      },
      dom.span(
        {},
        getStr("fontinspector.usedAs")
      ),
      " \"",
      dom.span(
        {
          className: "font-css-name"
        },
        cssFamilyName
      ),
      "\""
    );
  }

  renderFontCSSCode(rule, ruleText) {
    return dom.pre(
      {
        className: "font-css-code"
      },
      rule ? ruleText : null
    );
  }

  renderFontFormatURL(url, format) {
    return dom.p(
      {
        className: "font-format-url"
      },
      dom.input(
        {
          className: "font-url",
          readOnly: "readonly",
          value: url
        }
      ),
      " ",
      format ?
        dom.span(
          {
            className: "font-format"
          },
          format
        )
        :
        dom.span(
          {
            className: "font-format",
            hidden: "true"
          },
          format
        )
    );
  }

  renderFontName(name) {
    return dom.h1(
      {
        className: "font-name",
      },
      name
    );
  }

  renderFontPreview(previewUrl) {
    return dom.div(
      {
        className: "font-preview-container",
      },
      dom.img(
        {
          className: "font-preview",
          src: previewUrl
        }
      )
    );
  }

  render() {
    let { font } = this.props;
    let {
      CSSFamilyName,
      format,
      name,
      previewUrl,
      rule,
      ruleText,
      URI,
    } = font;

    return dom.section(
      {
        className: this.getSectionClasses(),
      },
      this.renderFontPreview(previewUrl),
      dom.div(
        {
          className: "font-info",
        },
        this.renderFontName(name),
        dom.span(
          {
            className: "font-is-local",
          },
          " " + getStr("fontinspector.system")
        ),
        dom.span(
          {
            className: "font-is-remote",
          },
          " " + getStr("fontinspector.remote")
        ),
        this.renderFontFormatURL(URI, format),
        this.renderFontCSS(CSSFamilyName),
        this.renderFontCSSCode(rule, ruleText)
      )
    );
  }
}

module.exports = Font;

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class Font extends PureComponent {
  static get propTypes() {
    return PropTypes.shape(Types.font).isRequired;
  }

  constructor(props) {
    super(props);
    this.renderFontCSS = this.renderFontCSS.bind(this);
    this.renderFontCSSCode = this.renderFontCSSCode.bind(this);
    this.renderFontFormatURL = this.renderFontFormatURL.bind(this);
    this.renderFontName = this.renderFontName.bind(this);
    this.renderFontPreview = this.renderFontPreview.bind(this);
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

  renderFontCSSCode(ruleText) {
    return dom.pre(
      {
        className: "font-css-code"
      },
      ruleText
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

    return dom.li(
      {
        className: "font",
      },
      this.renderFontPreview(previewUrl),
      this.renderFontName(name),
      " " + (URI ? getStr("fontinspector.remote") : getStr("fontinspector.system")),
      URI ? this.renderFontFormatURL(URI, format) : null,
      this.renderFontCSS(CSSFamilyName),
      rule ? this.renderFontCSSCode(ruleText) : null
    );
  }
}

module.exports = Font;

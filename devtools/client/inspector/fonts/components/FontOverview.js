/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Accordion = createFactory(require("devtools/client/inspector/layout/components/Accordion"));
const FontList = createFactory(require("./FontList"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

class FontOverview extends PureComponent {
  static get propTypes() {
    return {
      fontData: PropTypes.shape(Types.fontData).isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  renderElementFonts() {
    let {
      fontData,
      fontOptions,
      onPreviewFonts,
      onToggleFontHighlight,
    } = this.props;
    let { fonts } = fontData;

    return fonts.length ?
      FontList({
        fonts,
        fontOptions,
        isCurrentElementFonts: true,
        onPreviewFonts,
        onToggleFontHighlight,
      })
      :
      dom.div(
        {
          className: "devtools-sidepanel-no-result"
        },
        getStr("fontinspector.noFontsOnSelectedElement")
      );
  }

  renderOtherFonts() {
    let {
      fontData,
      fontOptions,
      onPreviewFonts,
      onToggleFontHighlight,
    } = this.props;
    let { otherFonts } = fontData;

    if (!otherFonts.length) {
      return null;
    }

    return Accordion({
      items: [
        {
          header: getStr("fontinspector.otherFontsInPageHeader"),
          component: FontList,
          componentProps: {
            fontOptions,
            fonts: otherFonts,
            isCurrentElementFonts: false,
            onPreviewFonts,
            onToggleFontHighlight,
          },
          opened: false
        }
      ]
    });
  }

  render() {
    return dom.div(
      {
        id: "font-container",
      },
      this.renderElementFonts(),
      this.renderOtherFonts()
    );
  }
}

module.exports = FontOverview;

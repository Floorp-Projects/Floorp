/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");
const Services = require("Services");

const Accordion = createFactory(require("devtools/client/inspector/layout/components/Accordion"));
const FontList = createFactory(require("./FontList"));

const { getStr } = require("../utils/l10n");
const Types = require("../types");

const PREF_FONT_EDITOR = "devtools.inspector.fonteditor.enabled";

class FontOverview extends PureComponent {
  static get propTypes() {
    return {
      fontData: PropTypes.shape(Types.fontData).isRequired,
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);
    this.onToggleFontHighlightGlobal = (font, show) => {
      this.props.onToggleFontHighlight(font, show, false);
    };
  }

  renderElementFonts() {
    const {
      fontData,
      fontOptions,
      onPreviewFonts,
      onToggleFontHighlight,
    } = this.props;
    const { fonts } = fontData;

    // If the font editor is enabled, show the fonts in a collapsed accordion.
    // The editor already displays fonts, in another way, rendering twice is not desired.
    if (Services.prefs.getBoolPref(PREF_FONT_EDITOR)) {
      return fonts.length ? Accordion({
        items: [
          {
            header: getStr("fontinspector.renderedFontsInPageHeader"),
            component: FontList,
            componentProps: {
              fonts,
              fontOptions,
              onPreviewFonts,
              onToggleFontHighlight,
            },
            opened: false
          }
        ]
      })
      :
      null;
    }

    return fonts.length ?
      FontList({
        fonts,
        fontOptions,
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
    const {
      fontData,
      fontOptions,
      onPreviewFonts,
    } = this.props;
    const { otherFonts } = fontData;

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
            onPreviewFonts,
            onToggleFontHighlight: this.onToggleFontHighlightGlobal
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

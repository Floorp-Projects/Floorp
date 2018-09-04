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
      onPreviewTextChange: PropTypes.func.isRequired,
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
      onPreviewTextChange,
      onToggleFontHighlight,
    } = this.props;
    const { fonts } = fontData;

    return fonts.length ?
      FontList({
        fonts,
        fontOptions,
        onPreviewTextChange,
        onToggleFontHighlight,
      })
      :
      dom.div(
        {
          className: "devtools-sidepanel-no-result"
        },
        getStr("fontinspector.noFontsUsedOnCurrentElement")
      );
  }

  renderFonts() {
    const {
      fontData,
      fontOptions,
      onPreviewTextChange,
    } = this.props;

    const header = Services.prefs.getBoolPref(PREF_FONT_EDITOR)
      ? getStr("fontinspector.allFontsOnPageHeader")
      : getStr("fontinspector.otherFontsInPageHeader");

    const fonts = Services.prefs.getBoolPref(PREF_FONT_EDITOR)
      ? fontData.allFonts
      : fontData.otherFonts;

    if (!fonts.length) {
      return null;
    }

    return Accordion({
      items: [
        {
          header,
          component: FontList,
          componentProps: {
            fontOptions,
            fonts,
            onPreviewTextChange,
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
      // Render element fonts only when the Font Editor is not enabled.
      !Services.prefs.getBoolPref(PREF_FONT_EDITOR) && this.renderElementFonts(),
      this.renderFonts()
    );
  }
}

module.exports = FontOverview;

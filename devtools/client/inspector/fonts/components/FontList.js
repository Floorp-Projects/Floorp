/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const {
  createElement,
  createFactory,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Font = createFactory(require("./Font"));
const FontPreviewInput = createFactory(require("./FontPreviewInput"));

const Types = require("../types");

const PREF_FONT_EDITOR = "devtools.inspector.fonteditor.enabled";

class FontList extends PureComponent {
  static get propTypes() {
    return {
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired,
      onPreviewTextChange: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  render() {
    const {
      fonts,
      fontOptions,
      onPreviewTextChange,
      onToggleFontHighlight
    } = this.props;

    const { previewText } = fontOptions;

    const list = dom.ul(
      {
        className: "fonts-list"
      },
      fonts.map((font, i) => Font({
        key: i,
        font,
        fontOptions,
        onToggleFontHighlight,
      }))
    );

    // Show the font preview input only when the font editor is enabled.
    const previewInput = Services.prefs.getBoolPref(PREF_FONT_EDITOR) ?
      FontPreviewInput({
        onPreviewTextChange,
        previewText,
      })
      :
      null;

    return createElement(Fragment, null, previewInput, list);
  }
}

module.exports = FontList;

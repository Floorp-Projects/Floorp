/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { createFactory, PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Font = createFactory(require("./Font"));

const Types = require("../types");

class FontList extends PureComponent {
  static get propTypes() {
    return {
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  render() {
    let {
      fonts,
      fontOptions,
      onPreviewFonts,
      onToggleFontHighlight
    } = this.props;

    return dom.ul(
      {
        className: "fonts-list"
      },
      fonts.map((font, i) => Font({
        key: i,
        font,
        fontOptions,
        onPreviewFonts,
        onToggleFontHighlight,
      }))
    );
  }
}

module.exports = FontList;

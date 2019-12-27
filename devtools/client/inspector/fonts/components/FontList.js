/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {
  createElement,
  createFactory,
  createRef,
  Fragment,
  PureComponent,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Font = createFactory(require("./Font"));
const FontPreviewInput = createFactory(require("./FontPreviewInput"));

const Types = require("../types");

class FontList extends PureComponent {
  static get propTypes() {
    return {
      fontOptions: PropTypes.shape(Types.fontOptions).isRequired,
      fonts: PropTypes.arrayOf(PropTypes.shape(Types.font)).isRequired,
      onPreviewTextChange: PropTypes.func.isRequired,
      onToggleFontHighlight: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.onPreviewClick = this.onPreviewClick.bind(this);
    this.previewInputRef = createRef();
  }

  /**
   * Handler for clicks on the font preview image.
   * Requests the FontPreviewInput component, if one exists, to focus its input field.
   */
  onPreviewClick() {
    this.previewInputRef.current && this.previewInputRef.current.focus();
  }

  render() {
    const {
      fonts,
      fontOptions,
      onPreviewTextChange,
      onToggleFontHighlight,
    } = this.props;

    const { previewText } = fontOptions;
    const { onPreviewClick } = this;

    const list = dom.ul(
      {
        className: "fonts-list",
      },
      fonts.map((font, i) =>
        Font({
          key: i,
          font,
          onPreviewClick,
          onToggleFontHighlight,
        })
      )
    );

    const previewInput = FontPreviewInput({
      ref: this.previewInputRef,
      onPreviewTextChange,
      previewText,
    });

    return createElement(Fragment, null, previewInput, list);
  }
}

module.exports = FontList;

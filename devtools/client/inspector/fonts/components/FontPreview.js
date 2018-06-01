/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { PureComponent } = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");
const PropTypes = require("devtools/client/shared/vendor/react-prop-types");

const Types = require("../types");
const { getStr } = require("../utils/l10n");

class FontPreview extends PureComponent {
  static get propTypes() {
    return {
      previewText: Types.fontOptions.previewText.isRequired,
      previewUrl: Types.font.previewUrl.isRequired,
      onPreviewFonts: PropTypes.func.isRequired,
    };
  }

  constructor(props) {
    super(props);

    this.state = {
      // Is the text preview input field currently focused?
      isFocused: false,
    };

    this.onBlur = this.onBlur.bind(this);
    this.onClick = this.onClick.bind(this);
    this.onChange = this.onChange.bind(this);
  }

  componentDidUpdate() {
    if (this.state.isFocused) {
      const input = this.fontPreviewInput;
      input.focus();
      input.selectionStart = input.selectionEnd = input.value.length;
    }
  }

  onBlur() {
    this.setState({ isFocused: false });
  }

  onClick(event) {
    this.setState({ isFocused: true });
    event.stopPropagation();
  }

  onChange(event) {
    this.props.onPreviewFonts(event.target.value);
  }

  render() {
    const {
      previewText,
      previewUrl,
    } = this.props;

    const { isFocused } = this.state;

    return dom.div(
      {
        className: "font-preview-container",
      },
      isFocused ?
        dom.input(
          {
            type: "search",
            className: "font-preview-input devtools-searchinput",
            value: previewText,
            onBlur: this.onBlur,
            onChange: this.onChange,
            ref: input => {
              this.fontPreviewInput = input;
            }
          }
        )
        :
        null,
      dom.img(
        {
          className: "font-preview",
          src: previewUrl,
          onClick: this.onClick,
          title: !isFocused ? getStr("fontinspector.editPreview") : "",
        }
      )
    );
  }
}

module.exports = FontPreview;
